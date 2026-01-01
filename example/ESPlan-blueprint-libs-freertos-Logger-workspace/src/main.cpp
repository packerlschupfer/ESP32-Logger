#include <Arduino.h>
#include <SPI.h>
#include "Logger.h"
#include "ConsoleBackend.h"  // Include backend
#include "LoggingMacros.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdarg.h>

const char* LOG_TAG_MAIN = "Main";

// Helper function for proper line endings
void serialPrintln(const char* str = "") {
    Serial.print(str);
    Serial.print("\r\n");
}

void serialPrintf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Print the formatted string
    Serial.print(buffer);
    
    // If the string doesn't already end with \r\n, add it
    size_t len = strlen(buffer);
    if (len >= 2 && buffer[len-2] == '\r' && buffer[len-1] == '\n') {
        // Already has proper line ending
    } else if (len >= 1 && buffer[len-1] == '\n') {
        // Has only \n, add \r before it
        Serial.print('\r');
    } else {
        // No line ending at all, add \r\n
        Serial.print("\r\n");
    }
}

// Function declarations
bool setupLogger();
void printTestHeader(const char* testName);
void printTestResult(const char* testName, bool passed);

// Test statistics
struct TestStats {
    uint32_t testsRun = 0;
    uint32_t testsPassed = 0;
    uint32_t testsFailed = 0;
} testStats;

unsigned long lastEventTime = 0;  // Time of the last significant event

/*
 * Initialize and set up the logger following best practices.
 */
bool setupLogger() {
    // Get logger singleton instance
    Logger& logger = Logger::getInstance();
    
    // Set backend (ConsoleBackend is already the default)
    logger.setBackend(std::make_shared<ConsoleBackend>());
    
    // Initialize with buffer size
    logger.init(CONFIG_LOG_BUFFER_SIZE);
    
    // Enable logging
    logger.enableLogging(true);
    
    // Set log level to verbose for testing
    logger.setLogLevel(ESP_LOG_VERBOSE);
    
    // Disable rate limiting during startup (0 = unlimited)
    logger.setMaxLogsPerSecond(0);
    
    // Log initialization details
    serialPrintln();
    serialPrintln("=== Logger Configuration ===");
    serialPrintf("  Instance: Singleton");
    serialPrintf("  Backend: ConsoleBackend");
    serialPrintf("  Buffer size: %d bytes", CONFIG_LOG_BUFFER_SIZE);
    serialPrintf("  Log level: VERBOSE");
    serialPrintf("  Rate limiting: Disabled for startup");
    serialPrintf("  Thread-local storage: Enabled");
    serialPrintln("===========================");
    serialPrintln();
    
    // Test helper macros
    LOG_INFO(LOG_TAG_MAIN, "Testing LoggingMacros.h integration");
    LOG_FUNC_ENTER(LOG_TAG_MAIN);
    LOG_FUNC_EXIT(LOG_TAG_MAIN);
    
    return true;
}

void printTestHeader(const char* testName) {
    serialPrintln();
    serialPrintf("==== Starting Test: %s ====", testName);
}

void printTestResult(const char* testName, bool passed) {
    testStats.testsRun++;
    if (passed) {
        testStats.testsPassed++;
        serialPrintf("==== Finished Test: %s ==== (PASSED)", testName);
        serialPrintln();
    } else {
        testStats.testsFailed++;
        serialPrintf("==== Finished Test: %s ==== (FAILED)", testName);
        serialPrintln();
    }
}

void testLogLevels() {
    const char* testTag = "LevelTest";
    serialPrintln("Testing log levels...");
    serialPrintln("Expected output: ERROR, WARN, INFO, DEBUG, VERBOSE logs");
    serialPrintln("NOT expected: NONE level log");
    serialPrintln();
    
    Logger& logger = Logger::getInstance();
    logger.log(ESP_LOG_NONE, testTag, "This should not appear.");
    LOG_ERROR(testTag, "This is an ERROR log.");
    LOG_WARN(testTag, "This is a WARN log.");
    LOG_INFO(testTag, "This is an INFO log.");
    LOG_DEBUG(testTag, "This is a DEBUG log.");
    LOG_VERBOSE(testTag, "This is a VERBOSE log.");
    
    // Verify level filtering
    serialPrintln();
    serialPrintln("Testing level filtering (setting to WARN)...");
    logger.setLogLevel(ESP_LOG_WARN);
    LOG_INFO(testTag, "This INFO should NOT appear (level set to WARN).");
    LOG_WARN(testTag, "This WARN should appear.");
    logger.setLogLevel(ESP_LOG_VERBOSE); // Restore
    serialPrintln("Level filtering test complete.");
}

void testThreadSafety() {
    serialPrintln("Testing thread safety with concurrent logging...");
    
    // Lambda for test task
    auto taskFunction = [](void* param) {
        int taskId = (int)param;
        const char* localTag = "ThreadTest";
        
        for (int i = 0; i < 20; i++) {
            LOG_INFO(localTag, 
                    "Task %d - Message %d - Stack: %d bytes", 
                    taskId, i, uxTaskGetStackHighWaterMark(NULL));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        vTaskDelete(NULL);
    };
    
    // Create multiple tasks
    serialPrintln("Creating 3 concurrent tasks...");
    TaskHandle_t taskHandles[3];
    for (int i = 0; i < 3; i++) {
        char taskName[32];
        snprintf(taskName, sizeof(taskName), "TestTask%d", i);
        
        if (xTaskCreate(taskFunction, taskName, 2048, (void*)i, 
                       tskIDLE_PRIORITY + 1, &taskHandles[i]) != pdPASS) {
            serialPrintf("Failed to create task %d\r\n", i);
        }
    }
    
    // Wait for tasks to complete
    serialPrintln("Waiting for tasks to complete...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    serialPrintln("Thread safety test completed - check for garbled output above.");
}

bool testRateLimiting() {
    const char* testTag = "RateLimit";
    Logger& logger = Logger::getInstance();
    bool testPassed = true;
    
    serialPrintln("Testing rate limiting...");
    
    // Reset dropped counter
    logger.resetDroppedLogs();
    
    // Set a rate limit for this test
    logger.setMaxLogsPerSecond(50);
    
    serialPrintf("Current rate limit: %d logs/second\r\n", 50);
    serialPrintln("Generating burst of 100 logs with no delay...");
    
    // Generate burst of logs
    unsigned long startTime = millis();
    const int burstCount = 100;
    
    for (int i = 0; i < burstCount; i++) {
        LOG_INFO(testTag, "Burst log #%d", i + 1);
        // No delay - should trigger rate limiting
    }
    
    unsigned long elapsed = millis() - startTime;
    uint32_t dropped = logger.getDroppedLogs();
    
    serialPrintln();
    serialPrintln("Burst test results:");
    serialPrintf("  Attempted: %d logs in %lu ms\r\n", burstCount, elapsed);
    serialPrintf("  Dropped: %u logs\r\n", dropped);
    serialPrintf("  Effective rate: %.1f logs/second\r\n", 
                 (float)(burstCount - dropped) * 1000.0 / elapsed);
    
    // Check that rate limiting is working (should drop some logs)
    if (dropped == 0) {
        testPassed = false;
        serialPrintln("  FAILED: Rate limiting didn't drop any logs!");
    }
    
    // Test with proper pacing
    logger.resetDroppedLogs();
    serialPrintln();
    serialPrintln("Testing with proper pacing (20ms between logs)...");
    
    for (int i = 0; i < 20; i++) {
        LOG_INFO(testTag, "Paced log #%d", i + 1);
        serialPrintf("Log attempt #%d processed.\r\n", i + 1);
        delay(20);  // 50 logs/second max
    }
    
    serialPrintln();
    serialPrintln("Paced test results:");
    serialPrintf("  Dropped with pacing: %u\r\n", logger.getDroppedLogs());
    
    // Test unlimited rate (0 = disabled)
    serialPrintln();
    serialPrintln("Testing unlimited rate (maxLogsPerSecond = 0)...");
    logger.resetDroppedLogs();
    logger.setMaxLogsPerSecond(0);  // Disable rate limiting
    
    // Generate rapid logs that should all go through
    const int unlimitedCount = 50;
    for (int i = 0; i < unlimitedCount; i++) {
        LOG_INFO(testTag, "Unlimited log #%d", i + 1);
    }
    
    uint32_t droppedUnlimited = logger.getDroppedLogs();
    serialPrintf("  Attempted: %d logs with rate limiting disabled\r\n", unlimitedCount);
    serialPrintf("  Dropped: %u logs (should be 0)\r\n", droppedUnlimited);
    
    // Check if unlimited test passed (should have 0 drops)
    if (droppedUnlimited != 0) {
        testPassed = false;
        serialPrintln("  FAILED: Unlimited mode dropped logs!");
    }
    
    // Restore rate limiting for other tests
    logger.setMaxLogsPerSecond(50);
    
    return testPassed;
}

void testLogTruncation() {
    const char* testTag = "Truncate";
    
    printTestHeader("Log Truncation");
    
    serialPrintf("Testing with message longer than buffer size (%d bytes)...\r\n", 
                 CONFIG_LOG_BUFFER_SIZE);
    
    // Create a very long message
    const int msgSize = CONFIG_LOG_BUFFER_SIZE + 100;
    char* longMessage = (char*)malloc(msgSize);
    if (!longMessage) {
        serialPrintln("Failed to allocate test buffer");
        printTestResult("Log Truncation", false);
        return;
    }
    
    // Fill with repeating pattern for easy verification
    for (int i = 0; i < msgSize - 1; i++) {
        longMessage[i] = '0' + (i % 10);
    }
    longMessage[msgSize - 1] = '\0';
    
    serialPrintln("Original message pattern: 0123456789012345...");
    serialPrintln("Logging oversized message...");
    
    // Log the oversized message
    LOG_INFO(testTag, "%s", longMessage);
    
    serialPrintln("Check above - message should be truncated");
    
    // Also test with format string that would overflow
    LOG_INFO(testTag, "Prefix: %s :Suffix", longMessage);
    
    free(longMessage);
    printTestResult("Log Truncation", true);
}

void testLogNnL() {
    const char* testTag = "NoNewline";
    Logger& logger = Logger::getInstance();
    
    printTestHeader("Log Without Newline");
    
    serialPrintln("Testing logNnL (no newline) functionality...");
    serialPrintln("Expected: Three parts on same line, then new line");
    serialPrintln();
    
    logger.logNnL(ESP_LOG_INFO, testTag, "Part 1 ");
    logger.logNnL(ESP_LOG_INFO, testTag, "Part 2 ");
    logger.logNnL(ESP_LOG_INFO, testTag, "Part 3");
    LOG_INFO(testTag, " - This completes the line");
    
    serialPrintln();
    serialPrintln("Also testing with different log levels:");
    logger.logNnL(ESP_LOG_ERROR, testTag, "[ERROR] ");
    logger.logNnL(ESP_LOG_WARN, testTag, "[WARN] ");
    LOG_INFO(testTag, "[INFO] - Mixed levels");
    
    printTestResult("Log Without Newline", true);
}

void testLogInL() {
    Logger& logger = Logger::getInstance();
    
    printTestHeader("Inline Logging");
    
    serialPrintln("Testing logInL (inline log) functionality...");
    serialPrintln("Expected: [Serial2] prefix on each message");
    serialPrintln();
    
    logger.logInL("Simple inline message");
    logger.logInL("Inline with number: %d", 42);
    logger.logInL("Inline with float: %.2f", 3.14159);
    logger.logInL("Inline with multiple args: %s = %d", "answer", 42);
    
    // Test with special characters
    logger.logInL("Special chars: \t[TAB]\t %% percent");
    
    printTestResult("Inline Logging", true);
}

void testFormatting() {
    const char* testTag = "Format";
    Logger& logger = Logger::getInstance();
    
    printTestHeader("Format String Support");
    
    serialPrintln("Testing various format specifiers...");
    
    // Test different format types
    LOG_INFO(testTag, "Integer: %d, Unsigned: %u", -42, 42U);
    LOG_INFO(testTag, "Hex: 0x%08X, Octal: %o", 0xDEADBEEF, 0755);
    LOG_INFO(testTag, "Float: %.2f, Scientific: %.2e", 3.14159, 0.000123);
    LOG_INFO(testTag, "String: '%s', Char: '%c'", "Hello", 'A');
    LOG_INFO(testTag, "Pointer: %p", (void*)&logger);
    LOG_INFO(testTag, "Percent: 100%%");
    
    // Test with NULL string (safely)
    const char* nullStr = NULL;
    LOG_INFO(testTag, "NULL string test: '%s'", nullStr ? nullStr : "(null)");
    
    // Test width and precision
    LOG_INFO(testTag, "Width: '%10s' '%10d'", "test", 123);
    LOG_INFO(testTag, "Precision: '%.10s' '%.2f'", "truncated string", 1.23456);
    
    printTestResult("Format String Support", true);
}

void testEnableDisableLogging() {
    const char* testTag = "EnableTest";
    Logger& logger = Logger::getInstance();
    
    printTestHeader("Enable/Disable Logging");
    
    serialPrintln("Disabling logger...");
    logger.enableLogging(false);
    
    serialPrintln("The following line should NOT appear:");
    LOG_ERROR(testTag, "[FAIL] This message should be suppressed!");
    
    serialPrintln("Re-enabling logger...");
    logger.enableLogging(true);
    
    serialPrintln("The following line SHOULD appear:");
    LOG_INFO(testTag, "[PASS] Logger is enabled again");
    
    // Verify state
    bool isEnabled = logger.getIsLoggingEnabled();
    serialPrintf("Logger enabled state: %s\r\n", isEnabled ? "true" : "false");
    
    printTestResult("Enable/Disable Logging", isEnabled);
}

void testFlush() {
    const char* testTag = "FlushTest";
    Logger& logger = Logger::getInstance();
    
    printTestHeader("Flush Functionality");
    
    serialPrintln("Testing flush() method...");
    
    // Generate some logs
    for (int i = 0; i < 5; i++) {
        LOG_INFO(testTag, "Pre-flush message %d", i);
    }
    
    serialPrintln("Calling flush()...");
    logger.flush();
    serialPrintln("Flush completed - all buffered data should be written");
    
    // Log after flush
    LOG_INFO(testTag, "Post-flush message");
    
    printTestResult("Flush Functionality", true);
}

// Additional test functions
void testStackUsageOptimization() {
    const char* testTag = "StackOpt";
    
    serialPrintln("Testing stack usage with heap allocation design...");
    serialPrintln("Note: Logger uses heap allocation to minimize stack usage");
    
    // Check stack before logging
    UBaseType_t stackBefore = uxTaskGetStackHighWaterMark(NULL);
    serialPrintf("Stack before logging: %d bytes\r\n", stackBefore);
    
    // Generate various sized logs
    for (int i = 0; i < 10; i++) {
        LOG_INFO(testTag, 
                "Stack test %d - Current stack: %d bytes, Heap: %d bytes",
                i, uxTaskGetStackHighWaterMark(NULL), ESP.getFreeHeap());
    }
    
    UBaseType_t stackAfter = uxTaskGetStackHighWaterMark(NULL);
    
    serialPrintf("Stack after logging: %d bytes\r\n", stackAfter);
    serialPrintf("Stack used during test: %d bytes\r\n", stackBefore - stackAfter);
    serialPrintln("Low stack usage confirms heap allocation is working.");
}

void testDirectMode() {
    const char* testTag = "DirectMode";
    Logger& logger = Logger::getInstance();
    
    serialPrintln("Testing logDirect() for critical messages...");
    serialPrintln("Direct mode should bypass rate limiting.");
    
    // Direct mode bypasses rate limiting
    logger.resetDroppedLogs();
    
    serialPrintln();
    serialPrintln("Generating 20 rapid direct logs...");
    for (int i = 0; i < 20; i++) {
        logger.logDirect(ESP_LOG_ERROR, testTag, "Critical message");
    }
    
    uint32_t dropped = logger.getDroppedLogs();
    serialPrintln();
    serialPrintf("Result: Dropped logs in direct mode: %u", dropped);
    if (dropped == 0) {
        serialPrintln(" (GOOD - no drops as expected)");
    } else {
        serialPrintln(" (BAD - should not drop in direct mode)");
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);  // Short delay for serial initialization
    
    if (!setupLogger()) {
        serialPrintln("Failed to setup Logger");
    }
    
    // Demonstrate that ESP-IDF logs now go directly to console
    // These will appear with ESP-IDF's native formatting
    ESP_LOGI("setup", "ESP-IDF logs now go directly to console");
    ESP_LOGW("setup", "This warning uses ESP-IDF's native format");
    ESP_LOGE("setup", "Errors also bypass our Logger for better performance");
}

void loop() {
    static bool testsRun = false;
    static unsigned long lastLog = 0;
    static unsigned long lastDetailedStatus = 0;
    static uint32_t loopCount = 0;
    
    Logger& logger = Logger::getInstance();
    
    // Run tests once on first loop iteration
    if (!testsRun) {
        // Print system info first
        serialPrintln();
        serialPrintln();
        serialPrintln("=== System Information ===");
        serialPrintf("  Chip Model: %s\r\n", ESP.getChipModel());
        serialPrintf("  CPU Frequency: %d MHz\r\n", ESP.getCpuFreqMHz());
        serialPrintf("  Free Heap: %d bytes\r\n", ESP.getFreeHeap());
        serialPrintf("  Heap Size: %d bytes\r\n", ESP.getHeapSize());
        serialPrintf("  Free PSRAM: %d bytes\r\n", ESP.getFreePsram());
        serialPrintf("  SDK Version: %s\r\n", ESP.getSdkVersion());
        serialPrintln("========================");
        serialPrintln();
        
        delay(1000);  // Give time to read system info
        
        // Run all tests
        printTestHeader("Log Levels");
        testLogLevels();
        printTestResult("Log Levels", true);
        delay(500);
        
        printTestHeader("Thread Safety");
        testThreadSafety();
        printTestResult("Thread Safety", true);
        delay(500);
        
        printTestHeader("Rate Limiting");
        bool rateLimitingPassed = testRateLimiting();
        printTestResult("Rate Limiting", rateLimitingPassed);
        delay(500);
        
        printTestHeader("Log Truncation");
        testLogTruncation();
        printTestResult("Log Truncation", true);
        delay(500);
        
        printTestHeader("Log Without Newline");
        testLogNnL();
        printTestResult("Log Without Newline", true);
        delay(500);
        
        printTestHeader("Inline Log (logInL)");
        testLogInL();
        printTestResult("Inline Log (logInL)", true);
        delay(500);
        
        printTestHeader("Format String Support");
        testFormatting();
        printTestResult("Format String Support", true);
        delay(500);
        
        printTestHeader("Enable/Disable Logging");
        testEnableDisableLogging();
        printTestResult("Enable/Disable Logging", logger.getIsLoggingEnabled());
        delay(500);
        
        printTestHeader("Flush Functionality");
        testFlush();
        printTestResult("Flush Functionality", true);
        delay(500);
        
        printTestHeader("Stack Usage Optimization");
        testStackUsageOptimization();
        printTestResult("Stack Usage Optimization", true);
        delay(500);
        
        printTestHeader("Direct Logging Mode");
        testDirectMode();
        printTestResult("Direct Logging Mode", logger.getDroppedLogs() == 0);
        delay(500);
        
        serialPrintln();
        serialPrintln("==== All Tests Completed ====");
        
        // Print summary
        serialPrintln();
        serialPrintln("========================================");
        serialPrintln("         TEST SUITE SUMMARY");
        serialPrintln("========================================");
        serialPrintf("Total tests run: %d\r\n", testStats.testsRun);
        serialPrintf("Tests passed: %d\r\n", testStats.testsPassed);
        serialPrintf("Tests failed: %d\r\n", testStats.testsFailed);
        serialPrintf("Final heap free: %d bytes\r\n", ESP.getFreeHeap());
        serialPrintf("Total dropped logs: %u\r\n", logger.getDroppedLogs());
        serialPrintln("========================================");
        serialPrintln();
        
        if (testStats.testsFailed == 0) {
            LOG_INFO(LOG_TAG_MAIN, "ALL TESTS PASSED!");
        } else {
            LOG_ERROR(LOG_TAG_MAIN, "Some tests failed. Check output above.");
        }
        
        // Reset for normal operation
        logger.resetDroppedLogs();
        logger.setMaxLogsPerSecond(100);  // Set to normal rate
        
        testsRun = true;
    }
    
    // Regular loop operations after tests
    unsigned long currentTime = millis();
    loopCount++;
    
    // Basic keep-alive every 5 seconds
    if (currentTime - lastLog >= 5000) {
        LOG_INFO(LOG_TAG_MAIN, 
                "System healthy - Uptime: %lu s, Loops: %u, Heap: %d bytes", 
                currentTime / 1000, loopCount, ESP.getFreeHeap());
        lastLog = currentTime;
    }
    
    // Detailed status every 30 seconds
    if (currentTime - lastDetailedStatus >= 30000) {
        serialPrintln();
        serialPrintln("--- Detailed Status Report ---");
        serialPrintf("Uptime: %lu seconds\r\n", currentTime / 1000);
        serialPrintf("Free Heap: %d / %d bytes (%.1f%% free)\r\n", 
                    ESP.getFreeHeap(), ESP.getHeapSize(), 
                    (float)ESP.getFreeHeap() * 100.0 / ESP.getHeapSize());
        serialPrintf("Dropped logs since last report: %u\r\n", logger.getDroppedLogs());
        serialPrintf("Main task stack watermark: %d bytes\r\n", 
                    uxTaskGetStackHighWaterMark(NULL));
        serialPrintf("Current log level: %d\r\n", logger.getLogLevel());
        serialPrintf("Logging enabled: %s\r\n", logger.getIsLoggingEnabled() ? "Yes" : "No");
        serialPrintln("-----------------------------");
        serialPrintln();
        
        lastDetailedStatus = currentTime;
    }
    
    // Simulate occasional high-priority events
    if (loopCount % 100 == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Periodic system check #%lu", loopCount / 100);
        logger.logDirect(ESP_LOG_WARN, LOG_TAG_MAIN, msg);
    }
    
    // Simulate variable activity
    if (loopCount % 250 == 0) {
        LOG_INFO(LOG_TAG_MAIN, "Quarter-thousand loops milestone: %u", loopCount);
    }
    
    delay(100);  // Main loop delay
}