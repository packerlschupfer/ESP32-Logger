#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logger.h"
#include "NonBlockingConsoleBackend.h"
#include "ConsoleBackend.h"
#include "LogInterface.h"

// Demo configuration
const char* TAG_DEMO = "Demo";
const char* TAG_FLOOD = "Flood";
const char* TAG_STATS = "Stats";

// Task handles
TaskHandle_t floodTaskHandle = NULL;
TaskHandle_t normalTaskHandle = NULL;
TaskHandle_t statsTaskHandle = NULL;

// Control flags
volatile bool stopDemo = false;

// Global logger instance
Logger& logger = Logger::getInstance();
NonBlockingConsoleBackend* nonBlockingBackend = nullptr;

void floodTask(void* param) {
    // This task floods the logger to test non-blocking behavior
    int messageCount = 0;
    
    while (!stopDemo) {
        // Flood with messages
        for (int i = 0; i < 100; i++) {
            LOG_INFO(TAG_FLOOD, "Flood message %d - This is a long message designed to fill the serial buffer quickly and test the non-blocking behavior of our new backend implementation", messageCount++);
        }
        
        // Brief pause
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

void normalTask(void* param) {
    // This task logs at a normal rate
    uint32_t counter = 0;
    
    while (!stopDemo) {
        LOG_INFO(TAG_DEMO, "Normal task message %lu - System should remain responsive", counter++);
        
        // Check responsiveness
        uint32_t start = millis();
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t elapsed = millis() - start;
        
        if (elapsed > 1100) {  // More than 100ms over expected
            LOG_WARN(TAG_DEMO, "Task delayed! Expected 1000ms, got %lums", elapsed);
        }
    }
    
    vTaskDelete(NULL);
}

void statsTask(void* param) {
    // This task reports statistics
    while (!stopDemo) {
        vTaskDelay(pdMS_TO_TICKS(5000));  // Report every 5 seconds
        
        if (nonBlockingBackend) {
            LOG_INFO(TAG_STATS, "=== Non-Blocking Backend Statistics ===");
            LOG_INFO(TAG_STATS, "Dropped messages: %lu", nonBlockingBackend->getDroppedMessages());
            LOG_INFO(TAG_STATS, "Dropped bytes: %lu", nonBlockingBackend->getDroppedBytes());
            LOG_INFO(TAG_STATS, "Partial writes: %lu", nonBlockingBackend->getPartialWrites());
            LOG_INFO(TAG_STATS, "Buffer available: %u bytes", Serial.availableForWrite());
            LOG_INFO(TAG_STATS, "Buffer critical: %s", nonBlockingBackend->isBufferCritical() ? "YES" : "NO");
            LOG_INFO(TAG_STATS, "=====================================");
        }
    }
    
    vTaskDelete(NULL);
}

void demonstrateBlockingVsNonBlocking() {
    Serial.println("\r\n=== Demonstrating Blocking vs Non-Blocking Behavior ===\r\n");
    
    // Test 1: Blocking backend
    Serial.println("Test 1: Using BLOCKING ConsoleBackend");
    logger.setBackend(std::make_shared<ConsoleBackend>());
    
    // Fill buffer
    while (Serial.availableForWrite() > 10) {
        Serial.print("X");
    }
    
    uint32_t blockStart = millis();
    LOG_INFO(TAG_DEMO, "This message with blocking backend...");
    uint32_t blockTime = millis() - blockStart;
    Serial.printf("\r\nBlocking time: %lu ms\r\n\r\n", blockTime);
    
    delay(2000);  // Let buffer clear
    
    // Test 2: Non-blocking backend
    Serial.println("Test 2: Using NON-BLOCKING NonBlockingConsoleBackend");
    auto nbBackend = std::make_shared<NonBlockingConsoleBackend>();
    logger.setBackend(nbBackend);
    nonBlockingBackend = nbBackend.get();
    
    // Fill buffer again
    while (Serial.availableForWrite() > 10) {
        Serial.print("Y");
    }
    
    uint32_t nonBlockStart = millis();
    LOG_INFO(TAG_DEMO, "This message with non-blocking backend...");
    uint32_t nonBlockTime = millis() - nonBlockStart;
    Serial.printf("\r\nNon-blocking time: %lu ms (message may be dropped)\r\n", nonBlockTime);
    
    // Show stats
    delay(100);
    nonBlockingBackend->printStats();
    
    Serial.println("\r\n=== Comparison ===");
    Serial.printf("Blocking backend: %lu ms\r\n", blockTime);
    Serial.printf("Non-blocking backend: %lu ms\r\n", nonBlockTime);
    Serial.printf("Improvement: %lu ms saved\r\n", blockTime - nonBlockTime);
    Serial.println("==================\r\n");
    
    delay(2000);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\r\n========================================");
    Serial.println("Non-Blocking Logger Backend Demo");
    Serial.println("========================================\r\n");
    
    // Configure logger with non-blocking backend
    LoggerConfig config = LoggerConfig::createDevelopment();
    config.primaryBackend = LoggerConfig::BackendType::NON_BLOCKING_CONSOLE;
    config.maxLogsPerSecond = 0;  // Unlimited to stress test
    logger.configure(config);
    
    // Store reference to backend for statistics
    // (In production, you might want to keep this reference when creating the backend)
    
    // Demonstrate the difference
    demonstrateBlockingVsNonBlocking();
    
    Serial.println("Starting stress test with multiple tasks...");
    Serial.println("- Flood task: Generates massive amounts of logs");
    Serial.println("- Normal task: Logs once per second");
    Serial.println("- Stats task: Reports statistics every 5 seconds");
    Serial.println("\nPress any key to stop the demo\n");
    
    // Create tasks
    xTaskCreate(floodTask, "Flood", 4096, NULL, tskIDLE_PRIORITY + 1, &floodTaskHandle);
    xTaskCreate(normalTask, "Normal", 4096, NULL, tskIDLE_PRIORITY + 2, &normalTaskHandle);
    xTaskCreate(statsTask, "Stats", 4096, NULL, tskIDLE_PRIORITY + 2, &statsTaskHandle);
}

void loop() {
    static uint32_t lastMainLoop = millis();
    
    // Check if main loop is responsive
    uint32_t now = millis();
    if (now - lastMainLoop > 2000) {
        Serial.println("\r\n[Main Loop] Still responsive!");
        lastMainLoop = now;
    }
    
    // Check for user input to stop
    if (Serial.available()) {
        Serial.read();  // Clear input
        stopDemo = true;
        
        Serial.println("\r\n=== Stopping Demo ===");
        delay(1000);  // Let tasks finish
        
        if (nonBlockingBackend) {
            Serial.println("\r\nFinal Statistics:");
            nonBlockingBackend->printStats();
        }
        
        Serial.println("\r\nDemo stopped. System remains responsive!");
        Serial.println("Notice how the system never froze despite the flood of logs.");
        
        while (true) {
            delay(1000);
        }
    }
    
    delay(100);
}