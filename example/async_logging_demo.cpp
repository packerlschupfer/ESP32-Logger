#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logger.h"
#include "AsyncLogger.h"
#include "CircularBufferBackend.h"
#include "ConsoleBackend.h"

// Demo configuration
#define NUM_PRODUCER_TASKS 3
#define CIRCULAR_BUFFER_SIZE 50
#define ASYNC_QUEUE_SIZE 30

const char* DEMO_TAG = "AsyncDemo";

// Global objects
Logger& logger = getLogger();
std::shared_ptr<CircularBufferBackend> circularBackend;
AsyncLogger* asyncLogger = nullptr;

// Task handles
TaskHandle_t producerTasks[NUM_PRODUCER_TASKS];
TaskHandle_t monitorTask;

// Demo control
volatile bool demoRunning = true;

/**
 * Producer task that generates logs at different rates
 */
void producerTask(void* pvParameters) {
    int taskId = (int)pvParameters;
    char taskTag[32];
    snprintf(taskTag, sizeof(taskTag), "Producer%d", taskId);
    
    // Different behaviors for each producer
    const int delays[] = {50, 100, 200};  // Different rates
    const esp_log_level_t levels[] = {ESP_LOG_INFO, ESP_LOG_DEBUG, ESP_LOG_WARN};
    
    int messageCount = 0;
    
    while (demoRunning) {
        // Generate log message
        asyncLogger->log(levels[taskId % 3], taskTag, 
                        "Message %d from task %d - Free heap: %d, Stack: %d", 
                        messageCount++, taskId, 
                        ESP.getFreeHeap(), 
                        uxTaskGetStackHighWaterMark(NULL));
        
        // Task-specific delay
        vTaskDelay(pdMS_TO_TICKS(delays[taskId % 3]));
        
        // Occasionally generate a burst of logs
        if (messageCount % 20 == 0) {
            for (int i = 0; i < 5; i++) {
                asyncLogger->log(ESP_LOG_DEBUG, taskTag, 
                                "Burst message %d/%d", i + 1, 5);
            }
        }
    }
    
    vTaskDelete(NULL);
}

/**
 * Monitor task that reports statistics
 */
void monitorTaskFunction(void* pvParameters) {
    unsigned long lastReport = 0;
    const unsigned long REPORT_INTERVAL = 3000;  // 3 seconds
    
    while (demoRunning) {
        unsigned long now = millis();
        
        if (now - lastReport >= REPORT_INTERVAL) {
            // Get async queue statistics
            AsyncLogger::QueueStats queueStats = asyncLogger->getStats();
            
            // Get logger performance metrics
            Logger::PerformanceMetrics logMetrics = logger.getMetrics();
            
            Serial.println("\n=== Async Logger Statistics ===");
            Serial.printf("Queue Status:\n");
            Serial.printf("  Current depth: %lu/%d\n", 
                         queueStats.currentQueueDepth, ASYNC_QUEUE_SIZE);
            Serial.printf("  Max depth reached: %lu\n", queueStats.maxQueueDepth);
            Serial.printf("  Total queued: %lu\n", queueStats.totalQueued);
            Serial.printf("  Total processed: %lu\n", queueStats.totalProcessed);
            Serial.printf("  Total dropped: %lu (%.2f%%)\n", 
                         queueStats.totalDropped,
                         queueStats.totalQueued > 0 ? 
                         (float)queueStats.totalDropped * 100.0 / queueStats.totalQueued : 0.0);
            Serial.printf("  Avg processing time: %lu us\n", 
                         queueStats.avgProcessingTimeUs);
            Serial.printf("  Max processing time: %lu us\n", 
                         queueStats.maxProcessingTimeUs);
            
            Serial.println("\nLogger Performance:");
            Serial.printf("  Total logs: %lu\n", logMetrics.totalLogs);
            Serial.printf("  Rate limited drops: %lu\n", logMetrics.droppedLogs);
            Serial.printf("  Pool allocations: %lu\n", logMetrics.poolAllocations);
            Serial.printf("  Heap allocations: %lu\n", logMetrics.heapAllocations);
            Serial.printf("  Avg log time: %lu us\n", logMetrics.avgLogTimeUs);
            
            Serial.println("\nCircular Buffer Status:");
            Serial.printf("  Messages stored: %zu/%d\n", 
                         circularBackend->getLogCount(), CIRCULAR_BUFFER_SIZE);
            Serial.printf("  Buffer full: %s\n", 
                         circularBackend->isFull() ? "YES" : "NO");
            
            uint32_t writes, reads;
            circularBackend->getStats(writes, reads);
            Serial.printf("  Total writes: %lu, reads: %lu\n", writes, reads);
            
            Serial.println("==============================\n");
            
            lastReport = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

/**
 * Demonstrate different overflow strategies
 */
void demonstrateOverflowStrategies() {
    Serial.println("\n=== Demonstrating Overflow Strategies ===");
    
    // Save current strategy
    AsyncLogger::OverflowStrategy originalStrategy = asyncLogger->getOverflowStrategy();
    
    // Test DROP_NEWEST (default)
    Serial.println("\n1. Testing DROP_NEWEST strategy:");
    asyncLogger->setOverflowStrategy(AsyncLogger::OverflowStrategy::DROP_NEWEST);
    asyncLogger->resetStats();
    
    // Generate burst to overflow queue
    for (int i = 0; i < ASYNC_QUEUE_SIZE + 10; i++) {
        asyncLogger->log(ESP_LOG_INFO, "Overflow", "DROP_NEWEST test message %d", i);
    }
    
    delay(500);  // Let worker process
    AsyncLogger::QueueStats stats = asyncLogger->getStats();
    Serial.printf("   Dropped: %lu messages\n", stats.totalDropped);
    
    // Test DROP_OLDEST
    Serial.println("\n2. Testing DROP_OLDEST strategy:");
    asyncLogger->setOverflowStrategy(AsyncLogger::OverflowStrategy::DROP_OLDEST);
    asyncLogger->resetStats();
    
    for (int i = 0; i < ASYNC_QUEUE_SIZE + 10; i++) {
        asyncLogger->log(ESP_LOG_INFO, "Overflow", "DROP_OLDEST test message %d", i);
    }
    
    delay(500);
    stats = asyncLogger->getStats();
    Serial.printf("   Dropped: %lu messages (oldest removed)\n", stats.totalDropped);
    
    // Restore original strategy
    asyncLogger->setOverflowStrategy(originalStrategy);
    Serial.println("\n=== Overflow Strategy Demo Complete ===\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Async Logger Demo Starting ===");
    
    // Initialize logger with circular buffer backend
    circularBackend = std::make_shared<CircularBufferBackend>(CIRCULAR_BUFFER_SIZE);
    logger = Logger(circularBackend);
    logger.init(256);
    logger.enableLogging(true);
    logger.setLogLevel(ESP_LOG_VERBOSE);
    logger.setMaxLogsPerSecond(100);  // Allow reasonable rate for demo
    
    Serial.println("Base logger initialized with CircularBufferBackend");
    
    // Create async logger wrapper
    asyncLogger = new AsyncLogger(logger, ASYNC_QUEUE_SIZE, 
                                 AsyncLogger::OverflowStrategy::DROP_NEWEST,
                                 tskIDLE_PRIORITY + 2);
    
    if (!asyncLogger->start()) {
        Serial.println("ERROR: Failed to start async logger!");
        return;
    }
    
    Serial.println("Async logger started successfully");
    
    // Reset all metrics
    logger.resetMetrics();
    asyncLogger->resetStats();
    
    // Create producer tasks
    for (int i = 0; i < NUM_PRODUCER_TASKS; i++) {
        char taskName[32];
        snprintf(taskName, sizeof(taskName), "Producer%d", i);
        
        xTaskCreate(producerTask, taskName, 4096, (void*)i, 
                   tskIDLE_PRIORITY + 1, &producerTasks[i]);
    }
    
    Serial.printf("Created %d producer tasks\n", NUM_PRODUCER_TASKS);
    
    // Create monitor task
    xTaskCreate(monitorTaskFunction, "Monitor", 4096, NULL, 
               tskIDLE_PRIORITY + 1, &monitorTask);
    
    Serial.println("Monitor task created");
    
    // Demonstrate overflow strategies after a delay
    delay(5000);
    demonstrateOverflowStrategies();
    
    Serial.println("\nDemo is running. Logs are being stored in circular buffer.");
    Serial.println("Press 'd' to dump circular buffer contents");
    Serial.println("Press 'c' to clear circular buffer");
    Serial.println("Press 'r' to show recent logs");
    Serial.println("Press 's' to stop demo\n");
}

void loop() {
    static unsigned long demoStartTime = millis();
    
    // Check for user input
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'd':
            case 'D':
                Serial.println("\n=== Dumping Circular Buffer ===");
                circularBackend->dumpToSerial("AsyncDemo");
                break;
                
            case 'c':
            case 'C':
                circularBackend->clear();
                Serial.println("\nCircular buffer cleared!");
                break;
                
            case 'r':
            case 'R': {
                Serial.println("\n=== Recent Logs (last 10) ===");
                auto recentLogs = circularBackend->getRecentLogs(10);
                for (const auto& log : recentLogs) {
                    Serial.print(log.c_str());
                    if (log.back() != '\n') Serial.println();
                }
                Serial.println("=== End Recent Logs ===\n");
                break;
            }
                
            case 's':
            case 'S':
                Serial.println("\n=== Stopping Demo ===");
                demoRunning = false;
                break;
        }
        
        // Clear any remaining input
        while (Serial.available()) {
            Serial.read();
        }
    }
    
    // Stop demo after 60 seconds if not stopped manually
    if (demoRunning && millis() - demoStartTime > 60000) {
        Serial.println("\n=== Demo timeout reached, stopping ===");
        demoRunning = false;
    }
    
    if (!demoRunning) {
        // Shutdown sequence
        Serial.println("Waiting for tasks to complete...");
        delay(2000);
        
        // Flush async queue
        Serial.println("Flushing async queue...");
        asyncLogger->flush(2000);
        
        // Final statistics
        AsyncLogger::QueueStats finalQueueStats = asyncLogger->getStats();
        Logger::PerformanceMetrics finalLogMetrics = logger.getMetrics();
        
        Serial.println("\n=== FINAL STATISTICS ===");
        Serial.printf("Async Queue:\n");
        Serial.printf("  Total queued: %lu\n", finalQueueStats.totalQueued);
        Serial.printf("  Total processed: %lu\n", finalQueueStats.totalProcessed);
        Serial.printf("  Total dropped: %lu\n", finalQueueStats.totalDropped);
        Serial.printf("  Success rate: %.2f%%\n", 
                     finalQueueStats.totalQueued > 0 ?
                     (float)finalQueueStats.totalProcessed * 100.0 / finalQueueStats.totalQueued : 0.0);
        
        Serial.printf("\nLogger Performance:\n");
        Serial.printf("  Total logs: %lu\n", finalLogMetrics.totalLogs);
        Serial.printf("  Avg processing: %lu us\n", finalLogMetrics.avgLogTimeUs);
        
        Serial.printf("\nCircular Buffer:\n");
        Serial.printf("  Final count: %zu messages\n", circularBackend->getLogCount());
        
        // Cleanup
        asyncLogger->stop(true);
        delete asyncLogger;
        
        Serial.println("\nDemo complete!");
        
        // Prevent further execution
        while (true) {
            delay(1000);
        }
    }
    
    delay(100);
}