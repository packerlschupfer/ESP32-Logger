/**
 * @file test_thread_safety.cpp
 * @brief Thread safety tests for Logger
 */

#ifdef UNIT_TEST

#include <Arduino.h>
#include <unity.h>
#include <Logger.h>

#define TEST_THREADS 4
#define TEST_ITERATIONS 200

static SemaphoreHandle_t startSemaphore = nullptr;
static volatile int threadsDone = 0;
static volatile int totalLogs = 0;

// Counting backend
class CountingBackend : public ILogBackend {
public:
    std::atomic<int> writeCount{0};

    bool write(const char* message, size_t length) override {
        writeCount++;
        return true;
    }

    void flush() override {}

    void reset() {
        writeCount = 0;
    }
};

static std::shared_ptr<CountingBackend> countingBackend;

void loggerTask(void* param) {
    int taskId = (int)(intptr_t)param;
    Logger& logger = Logger::getInstance();

    // Wait for start signal
    xSemaphoreTake(startSemaphore, portMAX_DELAY);

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        logger.log(ESP_LOG_INFO, "THREAD", "Task %d, iteration %d", taskId, i);
        totalLogs++;

        // Small random delay
        vTaskDelay(pdMS_TO_TICKS(random(0, 3)));
    }

    threadsDone++;
    vTaskDelete(NULL);
}

void test_concurrent_logging() {
    threadsDone = 0;
    totalLogs = 0;

    countingBackend = std::make_shared<CountingBackend>();
    Logger& logger = Logger::getInstance();
    logger.clearBackends();
    logger.addBackend(countingBackend);
    logger.setMaxLogsPerSecond(10000);  // High limit for this test
    logger.setLogLevel(ESP_LOG_VERBOSE);

    startSemaphore = xSemaphoreCreateBinary();

    // Create worker tasks
    for (int i = 0; i < TEST_THREADS; i++) {
        xTaskCreate(loggerTask, "LogTask", 4096, (void*)(intptr_t)i, 1, NULL);
    }

    // Start all tasks simultaneously
    xSemaphoreGive(startSemaphore);

    // Wait for all tasks to complete
    unsigned long start = millis();
    while (threadsDone < TEST_THREADS && (millis() - start) < 30000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    TEST_ASSERT_EQUAL(TEST_THREADS, threadsDone);

    // All logs should have been written (minus any rate-limited ones)
    int expected = TEST_THREADS * TEST_ITERATIONS;
    int written = countingBackend->writeCount;
    int dropped = logger.getDroppedLogs();

    TEST_ASSERT_EQUAL(expected, written + dropped);

    vSemaphoreDelete(startSemaphore);
}

void bufferPoolTask(void* param) {
    xSemaphoreTake(startSemaphore, portMAX_DELAY);

    for (int i = 0; i < 100; i++) {
        char* buffer = BufferPool::getInstance().acquire();
        if (buffer) {
            // Use the buffer briefly
            snprintf(buffer, BufferPool::BUFFER_SIZE, "Task data %d", i);
            vTaskDelay(1);
            BufferPool::getInstance().release(buffer);
        }
    }

    threadsDone++;
    vTaskDelete(NULL);
}

void test_concurrent_buffer_pool() {
    threadsDone = 0;
    startSemaphore = xSemaphoreCreateBinary();

    // Create tasks that hammer the buffer pool
    for (int i = 0; i < TEST_THREADS; i++) {
        xTaskCreate(bufferPoolTask, "BufTask", 2048, NULL, 1, NULL);
    }

    xSemaphoreGive(startSemaphore);

    // Wait for completion
    unsigned long start = millis();
    while (threadsDone < TEST_THREADS && (millis() - start) < 10000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    TEST_ASSERT_EQUAL(TEST_THREADS, threadsDone);

    vSemaphoreDelete(startSemaphore);
}

void tagLevelTask(void* param) {
    int taskId = (int)(intptr_t)param;
    Logger& logger = Logger::getInstance();

    xSemaphoreTake(startSemaphore, portMAX_DELAY);

    char tag[16];
    snprintf(tag, sizeof(tag), "TAG%d", taskId);

    for (int i = 0; i < 50; i++) {
        // Randomly change tag level
        esp_log_level_t level = (esp_log_level_t)(random(0, 6));
        logger.setTagLevel(tag, level);

        // Log at various levels
        logger.log(ESP_LOG_INFO, tag, "Message %d at level %d", i, level);

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    threadsDone++;
    vTaskDelete(NULL);
}

void test_concurrent_tag_level_changes() {
    threadsDone = 0;

    countingBackend->reset();
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(ESP_LOG_VERBOSE);

    startSemaphore = xSemaphoreCreateBinary();

    for (int i = 0; i < TEST_THREADS; i++) {
        xTaskCreate(tagLevelTask, "TagTask", 4096, (void*)(intptr_t)i, 1, NULL);
    }

    xSemaphoreGive(startSemaphore);

    unsigned long start = millis();
    while (threadsDone < TEST_THREADS && (millis() - start) < 10000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // If we get here without crashing, test passed
    TEST_ASSERT_EQUAL(TEST_THREADS, threadsDone);

    vSemaphoreDelete(startSemaphore);
}

void runThreadSafetyTests() {
    UNITY_BEGIN();

    RUN_TEST(test_concurrent_logging);
    RUN_TEST(test_concurrent_buffer_pool);
    RUN_TEST(test_concurrent_tag_level_changes);

    UNITY_END();
}

void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println("\n=== Logger Thread Safety Tests ===\n");
    runThreadSafetyTests();
}

void loop() {}

#endif // UNIT_TEST
