// Professional Logger Example with Multi-threaded Testing
// Demonstrates tag-level filtering, multiple backends, and memory efficiency

#include <Arduino.h>
#include "Logger.h"
#include "LoggerConfig.h"
#include "SynchronizedConsoleBackend.h"
#include "LoggingMacros.h"
#include "LogInterfaceImpl.cpp"  // Include implementation once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Task configuration
#define NUM_WORKER_TASKS 10
#define TASK_STACK_SIZE 2048
#define LOG_STRESS_DURATION_MS 10000

// Define log tags
const char* LOG_TAG_MAIN = "Main";
const char* LOG_TAG_WORKER = "Worker";
const char* LOG_TAG_MONITOR = "Monitor";
const char* LOG_TAG_TEST = "Test";

// Test control
static volatile bool g_runStressTest = false;
static SemaphoreHandle_t g_testCompleteSem = nullptr;
static uint32_t g_totalLogsGenerated = 0;
static uint32_t g_taskLogCounts[NUM_WORKER_TASKS] = {0};

// Worker task that generates logs
void workerTask(void* pvParameter) {
    int taskId = (int)pvParameter;
    char tagBuffer[16];
    snprintf(tagBuffer, sizeof(tagBuffer), "Worker%d", taskId);
    
    LOG_INFO(LOG_TAG_WORKER, "Task %d started on core %d", taskId, xPortGetCoreID());
    
    while (g_runStressTest) {
        // Generate different log levels
        LOG_DEBUG(tagBuffer, "Debug message %lu from task %d", g_taskLogCounts[taskId], taskId);
        LOG_INFO(tagBuffer, "Processing item %lu", g_taskLogCounts[taskId]);
        
        if (g_taskLogCounts[taskId] % 10 == 0) {
            LOG_WARN(tagBuffer, "Milestone reached: %lu items processed", g_taskLogCounts[taskId]);
        }
        
        if (g_taskLogCounts[taskId] % 100 == 0) {
            LOG_ERROR(tagBuffer, "Simulated error at count %lu", g_taskLogCounts[taskId]);
        }
        
        g_taskLogCounts[taskId]++;
        xAtomicAdd(&g_totalLogsGenerated, 4);  // We generated 4 logs
        
        // Small delay to prevent overwhelming
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    LOG_INFO(LOG_TAG_WORKER, "Task %d stopping. Generated %lu logs", taskId, g_taskLogCounts[taskId] * 4);
    xSemaphoreGive(g_testCompleteSem);
    vTaskDelete(NULL);
}

// Monitor task that reports memory usage
void monitorTask(void* pvParameter) {
    const TickType_t reportInterval = pdMS_TO_TICKS(1000);
    
    while (g_runStressTest) {
        size_t freeHeap = ESP.getFreeHeap();
        size_t minFreeHeap = ESP.getMinFreeHeap();
        UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
        
        LOG_INFO(LOG_TAG_MONITOR, "Memory - Heap: %u/%u bytes, Stack: %u words free", 
                 freeHeap, minFreeHeap, stackFree);
        
        // Report logger statistics
        Logger& logger = Logger::getInstance();
        LOG_INFO(LOG_TAG_MONITOR, "Logger - Dropped: %lu logs, Total generated: %lu", 
                 logger.getDroppedLogs(), g_totalLogsGenerated);
        
        vTaskDelay(reportInterval);
    }
    
    vTaskDelete(NULL);
}

void setupProfessionalLogger() {
    Serial.println("\r\n=== Professional Logger Configuration ===\r\n");
    
    // Create configuration
    LoggerConfig config = LoggerConfig::createDevelopment();
    
    // Add tag-specific configurations
    config.addTagConfig("Worker0", ESP_LOG_DEBUG);   // Verbose for first worker
    config.addTagConfig("Worker1", ESP_LOG_INFO);    // Normal for second
    config.addTagConfig("Worker2", ESP_LOG_WARN);    // Only warnings for third
    config.addTagConfig("Worker3", ESP_LOG_ERROR);   // Only errors for fourth
    config.addTagConfig("Worker*", ESP_LOG_INFO);    // Default for other workers
    config.addTagConfig("Monitor", ESP_LOG_INFO);
    config.addTagConfig("Test", ESP_LOG_DEBUG);
    
    // Apply configuration
    Logger& logger = Logger::getInstance();
    logger.configure(config);
    
    Serial.println("Logger configured with:");
    Serial.printf("- Default level: %s\r\n", Logger::levelToString(config.defaultLevel));
    Serial.printf("- Backend: SynchronizedConsole\r\n");
    Serial.printf("- Buffer pool: %d x %d bytes\r\n", 
                  LoggerConfig::BUFFER_COUNT, LoggerConfig::BUFFER_SIZE);
    Serial.printf("- Estimated memory: %u bytes\r\n", LoggerConfig::estimatedMemoryUsage());
    Serial.println("\r\n");
}

void runTagFilteringDemo() {
    Serial.println("=== Tag Filtering Demo ===\r\n");
    
    LOG_DEBUG(LOG_TAG_TEST, "This debug message should appear");
    LOG_INFO(LOG_TAG_TEST, "This info message should appear");
    
    // Test filtered workers
    LOG_DEBUG("Worker0", "Worker0 debug - should appear");
    LOG_DEBUG("Worker1", "Worker1 debug - should NOT appear (INFO level)");
    LOG_DEBUG("Worker2", "Worker2 debug - should NOT appear (WARN level)");
    LOG_WARN("Worker2", "Worker2 warning - should appear");
    LOG_ERROR("Worker3", "Worker3 error - should appear");
    LOG_WARN("Worker3", "Worker3 warning - should NOT appear (ERROR level)");
    
    Serial.println("\r\n");
}

void runMultiThreadStressTest() {
    Serial.println("=== Multi-Thread Stress Test ===");
    Serial.printf("Creating %d worker tasks...\r\n\r\n", NUM_WORKER_TASKS);
    
    // Create completion semaphore
    g_testCompleteSem = xSemaphoreCreateCounting(NUM_WORKER_TASKS, 0);
    
    // Reset counters
    g_totalLogsGenerated = 0;
    memset(g_taskLogCounts, 0, sizeof(g_taskLogCounts));
    
    // Start stress test
    g_runStressTest = true;
    
    // Create monitor task
    xTaskCreatePinnedToCore(monitorTask, "Monitor", TASK_STACK_SIZE, 
                           NULL, 2, NULL, 0);
    
    // Create worker tasks
    for (int i = 0; i < NUM_WORKER_TASKS; i++) {
        char taskName[16];
        snprintf(taskName, sizeof(taskName), "Worker%d", i);
        
        BaseType_t core = (i % 2);  // Alternate between cores
        xTaskCreatePinnedToCore(workerTask, taskName, TASK_STACK_SIZE, 
                               (void*)i, 1, NULL, core);
        vTaskDelay(pdMS_TO_TICKS(50));  // Stagger task creation
    }
    
    // Run test for specified duration
    Serial.printf("Running stress test for %d seconds...\r\n\r\n", LOG_STRESS_DURATION_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(LOG_STRESS_DURATION_MS));
    
    // Stop test
    Serial.println("\r\nStopping stress test...");
    g_runStressTest = false;
    
    // Wait for all tasks to complete
    for (int i = 0; i < NUM_WORKER_TASKS; i++) {
        xSemaphoreTake(g_testCompleteSem, portMAX_DELAY);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));  // Let monitor task finish
    
    // Report results
    Serial.println("\r\n=== Stress Test Results ===");
    Serial.printf("Total logs generated: %lu\r\n", g_totalLogsGenerated);
    Serial.printf("Logs dropped: %lu\r\n", Logger::getInstance().getDroppedLogs());
    Serial.printf("Drop rate: %.2f%%\r\n", 
                  (float)Logger::getInstance().getDroppedLogs() * 100.0f / g_totalLogsGenerated);
    
    for (int i = 0; i < NUM_WORKER_TASKS; i++) {
        Serial.printf("Worker%d: %lu logs\r\n", i, g_taskLogCounts[i] * 4);
    }
    
    // Memory report
    Serial.println("\r\nFinal Memory Status:");
    Serial.printf("Free heap: %u bytes\r\n", ESP.getFreeHeap());
    Serial.printf("Min free heap: %u bytes\r\n", ESP.getMinFreeHeap());
    Serial.printf("Largest free block: %u bytes\r\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    
    vSemaphoreDelete(g_testCompleteSem);
}

void runBackendDemo() {
    Serial.println("\r\n=== Multiple Backend Demo ===\r\n");
    
    Logger& logger = Logger::getInstance();
    
    // Add a second backend (for demonstration)
    auto secondBackend = std::make_shared<ConsoleBackend>();
    logger.addBackend(secondBackend);
    
    LOG_INFO(LOG_TAG_TEST, "This message goes to both backends");
    
    // Remove the second backend
    logger.removeBackend(secondBackend);
    
    LOG_INFO(LOG_TAG_TEST, "This message goes to primary backend only");
    
    Serial.println("\r\n");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    Serial.println("\r\n\r\n==================================");
    Serial.println("Professional Logger Example");
    Serial.println("==================================\r\n");
    
    // Setup professional logger
    setupProfessionalLogger();
    
    // Run demonstrations
    runTagFilteringDemo();
    runBackendDemo();
    
    // Wait before stress test
    Serial.println("Starting stress test in 3 seconds...\r\n");
    delay(3000);
    
    runMultiThreadStressTest();
    
    Serial.println("\r\n=== All Tests Complete ===\r\n");
}

void loop() {
    static uint32_t lastReport = 0;
    uint32_t now = millis();
    
    // Periodic keep-alive
    if (now - lastReport > 5000) {
        lastReport = now;
        LOG_INFO(LOG_TAG_MAIN, "System running for %lu seconds", now / 1000);
        
        // Also test direct logging
        Logger::getInstance().logDirect(ESP_LOG_INFO, LOG_TAG_MAIN, 
                                       "Direct log bypasses rate limiting");
    }
    
    delay(100);
}