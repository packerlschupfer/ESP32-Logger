#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logger.h"

// Test configuration
#define NUM_TEST_TASKS 4
#define LOGS_PER_TASK 100
#define TEST_DURATION_MS 10000
#define METRICS_INTERVAL_MS 2000

const char* STRESS_TAG = "StressTest";

// Task handles
TaskHandle_t testTasks[NUM_TEST_TASKS];
TaskHandle_t metricsTask;

// Shared test data
volatile bool testRunning = true;
Logger& logger = getLogger();

// Test task that generates logs at high frequency
void stressTestTask(void* pvParameters) {
    int taskId = (int)pvParameters;
    char taskTag[32];
    snprintf(taskTag, sizeof(taskTag), "Task%d", taskId);
    
    // Mix of different log levels and message sizes
    const char* shortMsg = "Short log message";
    const char* mediumMsg = "This is a medium length log message that tests the normal buffer allocation path";
    const char* longMsg = "This is a very long log message that is designed to test the memory pool "
                         "allocation system and ensure that the logger can handle messages that exceed "
                         "the normal stack buffer size without causing heap fragmentation or other issues "
                         "in a multi-threaded environment with high frequency logging";
    
    uint32_t logCount = 0;
    unsigned long startTime = millis();
    
    while (testRunning) {
        // Vary log types to stress different code paths
        switch (logCount % 10) {
            case 0:
                // Direct mode test
                logger.setDirectMode(true);
                logger.log(ESP_LOG_INFO, taskTag, "Direct mode: %s", shortMsg);
                logger.setDirectMode(false);
                break;
                
            case 1:
                // Tag-specific level test
                logger.setTagLevel(taskTag, ESP_LOG_DEBUG);
                logger.log(ESP_LOG_DEBUG, taskTag, "Debug level: Task %d, count %lu", taskId, logCount);
                logger.clearTagLevel(taskTag);
                break;
                
            case 2:
                // Long message test (triggers pool allocation)
                logger.log(ESP_LOG_WARN, taskTag, "Long message test: %s (count: %lu)", longMsg, logCount);
                break;
                
            case 3:
                // Rapid fire test
                for (int i = 0; i < 5; i++) {
                    logger.log(ESP_LOG_INFO, taskTag, "Rapid %d", i);
                }
                break;
                
            case 4:
                // Context test
                std::unordered_map<std::string, std::string> ctx = {
                    {"task", std::to_string(taskId)},
                    {"count", std::to_string(logCount)}
                };
                logger.setContext(ctx);
                logger.log(ESP_LOG_INFO, taskTag, "Context test: %s", mediumMsg);
                logger.setContext({});  // Clear context
                break;
                
            case 5:
                // Error level test
                logger.log(ESP_LOG_ERROR, taskTag, "Error simulation: Task %d encountered issue at %lu ms", 
                          taskId, millis() - startTime);
                break;
                
            case 6:
                // Variable args test
                logger.log(ESP_LOG_INFO, taskTag, "Multi-arg test: int=%d, float=%.2f, str=%s, hex=0x%08X", 
                          taskId, (float)logCount / 10.0, shortMsg, logCount);
                break;
                
            case 7:
                // Callback simulation
                logger.enterCallback();
                logger.log(ESP_LOG_INFO, taskTag, "Callback log from task %d", taskId);
                logger.exitCallback();
                break;
                
            case 8:
                // Medium message test
                logger.log(ESP_LOG_INFO, taskTag, "%s (task: %d, iteration: %lu)", mediumMsg, taskId, logCount);
                break;
                
            case 9:
                // Verbose level test
                logger.log(ESP_LOG_VERBOSE, taskTag, "Verbose details: heap=%d, stack=%d", 
                          ESP.getFreeHeap(), uxTaskGetStackHighWaterMark(NULL));
                break;
        }
        
        logCount++;
        
        // Add some randomness to timing
        vTaskDelay(pdMS_TO_TICKS(random(10, 50)));
    }
    
    logger.log(ESP_LOG_INFO, taskTag, "Task %d completed. Total logs: %lu", taskId, logCount);
    vTaskDelete(NULL);
}

// Task that monitors and reports metrics
void metricsReportTask(void* pvParameters) {
    unsigned long lastReport = 0;
    
    while (testRunning) {
        unsigned long now = millis();
        if (now - lastReport >= METRICS_INTERVAL_MS) {
            Logger::PerformanceMetrics metrics = logger.getMetrics();
            
            logger.log(ESP_LOG_INFO, STRESS_TAG, 
                      "=== Performance Metrics ===\n"
                      "  Total logs: %lu\n"
                      "  Dropped logs: %lu (%.2f%%)\n"
                      "  Pool allocations: %lu\n"
                      "  Heap allocations: %lu\n"
                      "  Avg log time: %lu us\n"
                      "  Min log time: %lu us\n"
                      "  Max log time: %lu us\n"
                      "  Failed mutex: %lu\n"
                      "  Free heap: %d bytes\n"
                      "===========================",
                      metrics.totalLogs,
                      metrics.droppedLogs,
                      metrics.totalLogs > 0 ? (float)metrics.droppedLogs * 100.0 / metrics.totalLogs : 0.0,
                      metrics.poolAllocations,
                      metrics.heapAllocations,
                      metrics.avgLogTimeUs,
                      metrics.minLogTimeUs == UINT32_MAX ? 0 : metrics.minLogTimeUs,
                      metrics.maxLogTimeUs,
                      logger.getFailedMutexAcquisitions(),
                      ESP.getFreeHeap());
            
            lastReport = now;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Logger Stress Test Starting ===");
    
    // Initialize logger with various configurations
    logger.init(256);
    logger.enableLogging(true);
    logger.setLogLevel(ESP_LOG_VERBOSE);
    logger.setMaxLogsPerSecond(200);  // Allow high frequency for stress test
    logger.setMutexWait(pdMS_TO_TICKS(20));  // Slightly longer wait for stress conditions
    
    // Reset metrics before test
    logger.resetMetrics();
    logger.resetFailedMutexAcquisitions();
    
    Serial.println("Logger initialized. Starting stress test tasks...");
    
    // Create stress test tasks with different priorities
    for (int i = 0; i < NUM_TEST_TASKS; i++) {
        char taskName[32];
        snprintf(taskName, sizeof(taskName), "StressTask%d", i);
        
        BaseType_t result = xTaskCreate(
            stressTestTask,
            taskName,
            4096,  // Stack size
            (void*)i,  // Task ID as parameter
            tskIDLE_PRIORITY + 1 + (i % 3),  // Vary priorities
            &testTasks[i]
        );
        
        if (result != pdPASS) {
            Serial.printf("Failed to create task %d\n", i);
        }
    }
    
    // Create metrics reporting task
    xTaskCreate(
        metricsReportTask,
        "MetricsTask",
        4096,
        NULL,
        tskIDLE_PRIORITY + 2,
        &metricsTask
    );
    
    Serial.printf("Created %d stress test tasks. Test will run for %d seconds.\n", 
                  NUM_TEST_TASKS, TEST_DURATION_MS / 1000);
}

void loop() {
    static unsigned long testStartTime = millis();
    
    // Run test for specified duration
    if (millis() - testStartTime < TEST_DURATION_MS) {
        // Main loop can do other work here
        delay(1000);
    } else if (testRunning) {
        // Stop the test
        testRunning = false;
        
        Serial.println("\n=== Stopping stress test ===");
        
        // Wait for tasks to complete
        delay(1000);
        
        // Print final metrics
        Logger::PerformanceMetrics finalMetrics = logger.getMetrics();
        
        Serial.println("\n=== FINAL STRESS TEST RESULTS ===");
        Serial.printf("Total logs processed: %lu\n", finalMetrics.totalLogs);
        Serial.printf("Logs dropped (rate limited): %lu (%.2f%%)\n", 
                     finalMetrics.droppedLogs,
                     finalMetrics.totalLogs > 0 ? (float)finalMetrics.droppedLogs * 100.0 / finalMetrics.totalLogs : 0.0);
        Serial.printf("Memory pool allocations: %lu\n", finalMetrics.poolAllocations);
        Serial.printf("Heap allocations: %lu\n", finalMetrics.heapAllocations);
        Serial.printf("Average log processing time: %lu microseconds\n", finalMetrics.avgLogTimeUs);
        Serial.printf("Minimum log processing time: %lu microseconds\n", 
                     finalMetrics.minLogTimeUs == UINT32_MAX ? 0 : finalMetrics.minLogTimeUs);
        Serial.printf("Maximum log processing time: %lu microseconds\n", finalMetrics.maxLogTimeUs);
        Serial.printf("Failed mutex acquisitions: %lu\n", logger.getFailedMutexAcquisitions());
        Serial.printf("Logs per second: %.2f\n", 
                     (float)finalMetrics.totalLogs / (TEST_DURATION_MS / 1000.0));
        Serial.printf("Final free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.println("=================================\n");
        
        // Performance analysis
        if (finalMetrics.droppedLogs > finalMetrics.totalLogs * 0.05) {
            Serial.println("WARNING: More than 5% of logs were dropped. Consider:");
            Serial.println("  - Increasing maxLogsPerSecond");
            Serial.println("  - Reducing log frequency");
            Serial.println("  - Using direct mode for critical logs");
        }
        
        if (finalMetrics.maxLogTimeUs > 1000) {
            Serial.println("WARNING: Some logs took over 1ms to process. Consider:");
            Serial.println("  - Reducing message length");
            Serial.println("  - Using direct mode");
            Serial.println("  - Checking for mutex contention");
        }
        
        if (finalMetrics.heapAllocations > finalMetrics.poolAllocations) {
            Serial.println("NOTE: More heap allocations than pool allocations.");
            Serial.println("  - Consider increasing pool size for better performance");
        }
        
        Serial.println("\nStress test complete!");
    }
    
    delay(1000);
}