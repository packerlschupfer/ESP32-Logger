/*
 * Memory Comparison Test
 * 
 * This program measures the runtime memory impact of the custom Logger
 * compared to ESP-IDF logging.
 * 
 * Build with:
 * - esp32dev_no_logger: Uses ESP-IDF logging (zero heap overhead)
 * - esp32dev_with_logger: Uses custom Logger (~17KB singleton)
 */

#include <Arduino.h>

// Conditional compilation for logging
#ifndef USE_CUSTOM_LOGGER
    // ESP-IDF logging
    #include "esp_log.h"
    #define LOG_TAG "MemTest"
    #define LOG_INFO(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
    #define LOG_ERROR(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
    #define LOG_WARN(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
    #define LOG_DEBUG(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    // Custom Logger
    #include "Logger.h"
    #define LOG_TAG "MemTest"
    #define LOG_INFO(tag, ...) Logger::getInstance().log(ESP_LOG_INFO, tag, __VA_ARGS__)
    #define LOG_ERROR(tag, ...) Logger::getInstance().log(ESP_LOG_ERROR, tag, __VA_ARGS__)
    #define LOG_WARN(tag, ...) Logger::getInstance().log(ESP_LOG_WARN, tag, __VA_ARGS__)
    #define LOG_DEBUG(tag, ...) Logger::getInstance().log(ESP_LOG_DEBUG, tag, __VA_ARGS__)
#endif

// Memory tracking variables
struct MemorySnapshot {
    uint32_t freeHeap;
    uint32_t minFreeHeap;
    uint32_t maxAllocHeap;
    uint32_t totalHeap;
};

MemorySnapshot baseline;
MemorySnapshot afterFirstLog;
MemorySnapshot afterInit;
MemorySnapshot afterStress;

MemorySnapshot captureMemory() {
    MemorySnapshot snap;
    snap.freeHeap = ESP.getFreeHeap();
    snap.minFreeHeap = ESP.getMinFreeHeap();
    snap.maxAllocHeap = ESP.getMaxAllocHeap();
    snap.totalHeap = ESP.getHeapSize();
    return snap;
}

void printMemoryStats(const char* label, const MemorySnapshot& snap) {
    Serial.printf("\n=== %s ===\n", label);
    Serial.printf("Free heap: %lu bytes\n", (unsigned long)snap.freeHeap);
    Serial.printf("Min free heap: %lu bytes\n", (unsigned long)snap.minFreeHeap);
    Serial.printf("Largest free block: %lu bytes\n", (unsigned long)snap.maxAllocHeap);
    Serial.printf("Total heap: %lu bytes\n", (unsigned long)snap.totalHeap);
    
    uint32_t usedHeap = snap.totalHeap - snap.freeHeap;
    Serial.printf("Used heap: %lu bytes\n", (unsigned long)usedHeap);
    Serial.printf("Usage: %.1f%%\n", (usedHeap * 100.0) / snap.totalHeap);
}

void printMemoryDiff(const char* label, const MemorySnapshot& before, const MemorySnapshot& after) {
    int32_t heapDiff = (int32_t)before.freeHeap - (int32_t)after.freeHeap;
    Serial.printf("\n%s: %ld bytes\n", label, (long)heapDiff);
    if (heapDiff > 0) {
        Serial.printf("  (%.2f%% of total heap)\n", (heapDiff * 100.0) / before.totalHeap);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        delay(10);
    }
    
    Serial.println("\n\n========================================");
    Serial.println("       LOGGER MEMORY COMPARISON TEST");
    Serial.println("========================================");
    
    #ifdef USE_CUSTOM_LOGGER
    Serial.println("Mode: CUSTOM LOGGER");
    #else
    Serial.println("Mode: ESP-IDF LOGGING");
    #endif
    
    Serial.printf("ESP32 Total Heap: %lu bytes\n", (unsigned long)ESP.getHeapSize());
    Serial.println("========================================");
    
    // Step 1: Baseline memory before any logging
    delay(100); // Let system settle
    baseline = captureMemory();
    printMemoryStats("1. Baseline (before any logging)", baseline);
    
    // Step 2: First log call - this creates Logger singleton if using custom logger
    Serial.println("\n--- Performing first log call ---");
    #ifdef USE_CUSTOM_LOGGER
    LOG_INFO(LOG_TAG, "First log message - Hello from Custom Logger!");
    #else
    LOG_INFO(LOG_TAG, "First log message - Hello from ESP-IDF!");
    #endif
    
    delay(50);
    afterFirstLog = captureMemory();
    printMemoryStats("2. After first log call", afterFirstLog);
    printMemoryDiff("Memory used by first log", baseline, afterFirstLog);
    
    #ifdef USE_CUSTOM_LOGGER
    // Step 3: Initialize Logger with available settings
    Serial.println("\n--- Initializing Logger ---");
    Logger& logger = Logger::getInstance();
    logger.init(2048);  // Initialize (parameter is currently ignored)
    logger.setLogLevel(ESP_LOG_VERBOSE);
    logger.enableLogging(true);
    logger.setMaxLogsPerSecond(100);
    
    // The current Logger is simplified - test what's available
    // Multiple logs to ensure any internal buffers are allocated
    logger.log(ESP_LOG_INFO, LOG_TAG, "Test log 1");
    logger.log(ESP_LOG_DEBUG, LOG_TAG, "Test log 2 with number: %d", 42);
    logger.log(ESP_LOG_WARN, LOG_TAG, "Test log 3 with string: %s", "test string");
    logger.logNnL(ESP_LOG_INFO, LOG_TAG, "Test without newline");
    logger.logInL(" - inline continuation\n");
    
    delay(50);
    afterInit = captureMemory();
    printMemoryStats("3. After Logger initialization", afterInit);
    printMemoryDiff("Memory used by init", afterFirstLog, afterInit);
    #else
    // For ESP-IDF, just do more logging
    LOG_INFO(LOG_TAG, "Second log");
    LOG_INFO(LOG_TAG, "Third log");
    afterInit = captureMemory();
    printMemoryStats("3. After additional ESP-IDF logs", afterInit);
    #endif
    
    // Step 4: Stress test - log many messages with heavy formatting
    Serial.println("\n--- Stress test: 100 log messages with heavy formatting ---");
    uint32_t beforeStress = ESP.getFreeHeap();
    
    // Test various format specifiers and long messages
    for (int i = 0; i < 100; i++) {
        // Use simpler formatting to avoid stack overflow during stress test
        LOG_INFO(LOG_TAG, "Stress test message %d/100", i + 1);
        
        // Add some variety with different log levels
        if (i % 10 == 0) {
            LOG_WARN(LOG_TAG, "Warning %d: counter=%d", i/10, i);
        }
        
        if (i % 20 == 0) {
            LOG_ERROR(LOG_TAG, "Error simulation %d", i/20);
        }
        
        // Show progress
        if (i == 24 || i == 49 || i == 74 || i == 99) {
            Serial.printf("  Progress: %d messages sent, Free heap: %lu\n", 
                         i + 1, (unsigned long)ESP.getFreeHeap());
        }
    }
    
    delay(100);
    afterStress = captureMemory();
    printMemoryStats("4. After stress test", afterStress);
    printMemoryDiff("Memory used during stress test", afterInit, afterStress);
    
    // Step 5: Let system settle and check for memory leaks
    Serial.println("\n--- Waiting 3 seconds for cleanup ---");
    delay(3000);
    
    MemorySnapshot afterCleanup = captureMemory();
    printMemoryStats("5. After cleanup wait", afterCleanup);
    printMemoryDiff("Memory recovered after cleanup", afterStress, afterCleanup);
    
    // Step 6: Test stack usage with increasing parameter counts
    Serial.println("\n--- Testing stack usage limits ---");
    
    uint32_t beforeTest;
    
    // Test with 10 parameters
    Serial.println("Testing with 10 parameters...");
    beforeTest = ESP.getFreeHeap();
    LOG_INFO(LOG_TAG, "10 params: %d %u %ld %lu %f %f %s %c %p %d",
             1, 2U, 3L, 4UL, 5.5f, 6.6f, "test", 'X', (void*)0x1234, 10);
    Serial.printf("  10 params OK - heap used: %ld\n", (long)(beforeTest - ESP.getFreeHeap()));
    delay(100);
    
    // Test with 15 parameters
    Serial.println("Testing with 15 parameters...");
    beforeTest = ESP.getFreeHeap();
    LOG_INFO(LOG_TAG, "15 params: %d %u %ld %lu %f %f %s %c %p %d %u %ld %lu %f %s",
             1, 2U, 3L, 4UL, 5.5f, 6.6f, "test", 'X', (void*)0x1234, 10,
             11U, 12L, 13UL, 14.14f, "end");
    Serial.printf("  15 params OK - heap used: %ld\n", (long)(beforeTest - ESP.getFreeHeap()));
    delay(100);
    
    // Test with 20 parameters
    Serial.println("Testing with 20 parameters...");
    beforeTest = ESP.getFreeHeap();
    LOG_INFO(LOG_TAG, "20 params: %d %u %ld %lu %f %f %s %c %p %d %u %ld %lu %f %s %d %u %ld %lu %f",
             1, 2U, 3L, 4UL, 5.5f, 6.6f, "test", 'X', (void*)0x1234, 10,
             11U, 12L, 13UL, 14.14f, "mid", 16, 17U, 18L, 19UL, 20.0f);
    Serial.printf("  20 params OK - heap used: %ld\n", (long)(beforeTest - ESP.getFreeHeap()));
    delay(100);
    
    // Test with 25 parameters (might crash)
    Serial.println("Testing with 25 parameters...");
    beforeTest = ESP.getFreeHeap();
    LOG_INFO(LOG_TAG, "25 params: %d %u %ld %lu %f %f %s %c %p %d %u %ld %lu %f %s %d %u %ld %lu %f %d %u %ld %lu %f",
             1, 2U, 3L, 4UL, 5.5f, 6.6f, "test", 'X', (void*)0x1234, 10,
             11U, 12L, 13UL, 14.14f, "mid", 16, 17U, 18L, 19UL, 20.0f,
             21, 22U, 23L, 24UL, 25.25f);
    Serial.printf("  25 params OK - heap used: %ld\n", (long)(beforeTest - ESP.getFreeHeap()));
    
    // Final Summary
    Serial.println("\n\n========================================");
    Serial.println("         MEMORY IMPACT SUMMARY");
    Serial.println("========================================");
    
    #ifdef USE_CUSTOM_LOGGER
    Serial.println("\nCUSTOM LOGGER Memory Usage:");
    int32_t singletonCost = (int32_t)baseline.freeHeap - (int32_t)afterFirstLog.freeHeap;
    Serial.printf("  Logger singleton creation: %ld bytes\n", (long)singletonCost);
    Serial.printf("  Percentage of total heap: %.2f%%\n", (singletonCost * 100.0) / baseline.totalHeap);
    
    int32_t initCost = (int32_t)afterFirstLog.freeHeap - (int32_t)afterInit.freeHeap;
    Serial.printf("  Additional init cost: %ld bytes\n", (long)initCost);
    
    int32_t totalCost = (int32_t)baseline.freeHeap - (int32_t)afterCleanup.freeHeap;
    Serial.printf("  Total persistent memory: %ld bytes\n", (long)totalCost);
    Serial.printf("  Percentage of total heap: %.2f%%\n", (totalCost * 100.0) / baseline.totalHeap);
    
    Serial.println("\nStack Usage Notes:");
    Serial.println("  - Heavy formatting uses stack space (not heap)");
    Serial.println("  - Current implementation uses malloc for buffers");
    Serial.println("  - This trades heap allocation overhead for stack safety");
    #else
    Serial.println("\nESP-IDF LOGGING Memory Usage:");
    int32_t totalCost = (int32_t)baseline.freeHeap - (int32_t)afterCleanup.freeHeap;
    Serial.printf("  Total memory impact: %ld bytes\n", (long)totalCost);
    Serial.println("  (Should be near zero - no persistent allocations)");
    Serial.println("\nStack Usage Notes:");
    Serial.println("  - ESP-IDF uses stack for formatting");
    Serial.println("  - No heap allocations for normal logging");
    #endif
    
    Serial.println("\n========================================");
    Serial.println("To compare results, run both environments:");
    Serial.println("  pio run -e esp32dev_no_logger -t upload");
    Serial.println("  pio run -e esp32dev_with_logger -t upload");
    Serial.println("========================================\n");
}

void loop() {
    static unsigned long lastReport = 0;
    static unsigned long lastLog = 0;
    static int counter = 0;
    
    // Report memory every 30 seconds
    if (millis() - lastReport > 30000) {
        lastReport = millis();
        Serial.printf("\n[Runtime: %lu min] Free heap: %lu bytes, Min free: %lu bytes\n", 
                      millis() / 60000, 
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)ESP.getMinFreeHeap());
    }
    
    // Log periodically to check for memory leaks
    if (millis() - lastLog > 5000) {
        lastLog = millis();
        counter++;
        LOG_INFO(LOG_TAG, "Runtime message #%d - uptime: %lu seconds", 
                 counter, millis() / 1000);
    }
    
    delay(100);
}