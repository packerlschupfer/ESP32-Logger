#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logger.h"
#include "ConsoleBackend.h"
#include "LogInterface.h"

// Test configuration
#define TEST_BAUD_RATE 115200
#define FLOOD_MESSAGE_COUNT 5000  // Increased to stress test more
#define MULTI_TASK_COUNT 5
#define TEST_MESSAGE "Lorem ipsum dolor sit amet, consectetur adipiscing elit. This is a test message to fill the buffer quickly!"
#define AGGRESSIVE_FLOOD_TEST  // Enable more aggressive flooding

// Test tags
const char* TAG_TEST = "TEST";
const char* TAG_FLOOD = "FLOOD";
const char* TAG_TASK = "TASK";

// Global test results
struct TestResults {
    // Test 1: Direct Serial
    uint32_t directSerialBlockingMs = 0;
    uint32_t directSerialFillTimeMs = 0;
    
    // Test 2: Logger
    uint32_t loggerBlockingMs = 0;
    uint32_t loggerFillTimeMs = 0;
    
    // Test 3: Flooding
    uint32_t maxGapMs = 0;
    uint32_t floodDurationMs = 0;
    bool tasksBlocked = false;
    
    // Test 4: Multi-task
    uint32_t watchdogTimeouts = 0;
    uint32_t maxTaskBlockingMs = 0;
    
    // General
    uint32_t initialBufferSize = 0;
    uint32_t minBufferSeen = 0;
} results;

// Task handles
TaskHandle_t monitorTaskHandle = NULL;
TaskHandle_t floodTaskHandle = NULL;
TaskHandle_t multiTaskHandles[MULTI_TASK_COUNT];

// Control flags
volatile bool stopFlooding = false;
volatile bool monitorActive = false;
volatile uint32_t lastLogTime = 0;
volatile uint32_t maxGapDetected = 0;

// Helper function to print with proper line endings
void testPrint(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
    Serial.print("\r\n");
}

// Monitor task to detect gaps in logging
void monitorTask(void* param) {
    uint32_t lastCheck = millis();
    
    while (monitorActive) {
        uint32_t now = millis();
        
        // Check for gaps
        if (lastLogTime > 0) {
            uint32_t gap = now - lastLogTime;
            if (gap > maxGapDetected) {
                maxGapDetected = gap;
                if (gap > 1000) {  // Report gaps > 1 second
                    testPrint("!!! GAP DETECTED: %lu ms", gap);
                }
            }
        }
        
        // Monitor serial buffer
        size_t available = Serial.availableForWrite();
        if (available < results.minBufferSeen) {
            results.minBufferSeen = available;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    vTaskDelete(NULL);
}

// Test 1: Direct Serial Blocking Test
void testDirectSerialBlocking() {
    testPrint("\r\n=== Test 1: Direct Serial Blocking ===");
    
    // Get initial buffer size
    results.initialBufferSize = Serial.availableForWrite();
    results.minBufferSeen = results.initialBufferSize;
    testPrint("Initial buffer available: %lu bytes", results.initialBufferSize);
    
    // Fill the buffer
    uint32_t startFill = millis();
    int messagesSent = 0;
    
    // Send messages until buffer is nearly full
    while (Serial.availableForWrite() > 10) {
        Serial.print(TEST_MESSAGE);
        Serial.print("\r\n");
        messagesSent++;
    }
    
    results.directSerialFillTimeMs = millis() - startFill;
    testPrint("Buffer filled in %lu ms with %d messages", results.directSerialFillTimeMs, messagesSent);
    testPrint("Buffer now has %d bytes available", Serial.availableForWrite());
    
    // Now measure blocking time for one more write
    uint32_t startBlock = millis();
    Serial.print("This write should block...\r\n");
    Serial.flush();  // Force blocking
    results.directSerialBlockingMs = millis() - startBlock;
    
    testPrint("Blocking duration: %lu ms", results.directSerialBlockingMs);
    
    // Let buffer drain
    delay(2000);
}

// Test 2: Logger Blocking Test
void testLoggerBlocking() {
    testPrint("\r\n=== Test 2: Logger Blocking ===");
    
    // Initialize logger
    Logger& logger = Logger::getInstance();
    logger.setBackend(std::make_shared<ConsoleBackend>());
    logger.enableLogging(true);
    logger.setLogLevel(ESP_LOG_VERBOSE);
    logger.setMaxLogsPerSecond(0);  // Unlimited
    
    // Get initial buffer size
    size_t initialAvailable = Serial.availableForWrite();
    testPrint("Initial buffer available: %d bytes", initialAvailable);
    
    // Fill the buffer using Logger
    uint32_t startFill = millis();
    int messagesSent = 0;
    
    while (Serial.availableForWrite() > 10) {
        LOG_INFO(TAG_TEST, TEST_MESSAGE);
        messagesSent++;
    }
    
    results.loggerFillTimeMs = millis() - startFill;
    testPrint("Buffer filled in %lu ms with %d messages", results.loggerFillTimeMs, messagesSent);
    testPrint("Buffer now has %d bytes available", Serial.availableForWrite());
    
    // Measure blocking time for one more log
    uint32_t startBlock = millis();
    LOG_INFO(TAG_TEST, "This log should block...");
    logger.flush();  // Force flush
    results.loggerBlockingMs = millis() - startBlock;
    
    testPrint("Logger blocking duration: %lu ms", results.loggerBlockingMs);
    testPrint("Logger overhead vs Serial: %ld ms", 
              (int32_t)results.loggerBlockingMs - (int32_t)results.directSerialBlockingMs);
    
    // Let buffer drain
    delay(2000);
}

// Flood task for Test 3
void floodTask(void* param) {
    Logger& logger = Logger::getInstance();
    
    for (int i = 0; i < FLOOD_MESSAGE_COUNT && !stopFlooding; i++) {
        LOG_INFO(TAG_FLOOD, "Flood message %d: %s", i, TEST_MESSAGE);
        lastLogTime = millis();
        
#ifdef AGGRESSIVE_FLOOD_TEST
        // No delays - flood as fast as possible to reproduce the issue
        // This simulates what RYN4 library does
#else
        // Small delay every 10 messages to allow other tasks to run
        if (i % 10 == 0) {
            vTaskDelay(1);
        }
#endif
    }
    
    vTaskDelete(NULL);
}

// Test 3: Library Flooding Simulation
void testLibraryFlooding() {
    testPrint("\r\n=== Test 3: Library Flooding Simulation ===");
    testPrint("Simulating library (like RYN4) flooding the logger...");
    
    // Reset monitoring
    maxGapDetected = 0;
    lastLogTime = millis();
    stopFlooding = false;
    monitorActive = true;
    
    // Start monitor task
    xTaskCreate(monitorTask, "Monitor", 4096, NULL, tskIDLE_PRIORITY + 2, &monitorTaskHandle);
    
    // Record start time
    uint32_t startFlood = millis();
    
    // Start flood task
    xTaskCreate(floodTask, "Flood", 4096, NULL, tskIDLE_PRIORITY + 1, &floodTaskHandle);
    
    // Try to do work in main task during flooding
    uint32_t mainTaskBlocked = 0;
    uint32_t lastMainWork = millis();
    
    for (int i = 0; i < 50; i++) {
        uint32_t workStart = millis();
        
        // Try to print something
        testPrint("Main task attempt %d at %lu ms", i, workStart);
        
        uint32_t workDuration = millis() - workStart;
        if (workDuration > 100) {  // Blocked for > 100ms
            mainTaskBlocked++;
            results.tasksBlocked = true;
        }
        
        delay(100);  // Try to work every 100ms
    }
    
    // Stop flooding
    stopFlooding = true;
    monitorActive = false;
    delay(100);  // Let tasks finish
    
    results.floodDurationMs = millis() - startFlood;
    results.maxGapMs = maxGapDetected;
    
    testPrint("Flood test complete:");
    testPrint("  Duration: %lu ms", results.floodDurationMs);
    testPrint("  Max gap detected: %lu ms", results.maxGapMs);
    testPrint("  Main task blocked %lu times", mainTaskBlocked);
    testPrint("  Tasks blocked: %s", results.tasksBlocked ? "YES" : "NO");
    testPrint("  Min buffer seen: %lu bytes", results.minBufferSeen);
    
    delay(2000);  // Let buffer drain
}

// Worker task for Test 4
void workerTask(void* param) {
    int taskId = (int)param;
    char taskTag[16];
    snprintf(taskTag, sizeof(taskTag), "TASK%d", taskId);
    
    Logger& logger = Logger::getInstance();
    uint32_t maxBlockTime = 0;
    
    for (int i = 0; i < 100; i++) {
        uint32_t logStart = millis();
        LOG_INFO(taskTag, "Message %d from task %d", i, taskId);
        uint32_t logTime = millis() - logStart;
        
        if (logTime > maxBlockTime) {
            maxBlockTime = logTime;
        }
        
        if (logTime > 1000) {  // Blocked for > 1 second
            results.watchdogTimeouts++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // 20 logs/second per task
    }
    
    // Update global max if needed
    if (maxBlockTime > results.maxTaskBlockingMs) {
        results.maxTaskBlockingMs = maxBlockTime;
    }
    
    vTaskDelete(NULL);
}

// Test 4: Multi-Task Blocking Test
void testMultiTaskBlocking() {
    testPrint("\r\n=== Test 4: Multi-Task Blocking Test ===");
    testPrint("Creating %d tasks all logging simultaneously...", MULTI_TASK_COUNT);
    
    // Reset counters
    results.watchdogTimeouts = 0;
    results.maxTaskBlockingMs = 0;
    
    // Create worker tasks
    for (int i = 0; i < MULTI_TASK_COUNT; i++) {
        xTaskCreate(workerTask, "Worker", 4096, (void*)i, 
                   tskIDLE_PRIORITY + 1, &multiTaskHandles[i]);
    }
    
    testPrint("Tasks created, running test...");
    
    // Wait for tasks to complete (100 messages * 50ms = 5 seconds per task)
    delay(6000);
    
    testPrint("Multi-task test complete:");
    testPrint("  Tasks created: %d", MULTI_TASK_COUNT);
    testPrint("  Watchdog timeouts: %lu", results.watchdogTimeouts);
    testPrint("  Max blocking time: %lu ms", results.maxTaskBlockingMs);
}

// Print final results summary
// Test 5: Reproduce 10+ second blocking
void testSevereBlocking() {
    testPrint("\r\n=== Test 5: Attempting to Reproduce 10+ Second Blocking ===");
    testPrint("Simulating RYN4-style initialization flood...");
    
    // Fill buffer completely first
    while (Serial.availableForWrite() > 0) {
        Serial.print("X");
    }
    testPrint("Buffer filled, availableForWrite = %d", Serial.availableForWrite());
    
    // Now try to log with full buffer
    uint32_t blockStart = millis();
    
    // This should block severely
    for (int i = 0; i < 100; i++) {
        LOG_INFO("RYN4_INIT", "Initializing module component %d with extremely verbose debug information that fills the buffer", i);
        
        uint32_t elapsed = millis() - blockStart;
        if (elapsed > 1000) {
            testPrint("!!! SEVERE BLOCKING DETECTED: %lu ms after %d messages", elapsed, i);
            break;
        }
    }
    
    uint32_t totalBlockTime = millis() - blockStart;
    testPrint("Total blocking time: %lu ms", totalBlockTime);
    
    if (totalBlockTime > 5000) {
        testPrint("CRITICAL: Reproduced the 10+ second blocking issue!");
        results.maxGapMs = totalBlockTime;
    }
    
    // Let buffer drain
    delay(2000);
}

void printSummary() {
    testPrint("\r\n=== BLOCKING TEST RESULTS SUMMARY ===");
    
    testPrint("\r\nTest 1: Direct Serial Blocking");
    testPrint("  Buffer fill time: %lu ms", results.directSerialFillTimeMs);
    testPrint("  Blocking duration: %lu ms", results.directSerialBlockingMs);
    
    testPrint("\r\nTest 2: Logger Blocking");
    testPrint("  Buffer fill time: %lu ms", results.loggerFillTimeMs);
    testPrint("  Blocking duration: %lu ms", results.loggerBlockingMs);
    testPrint("  Additional Logger overhead: %ld ms", 
              (int32_t)results.loggerBlockingMs - (int32_t)results.directSerialBlockingMs);
    
    testPrint("\r\nTest 3: Library Flooding");
    testPrint("  Flood duration: %lu ms", results.floodDurationMs);
    testPrint("  Max gap detected: %lu ms", results.maxGapMs);
    testPrint("  Tasks blocked: %s", results.tasksBlocked ? "YES" : "NO");
    
    testPrint("\r\nTest 4: Multi-Task");
    testPrint("  Tasks created: %d", MULTI_TASK_COUNT);
    testPrint("  Watchdog timeouts: %lu", results.watchdogTimeouts);
    testPrint("  Max blocking time: %lu ms", results.maxTaskBlockingMs);
    
    testPrint("\r\nBuffer Statistics:");
    testPrint("  Initial size: %lu bytes", results.initialBufferSize);
    testPrint("  Minimum seen: %lu bytes", results.minBufferSeen);
    
    // Analysis
    testPrint("\r\nCONCLUSION:");
    
    bool serialBlocks = results.directSerialBlockingMs > 100;
    bool loggerAddsOverhead = results.loggerBlockingMs > results.directSerialBlockingMs + 50;
    bool severeGaps = results.maxGapMs > 5000;
    
    if (serialBlocks && !loggerAddsOverhead) {
        testPrint("  Blocking source is primarily ESP32 Serial hardware");
        testPrint("  Both Serial and Logger block when buffer is full");
    } else if (loggerAddsOverhead) {
        testPrint("  Logger adds significant overhead to blocking");
        testPrint("  Issue is in both Serial hardware AND Logger implementation");
    }
    
    if (severeGaps) {
        testPrint("  CRITICAL: Detected gaps > 5 seconds, confirming production issue");
    }
    
    testPrint("\r\nRECOMMENDATION:");
    testPrint("  Implement non-blocking serial backend with:");
    testPrint("  - Serial.availableForWrite() checks");
    testPrint("  - Message dropping when buffer full");
    testPrint("  - Per-tag rate limiting");
    testPrint("  - Never use Serial.flush()");
}

void setup() {
    Serial.begin(TEST_BAUD_RATE);
    delay(2000);  // Wait for serial monitor
    
    testPrint("\r\n========================================");
    testPrint("Logger Blocking Issue Verification Test");
    testPrint("========================================");
    testPrint("This test will verify the source of blocking");
    testPrint("Baud rate: %d", TEST_BAUD_RATE);
    testPrint("Starting tests in 3 seconds...\r\n");
    
    delay(3000);
}

void loop() {
    static bool testsComplete = false;
    
    if (!testsComplete) {
        // Run all tests
        testDirectSerialBlocking();
        testLoggerBlocking();
        testLibraryFlooding();
        testMultiTaskBlocking();
        testSevereBlocking();  // New test to reproduce 10+ second issue
        
        // Print summary
        printSummary();
        
        testsComplete = true;
        testPrint("\r\nAll tests complete!");
    }
    
    delay(1000);
}