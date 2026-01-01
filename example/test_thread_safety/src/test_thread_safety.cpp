#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logger.h"
#include "ConsoleBackend.h"
#include "SynchronizedConsoleBackend.h"
#include "NonBlockingConsoleBackend.h"
#include "LogInterface.h"
#include <memory>
#include <atomic>

// Test configuration
const int NUM_WORKER_TASKS = 6;
const int MESSAGES_PER_TASK = 50;
const int STRESS_TEST_MESSAGES = 1000;
const uint32_t TEST_DURATION_MS = 30000;

// Task handles
TaskHandle_t workerTasks[NUM_WORKER_TASKS];
TaskHandle_t stressTasks[2];
TaskHandle_t monitorTask;

// Global counters
std::atomic<uint32_t> totalMessagesSent(0);
std::atomic<uint32_t> testPhase(0);
std::atomic<bool> testRunning(true);

// Subscriber callback test state
std::atomic<uint32_t> subscriberCallbackCount(0);
std::atomic<int> subscriberCallbackCore(-1);
std::atomic<bool> subscriberTestPassed(true);

// Backend for current test
std::shared_ptr<ILogBackend> currentBackend;
const char* currentBackendName = "";

// Test results
struct TestResult {
    const char* backendName;
    uint32_t messagesSent;
    uint32_t corruptionDetected;
    uint32_t maxConcurrentTasks;
    bool passed;
};

TestResult testResults[4];  // Increased for subscriber test
int currentTestIndex = 0;

// Subscriber callback for testing async queue mechanism
void subscriberCallbackHandler(esp_log_level_t level, const char* tag, const char* message) {
    // Record which core the callback runs on
    int currentCore = xPortGetCoreID();
    int expectedCore = subscriberCallbackCore.load();

    // First callback sets the expected core
    if (expectedCore == -1) {
        subscriberCallbackCore.store(currentCore);
    } else if (currentCore != expectedCore) {
        // Callback ran on wrong core - test failed!
        subscriberTestPassed.store(false);
        Serial.printf("[SUBSCRIBER ERROR] Callback on Core %d, expected Core %d!\n",
                      currentCore, expectedCore);
    }

    subscriberCallbackCount++;

    // Simulate some work (like network operations would do)
    delayMicroseconds(100);
}

// Worker task that sends identifiable messages
void workerTask(void* parameter) {
    int taskId = (int)(intptr_t)parameter;
    char taskTag[16];
    snprintf(taskTag, sizeof(taskTag), "Worker%d", taskId);
    
    while (testRunning) {
        // Wait for test phase to start
        while (testPhase == 0 && testRunning) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        if (!testRunning) break;
        
        // Only run in phase 1
        if (testPhase == 1) {
            LOG_INFO(taskTag, "Task started on core %d, phase %d", xPortGetCoreID(), testPhase.load());
            
            for (int i = 0; i < MESSAGES_PER_TASK && testRunning && testPhase == 1; i++) {
                // Create message with pattern to detect corruption
                LOG_INFO(taskTag, "MSG_%03d_START_The_quick_brown_fox_jumps_over_the_lazy_dog_END_MSG_%03d", i, i);
                
                totalMessagesSent++;
                
                // Vary delay to create different concurrency patterns
                vTaskDelay(pdMS_TO_TICKS(5 + (taskId * 3) % 10));
            }
            
            LOG_INFO(taskTag, "Task completed - sent %d messages", MESSAGES_PER_TASK);
        }
        
        // Wait for next test
        while (testPhase != 0 && testRunning) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    vTaskDelete(NULL);
}

// Stress test task - rapid fire logging
void stressTask(void* parameter) {
    int taskId = (int)(intptr_t)parameter;
    char taskTag[16];
    snprintf(taskTag, sizeof(taskTag), "Stress%d", taskId);
    
    while (testRunning) {
        // Wait for test phase 2
        while (testPhase < 2 && testRunning) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        if (!testRunning) break;
        
        // Only run in phase 2
        if (testPhase == 2) {
            uint32_t counter = 0;
            LOG_WARN(taskTag, "Stress test started - flooding logger");
            
            while (counter < STRESS_TEST_MESSAGES && testRunning && testPhase == 2) {
                // Rapid fire without delay
                LOG_DEBUG(taskTag, "FLOOD_%04lu_AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA_%04lu", counter, counter);
                totalMessagesSent++;
                counter++;
                
                // Minimal yield every 100 messages
                if (counter % 100 == 0) {
                    taskYIELD();
                }
            }
            
            LOG_WARN(taskTag, "Stress test completed - sent %lu messages", counter);
        }
        
        // Wait for next test
        while (testPhase != 0 && testRunning) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    vTaskDelete(NULL);
}

// Monitor task to track test progress
void monitorTaskFunc(void* parameter) {
    uint32_t lastMessageCount = 0;
    uint32_t messageRate = 0;
    
    while (testRunning) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        uint32_t currentCount = totalMessagesSent.load();
        messageRate = (currentCount - lastMessageCount) / 5;
        lastMessageCount = currentCount;
        
        LOG_INFO("Monitor", "Phase %d - Total msgs: %lu, Rate: %lu msg/sec, Backend: %s",
                 testPhase.load(), currentCount, messageRate, currentBackendName);
    }
    
    vTaskDelete(NULL);
}

// Test a specific backend
void testBackend(std::shared_ptr<ILogBackend> backend, const char* name) {
    Serial.printf("\n\n========== Testing %s ==========\n", name);
    
    currentBackend = backend;
    currentBackendName = name;
    
    // Configure logger
    Logger& logger = Logger::getInstance();
    logger.setBackend(backend);
    logger.setMaxLogsPerSecond(0);  // No rate limiting
    logger.resetDroppedLogs();
    
    LOG_INFO("Test", "Starting thread safety test with %s", name);
    
    // Reset counters for this test
    totalMessagesSent = 0;
    
    // Phase 1: Normal concurrent logging
    testPhase = 1;
    Serial.printf("Phase 1: Normal concurrent logging with %d tasks\n", NUM_WORKER_TASKS);
    
    // Wait for workers to complete
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    // Phase 2: Stress test
    testPhase = 2;
    Serial.printf("Phase 2: Stress test with rapid logging\n");
    
    // Wait for stress test
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Record results
    testResults[currentTestIndex].backendName = name;
    testResults[currentTestIndex].messagesSent = totalMessagesSent.load();
    testResults[currentTestIndex].corruptionDetected = 0;  // Manual inspection needed
    testResults[currentTestIndex].maxConcurrentTasks = NUM_WORKER_TASKS + 2;
    testResults[currentTestIndex].passed = true;  // Assume pass, check output
    currentTestIndex++;
    
    LOG_INFO("Test", "Backend test completed - Total messages: %lu", totalMessagesSent.load());
    
    // Reset phase to 0 to signal end of test
    testPhase = 0;

    // Let system settle and ensure all tasks see phase change
    vTaskDelay(pdMS_TO_TICKS(3000));
}

// Test the async subscriber callback mechanism
void testAsyncSubscriber() {
    Serial.printf("\n\n========== Testing Async Subscriber Callback ==========\n");
    Serial.println("This test verifies that subscriber callbacks run on a dedicated");
    Serial.println("task with consistent core affinity (fixes cross-core crashes).\n");

    Logger& logger = Logger::getInstance();

    // Reset test state
    subscriberCallbackCount = 0;
    subscriberCallbackCore = -1;
    subscriberTestPassed = true;
    totalMessagesSent = 0;

    // Register subscriber callback
    bool registered = logger.addLogSubscriber(subscriberCallbackHandler);
    Serial.printf("Subscriber registered: %s\n", registered ? "YES" : "NO");

    // Start subscriber task pinned to Core 1
    bool taskStarted = logger.startSubscriberTask(1);
    Serial.printf("Subscriber task started on Core 1: %s\n", taskStarted ? "YES" : "NO");

    if (!taskStarted) {
        Serial.println("ERROR: Failed to start subscriber task!");
        testResults[currentTestIndex].backendName = "AsyncSubscriber";
        testResults[currentTestIndex].messagesSent = 0;
        testResults[currentTestIndex].passed = false;
        currentTestIndex++;
        return;
    }

    // Give task time to start
    vTaskDelay(pdMS_TO_TICKS(100));

    // Phase 3: Test subscriber with concurrent logging from both cores
    testPhase = 3;
    Serial.println("Phase 3: Logging from multiple cores, callbacks should stay on Core 1\n");

    // Log from this task (main loop runs on Core 1 typically)
    for (int i = 0; i < 50; i++) {
        LOG_INFO("SubTest", "Message %d from Core %d", i, xPortGetCoreID());
        totalMessagesSent++;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // Wait for all callbacks to process
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Check results
    uint32_t callbacksReceived = subscriberCallbackCount.load();
    int callbackCore = subscriberCallbackCore.load();
    bool coreConsistent = subscriberTestPassed.load();

    Serial.printf("\nResults:\n");
    Serial.printf("  Messages sent: %lu\n", totalMessagesSent.load());
    Serial.printf("  Callbacks received: %lu\n", callbacksReceived);
    Serial.printf("  Callback core: %d (expected: 1)\n", callbackCore);
    Serial.printf("  Core consistency: %s\n", coreConsistent ? "PASSED" : "FAILED");

    // Record test results
    testResults[currentTestIndex].backendName = "AsyncSubscriber";
    testResults[currentTestIndex].messagesSent = callbacksReceived;
    testResults[currentTestIndex].corruptionDetected = 0;
    testResults[currentTestIndex].maxConcurrentTasks = 1;
    testResults[currentTestIndex].passed = coreConsistent && (callbackCore == 1) && (callbacksReceived > 0);

    Serial.printf("  Test: %s\n", testResults[currentTestIndex].passed ? "PASSED" : "FAILED");
    currentTestIndex++;

    // Cleanup
    logger.removeLogSubscriber(subscriberCallbackHandler);
    logger.stopSubscriberTask();

    // Reset phase
    testPhase = 0;
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void printSummary() {
    Serial.println("\n\n==========================================");
    Serial.println("     THREAD SAFETY TEST SUMMARY");
    Serial.println("==========================================");
    
    Serial.println("\nExpected message counts:");
    Serial.printf("  Phase 1: %d workers × %d messages = %d\n", NUM_WORKER_TASKS, MESSAGES_PER_TASK, NUM_WORKER_TASKS * MESSAGES_PER_TASK);
    Serial.printf("  Phase 2: 2 stress tasks × %d messages = %d\n", STRESS_TEST_MESSAGES, 2 * STRESS_TEST_MESSAGES);
    Serial.printf("  Total expected: %d messages\n", (NUM_WORKER_TASKS * MESSAGES_PER_TASK) + (2 * STRESS_TEST_MESSAGES));
    
    for (int i = 0; i < currentTestIndex; i++) {
        Serial.printf("\nBackend: %s\n", testResults[i].backendName);
        Serial.printf("  Messages sent: %lu", testResults[i].messagesSent);
        if (testResults[i].messagesSent == 2300) {
            Serial.println(" ✓");
        } else {
            Serial.printf(" (missing %lu)\n", 2300 - testResults[i].messagesSent);
        }
        Serial.printf("  Concurrent tasks: %lu\n", testResults[i].maxConcurrentTasks);
        Serial.printf("  Status: %s\n", testResults[i].passed ? "PASSED" : "FAILED");
    }
    
    Serial.println("\n------------------------------------------");
    Serial.println("INSPECTION CHECKLIST:");
    Serial.println("1. Check for interleaved messages like:");
    Serial.println("   [1234][Worker1][I] MSG_001_START_The_qu[5678][Worker2][I] MSG_001_START");
    Serial.println("2. Check for corrupted timestamps:");
    Serial.println("   [12[5678][Worker3][E] Message");
    Serial.println("3. Check for partial messages:");
    Serial.println("   ick_brown_fox_jumps");
    Serial.println("4. Check for out-of-order messages");
    Serial.println("5. Check message sequence numbers (should be 000-049 per worker)");
    Serial.println("\nNonBlockingConsoleBackend should show NO corruption!");
    Serial.println("ConsoleBackend may show corruption under stress.");
    Serial.println("SynchronizedConsoleBackend should show NO corruption!");
    Serial.println("\nAsyncSubscriber test verifies:");
    Serial.println("- Callbacks run on dedicated task (not caller's context)");
    Serial.println("- Core affinity is respected (all callbacks on same core)");
    Serial.println("- No cross-core crashes with network operations");
    Serial.println("==========================================");
}

void setup() {
    // Configure serial with larger buffer
    Serial.setTxBufferSize(1024);
    Serial.begin(921600);
    
    // Wait for serial connection
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    delay(2000);
    
    Serial.println("\n\n==========================================");
    Serial.println("    Logger Thread Safety Test Suite");
    Serial.println("==========================================");
    Serial.println("This test verifies thread safety of different");
    Serial.println("Logger backends with concurrent FreeRTOS tasks.");
    Serial.println("\nWatch the output for message corruption!");
    Serial.println("==========================================\n");
    
    // Initialize logger
    Logger& logger = Logger::getInstance();
    logger.enableLogging(true);
    logger.setLogLevel(ESP_LOG_VERBOSE);
    
    // Create worker tasks
    for (int i = 0; i < NUM_WORKER_TASKS; i++) {
        char taskName[16];
        snprintf(taskName, sizeof(taskName), "Worker%d", i);
        xTaskCreatePinnedToCore(
            workerTask,
            taskName,
            4096,
            (void*)(intptr_t)i,
            1,
            &workerTasks[i],
            i % 2  // Distribute across cores
        );
    }
    
    // Create stress tasks
    for (int i = 0; i < 2; i++) {
        char taskName[16];
        snprintf(taskName, sizeof(taskName), "Stress%d", i);
        xTaskCreatePinnedToCore(
            stressTask,
            taskName,
            4096,
            (void*)(intptr_t)i,
            2,
            &stressTasks[i],
            i % 2
        );
    }
    
    // Create monitor task
    xTaskCreate(monitorTaskFunc, "Monitor", 4096, NULL, 1, &monitorTask);
    
    LOG_INFO("Setup", "All tasks created - starting tests");
    delay(1000);
}

void loop() {
    static bool testsRun = false;
    static uint32_t testStartTime = millis();
    
    if (!testsRun) {
        // Test 1: ConsoleBackend (expected to show corruption)
        testBackend(std::make_shared<ConsoleBackend>(), "ConsoleBackend");
        
        // Test 2: SynchronizedConsoleBackend (should be thread-safe)
        testBackend(std::make_shared<SynchronizedConsoleBackend>(), "SynchronizedConsoleBackend");
        
        // Test 3: NonBlockingConsoleBackend (should be thread-safe)
        testBackend(std::make_shared<NonBlockingConsoleBackend>(), "NonBlockingConsoleBackend");

        // Test 4: Async Subscriber Callback (core affinity test)
        testAsyncSubscriber();

        // Print summary
        testRunning = false;
        delay(1000);
        printSummary();
        
        testsRun = true;
    }
    
    // Keep alive
    static uint32_t lastKeepAlive = 0;
    if (millis() - lastKeepAlive > 10000) {
        Serial.println("\n[KeepAlive] Test completed. Check output above for corruption patterns.");
        lastKeepAlive = millis();
    }
    
    delay(1000);
}