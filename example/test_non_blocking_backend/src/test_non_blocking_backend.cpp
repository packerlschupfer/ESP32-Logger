#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logger.h"
#include "NonBlockingConsoleBackend.h"
#include "ConsoleBackend.h"
#include "LogInterface.h"
#include <memory>

// Test configuration
const char* TAG_TEST = "TEST";
const int NUM_TESTS = 10;
const uint32_t TEST_BAUD_RATE = 921600;  // High baud rate for testing

// Test results tracking
struct TestResult {
    const char* name;
    bool passed;
    const char* message;
};

TestResult testResults[NUM_TESTS];
int currentTest = 0;

// Helper function for test output
void testPrint(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
    Serial.print("\r\n");
    Serial.flush();  // Ensure test output is visible
}

// Record test result
void recordTest(const char* testName, bool passed, const char* message = "") {
    if (currentTest < NUM_TESTS) {
        testResults[currentTest].name = testName;
        testResults[currentTest].passed = passed;
        testResults[currentTest].message = message;
        currentTest++;
    }
}

// Test 1: Basic non-blocking write
bool testBasicWrite() {
    testPrint("\n=== Test 1: Basic Non-Blocking Write ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    
    // Write a simple message
    uint32_t startTime = millis();
    LOG_INFO(TAG_TEST, "This is a test message");
    uint32_t elapsed = millis() - startTime;
    
    testPrint("Write time: %lu ms", elapsed);
    bool passed = elapsed < 5;  // Should be nearly instant
    
    recordTest("Basic Write", passed, passed ? "Write was non-blocking" : "Write took too long");
    return passed;
}

// Test 2: Buffer full behavior
bool testBufferFullBehavior() {
    testPrint("\n=== Test 2: Buffer Full Behavior ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    
    // Reset stats first
    backend->resetStats();
    
    // Fill the serial buffer completely and keep it full
    // by writing a large amount of data quickly
    const char* fillData = "XXXXXXXXXX";  // 10 bytes at a time
    for (int i = 0; i < 50; i++) {
        Serial.print(fillData);
    }
    
    // Immediately try to log multiple messages
    // Some should be dropped due to full buffer
    uint32_t startTime = millis();
    for (int i = 0; i < 10; i++) {
        LOG_INFO(TAG_TEST, "Message %d - This should be dropped because buffer is full", i);
    }
    uint32_t elapsed = millis() - startTime;
    
    testPrint("Write time for 10 messages: %lu ms", elapsed);
    testPrint("Buffer available after writes: %d bytes", Serial.availableForWrite());
    testPrint("Dropped messages: %lu", backend->getDroppedMessages());
    
    // Pass if we didn't block AND we dropped at least some messages
    bool passed = elapsed < 50 && backend->getDroppedMessages() > 0;
    recordTest("Buffer Full", passed, 
               passed ? "Messages dropped without blocking" : "Either blocked or didn't drop");
    
    // Let buffer clear
    delay(1000);
    return passed;
}

// Test 3: Statistics tracking
bool testStatisticsTracking() {
    testPrint("\n=== Test 3: Statistics Tracking ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    backend->resetStats();
    
    // Generate some normal writes
    for (int i = 0; i < 5; i++) {
        LOG_INFO(TAG_TEST, "Normal message %d", i);
        delay(10);  // Let buffer drain
    }
    
    uint32_t normalDropped = backend->getDroppedMessages();
    testPrint("Normal operation - Dropped: %lu", normalDropped);
    
    // Now flood to cause drops
    while (Serial.availableForWrite() > 20) {
        Serial.print("Y");
    }
    
    for (int i = 0; i < 10; i++) {
        LOG_INFO(TAG_TEST, "Flood message %d - This should be dropped", i);
    }
    
    uint32_t totalDropped = backend->getDroppedMessages();
    uint32_t droppedBytes = backend->getDroppedBytes();
    
    testPrint("After flood - Dropped messages: %lu", totalDropped);
    testPrint("Dropped bytes: %lu", droppedBytes);
    
    bool passed = totalDropped > normalDropped && droppedBytes > 0;
    recordTest("Statistics", passed, 
               passed ? "Stats tracked correctly" : "Stats not updating");
    
    delay(1000);
    return passed;
}

// Test 4: Partial write with truncation
bool testPartialWrite() {
    testPrint("\n=== Test 4: Partial Write with Truncation ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    backend->resetStats();
    
    // Fill buffer leaving some space
    while (Serial.availableForWrite() > 50) {
        Serial.print("Z");
    }
    
    testPrint("Buffer partially filled, available: %d bytes", Serial.availableForWrite());
    
    // Write a long message that won't fit completely
    LOG_INFO(TAG_TEST, "This is a very long message that should be truncated when the buffer doesn't have enough space to hold the entire message");
    
    uint32_t partialWrites = backend->getPartialWrites();
    testPrint("Partial writes: %lu", partialWrites);
    
    // Note: We can't easily verify the truncation marker was written,
    // but we can check that partial writes were tracked
    bool passed = partialWrites > 0 || backend->getDroppedMessages() > 0;
    recordTest("Partial Write", passed, 
               passed ? "Handled partial writes" : "No partial write detected");
    
    delay(1000);
    return passed;
}

// Test 5: Buffer critical detection
bool testBufferCritical() {
    testPrint("\n=== Test 5: Buffer Critical Detection ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    
    // Test with plenty of space
    delay(1000);  // Let buffer clear
    bool notCritical = !backend->isBufferCritical();
    testPrint("With empty buffer - Critical: %s", notCritical ? "NO" : "YES");
    
    // Fill buffer
    while (Serial.availableForWrite() > 15) {
        Serial.print("W");
    }
    
    bool isCritical = backend->isBufferCritical();
    testPrint("With full buffer - Critical: %s", isCritical ? "YES" : "NO");
    testPrint("Available: %d bytes", Serial.availableForWrite());
    
    bool passed = notCritical && isCritical;
    recordTest("Critical Detection", passed, 
               passed ? "Correctly detects critical state" : "Detection failed");
    
    delay(1000);
    return passed;
}

// Test 6: Reset statistics
bool testResetStats() {
    testPrint("\n=== Test 6: Reset Statistics ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    
    // Generate some drops
    while (Serial.availableForWrite() > 10) {
        Serial.print("R");
    }
    
    for (int i = 0; i < 5; i++) {
        LOG_INFO(TAG_TEST, "Message to be dropped");
    }
    
    uint32_t droppedBefore = backend->getDroppedMessages();
    testPrint("Dropped before reset: %lu", droppedBefore);
    
    // Reset
    backend->resetStats();
    
    uint32_t droppedAfter = backend->getDroppedMessages();
    uint32_t bytesAfter = backend->getDroppedBytes();
    uint32_t partialAfter = backend->getPartialWrites();
    
    testPrint("After reset - Messages: %lu, Bytes: %lu, Partial: %lu", 
              droppedAfter, bytesAfter, partialAfter);
    
    bool passed = droppedBefore > 0 && droppedAfter == 0 && bytesAfter == 0 && partialAfter == 0;
    recordTest("Reset Stats", passed, 
               passed ? "Stats reset correctly" : "Reset failed");
    
    delay(1000);
    return passed;
}

// Test 7: Empty message handling
bool testEmptyMessage() {
    testPrint("\n=== Test 7: Empty Message Handling ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    backend->resetStats();
    
    // Test null pointer
    backend->write(nullptr, 0);
    backend->write(nullptr, 100);
    
    // Test empty string
    backend->write("", 0);
    backend->write(std::string(""));
    
    uint32_t dropped = backend->getDroppedMessages();
    testPrint("Dropped after empty messages: %lu", dropped);
    
    bool passed = dropped == 0;  // Empty messages shouldn't count as dropped
    recordTest("Empty Message", passed, 
               passed ? "Empty messages handled correctly" : "Incorrectly counted drops");
    
    return passed;
}

// Test 8: Performance under load
bool testPerformanceUnderLoad() {
    testPrint("\n=== Test 8: Performance Under Load ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    backend->resetStats();
    
    const int MESSAGE_COUNT = 1000;
    uint32_t startTime = millis();
    uint32_t maxWriteTime = 0;
    
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        uint32_t writeStart = micros();
        LOG_INFO(TAG_TEST, "Performance test message %d", i);
        uint32_t writeTime = micros() - writeStart;
        
        if (writeTime > maxWriteTime) {
            maxWriteTime = writeTime;
        }
    }
    
    uint32_t totalTime = millis() - startTime;
    uint32_t dropped = backend->getDroppedMessages();
    
    testPrint("Sent %d messages in %lu ms", MESSAGE_COUNT, totalTime);
    testPrint("Max single write time: %lu us", maxWriteTime);
    testPrint("Dropped: %lu (%.1f%%)", dropped, (float)dropped * 100.0 / MESSAGE_COUNT);
    
    // Should never block for more than 1ms per write
    bool passed = maxWriteTime < 1000;
    recordTest("Performance", passed, 
               passed ? "No blocking detected" : "Blocking detected");
    
    return passed;
}

// Test 9: Multi-threaded safety
TaskHandle_t writerTask1 = NULL;
TaskHandle_t writerTask2 = NULL;
volatile bool threadTestDone = false;
std::shared_ptr<NonBlockingConsoleBackend> threadTestBackend;

void writerTaskFunction(void* param) {
    int taskId = (int)param;
    Logger& logger = Logger::getInstance();
    
    for (int i = 0; i < 100; i++) {
        LOG_INFO("Thread", "Task %d message %d", taskId, i);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    vTaskDelete(NULL);
}

bool testMultiThreaded() {
    testPrint("\n=== Test 9: Multi-threaded Safety ===");
    
    threadTestBackend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(threadTestBackend);
    threadTestBackend->resetStats();
    
    // Create two tasks that write simultaneously
    xTaskCreate(writerTaskFunction, "Writer1", 4096, (void*)1, tskIDLE_PRIORITY + 1, &writerTask1);
    xTaskCreate(writerTaskFunction, "Writer2", 4096, (void*)2, tskIDLE_PRIORITY + 1, &writerTask2);
    
    // Wait for tasks to complete
    delay(2000);
    
    // Check results
    uint32_t dropped = threadTestBackend->getDroppedMessages();
    testPrint("Multi-threaded test - Dropped: %lu", dropped);
    
    // The test passes if we didn't crash
    bool passed = true;
    recordTest("Multi-threaded", passed, "No crashes with concurrent writes");
    
    return passed;
}

// Test 10: Print stats functionality
bool testPrintStats() {
    testPrint("\n=== Test 10: Print Stats Functionality ===");
    
    auto backend = std::make_shared<NonBlockingConsoleBackend>();
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    
    // Generate some activity
    for (int i = 0; i < 10; i++) {
        LOG_INFO(TAG_TEST, "Stats test message %d", i);
    }
    
    // Fill buffer and cause drops
    while (Serial.availableForWrite() > 10) {
        Serial.print("S");
    }
    for (int i = 0; i < 5; i++) {
        LOG_INFO(TAG_TEST, "This should be dropped");
    }
    
    delay(100);
    
    testPrint("Calling printStats():");
    backend->printStats();
    
    // Test passes if printStats doesn't crash
    bool passed = true;
    recordTest("Print Stats", passed, "Stats printed without crash");
    
    return passed;
}

// Print test summary
void printSummary() {
    testPrint("\n\n========================================");
    testPrint("      TEST SUMMARY");
    testPrint("========================================");
    
    int passed = 0;
    int failed = 0;
    
    for (int i = 0; i < currentTest; i++) {
        const char* status = testResults[i].passed ? "PASS" : "FAIL";
        testPrint("%-20s: %s - %s", testResults[i].name, status, testResults[i].message);
        
        if (testResults[i].passed) {
            passed++;
        } else {
            failed++;
        }
    }
    
    testPrint("\n----------------------------------------");
    testPrint("Total Tests: %d", currentTest);
    testPrint("Passed: %d", passed);
    testPrint("Failed: %d", failed);
    testPrint("Success Rate: %.1f%%", (float)passed * 100.0 / currentTest);
    testPrint("========================================");
    
    if (failed == 0) {
        testPrint("\nALL TESTS PASSED! ✓");
        testPrint("NonBlockingConsoleBackend is working correctly.");
    } else {
        testPrint("\nSOME TESTS FAILED! ✗");
        testPrint("Please check the implementation.");
    }
}

void setup() {
    Serial.begin(921600);  // High baud rate for better test accuracy
    delay(2000);
    
    testPrint("\n========================================");
    testPrint("  NonBlockingConsoleBackend Test Suite");
    testPrint("========================================");
    testPrint("This test verifies all aspects of the");
    testPrint("non-blocking backend implementation.\n");
    
    // Configure logger for testing
    Logger& logger = Logger::getInstance();
    logger.enableLogging(true);
    logger.setLogLevel(ESP_LOG_VERBOSE);
    logger.setMaxLogsPerSecond(0);  // Unlimited
    
    delay(1000);
}

void loop() {
    static bool testsRun = false;
    
    if (!testsRun) {
        // Run all tests
        testBasicWrite();
        testBufferFullBehavior();
        testStatisticsTracking();
        testPartialWrite();
        testBufferCritical();
        testResetStats();
        testEmptyMessage();
        testPerformanceUnderLoad();
        testMultiThreaded();
        testPrintStats();
        
        // Print summary
        printSummary();
        
        testsRun = true;
    }
    
    delay(1000);
}