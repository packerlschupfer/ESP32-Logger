/**
 * @file test_logger.cpp
 * @brief Unit tests for Logger
 */

#ifdef UNIT_TEST

#include <Arduino.h>
#include <unity.h>
#include <Logger.h>
#include <vector>
#include <string>

// Test backend to capture log output
class TestLogBackend : public ILogBackend {
public:
    std::vector<std::string> messages;
    bool writeResult = true;

    bool write(const char* message, size_t length) override {
        if (writeResult) {
            messages.push_back(std::string(message, length));
        }
        return writeResult;
    }

    void flush() override {}

    void clear() {
        messages.clear();
    }
};

static Logger* logger = nullptr;
static std::shared_ptr<TestLogBackend> testBackend;

void setUp() {
    if (logger == nullptr) {
        logger = &Logger::getInstance();
        testBackend = std::make_shared<TestLogBackend>();
        logger->setBackend(testBackend);
        logger->init();
        logger->enableLogging(true);
        logger->setLogLevel(ESP_LOG_VERBOSE);
    }
    testBackend->clear();
}

void tearDown() {
    // Keep logger instance
}

// ============= Basic Logging Tests =============

void test_logger_initialization() {
    TEST_ASSERT_TRUE(logger->isInitialized());
    TEST_ASSERT_TRUE(logger->getIsLoggingEnabled());
}

void test_log_level_setting() {
    logger->setLogLevel(ESP_LOG_INFO);
    TEST_ASSERT_EQUAL(ESP_LOG_INFO, logger->getLogLevel());

    logger->setLogLevel(ESP_LOG_DEBUG);
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, logger->getLogLevel());

    // Reset to verbose for other tests
    logger->setLogLevel(ESP_LOG_VERBOSE);
}

void test_log_message_capture() {
    logger->log(ESP_LOG_INFO, "TEST", "Hello World");

    TEST_ASSERT_EQUAL(1, testBackend->messages.size());
    TEST_ASSERT_TRUE(testBackend->messages[0].find("Hello World") != std::string::npos);
}

void test_log_with_format() {
    logger->log(ESP_LOG_INFO, "TEST", "Value: %d, String: %s", 42, "test");

    TEST_ASSERT_EQUAL(1, testBackend->messages.size());
    TEST_ASSERT_TRUE(testBackend->messages[0].find("42") != std::string::npos);
    TEST_ASSERT_TRUE(testBackend->messages[0].find("test") != std::string::npos);
}

// ============= Log Level Filtering Tests =============

void test_log_level_filtering() {
    logger->setLogLevel(ESP_LOG_WARN);

    logger->log(ESP_LOG_VERBOSE, "TEST", "Verbose message");
    logger->log(ESP_LOG_DEBUG, "TEST", "Debug message");
    logger->log(ESP_LOG_INFO, "TEST", "Info message");
    logger->log(ESP_LOG_WARN, "TEST", "Warn message");
    logger->log(ESP_LOG_ERROR, "TEST", "Error message");

    // Only WARN and ERROR should pass through
    TEST_ASSERT_EQUAL(2, testBackend->messages.size());

    // Reset
    logger->setLogLevel(ESP_LOG_VERBOSE);
}

void test_logging_disabled() {
    logger->enableLogging(false);

    logger->log(ESP_LOG_ERROR, "TEST", "Should not appear");

    TEST_ASSERT_EQUAL(0, testBackend->messages.size());

    // Re-enable
    logger->enableLogging(true);
}

// ============= Tag-Level Filtering Tests =============

void test_tag_level_filtering() {
    logger->setLogLevel(ESP_LOG_VERBOSE);  // Global level
    logger->setTagLevel("QUIET", ESP_LOG_ERROR);  // Only errors for QUIET tag

    logger->log(ESP_LOG_INFO, "QUIET", "Should be filtered");
    logger->log(ESP_LOG_ERROR, "QUIET", "Should appear");
    logger->log(ESP_LOG_INFO, "OTHER", "Should also appear");

    TEST_ASSERT_EQUAL(2, testBackend->messages.size());
}

void test_is_level_enabled_for_tag() {
    logger->setLogLevel(ESP_LOG_INFO);
    logger->setTagLevel("DEBUG_TAG", ESP_LOG_DEBUG);

    // Global level check
    TEST_ASSERT_TRUE(logger->isLevelEnabledForTag("NORMAL", ESP_LOG_INFO));
    TEST_ASSERT_FALSE(logger->isLevelEnabledForTag("NORMAL", ESP_LOG_DEBUG));

    // Tag-specific level check
    TEST_ASSERT_TRUE(logger->isLevelEnabledForTag("DEBUG_TAG", ESP_LOG_DEBUG));
}

// ============= Level String Conversion Tests =============

void test_level_to_string() {
    TEST_ASSERT_EQUAL_STRING("N", Logger::levelToString(ESP_LOG_NONE));
    TEST_ASSERT_EQUAL_STRING("E", Logger::levelToString(ESP_LOG_ERROR));
    TEST_ASSERT_EQUAL_STRING("W", Logger::levelToString(ESP_LOG_WARN));
    TEST_ASSERT_EQUAL_STRING("I", Logger::levelToString(ESP_LOG_INFO));
    TEST_ASSERT_EQUAL_STRING("D", Logger::levelToString(ESP_LOG_DEBUG));
    TEST_ASSERT_EQUAL_STRING("V", Logger::levelToString(ESP_LOG_VERBOSE));
}

// ============= Rate Limiting Tests =============

void test_rate_limiting() {
    logger->setMaxLogsPerSecond(10);
    logger->resetDroppedLogs();

    // Flood with messages
    for (int i = 0; i < 50; i++) {
        logger->log(ESP_LOG_INFO, "FLOOD", "Message %d", i);
    }

    // Should have dropped some
    uint32_t dropped = logger->getDroppedLogs();
    TEST_ASSERT_GREATER_THAN(0, dropped);

    // Reset rate limit
    logger->setMaxLogsPerSecond(MAX_LOGS_PER_SECOND);
}

// ============= Buffer Pool Tests =============

void test_buffer_pool_acquire_release() {
    char* buffer1 = BufferPool::getInstance().acquire();
    TEST_ASSERT_NOT_NULL(buffer1);

    char* buffer2 = BufferPool::getInstance().acquire();
    TEST_ASSERT_NOT_NULL(buffer2);

    TEST_ASSERT_NOT_EQUAL(buffer1, buffer2);

    BufferPool::getInstance().release(buffer1);
    BufferPool::getInstance().release(buffer2);
}

void test_buffer_pool_exhaustion() {
    std::vector<char*> buffers;

    // Acquire all buffers
    for (int i = 0; i < CONFIG_LOG_BUFFER_POOL_SIZE; i++) {
        char* buf = BufferPool::getInstance().acquire();
        if (buf) {
            buffers.push_back(buf);
        }
    }

    // Try to acquire one more (should fail or return nullptr)
    char* extra = BufferPool::getInstance().acquire();

    // Release all
    for (char* buf : buffers) {
        BufferPool::getInstance().release(buf);
    }
    if (extra) {
        BufferPool::getInstance().release(extra);
    }

    TEST_PASS();  // Test passes if no crash
}

// ============= Multiple Backend Tests =============

void test_multiple_backends() {
    auto backend1 = std::make_shared<TestLogBackend>();
    auto backend2 = std::make_shared<TestLogBackend>();

    logger->clearBackends();
    logger->addBackend(backend1);
    logger->addBackend(backend2);

    logger->log(ESP_LOG_INFO, "TEST", "Multi-backend test");

    TEST_ASSERT_EQUAL(1, backend1->messages.size());
    TEST_ASSERT_EQUAL(1, backend2->messages.size());

    // Restore original backend
    logger->clearBackends();
    logger->addBackend(testBackend);
}

// ============= Direct Logging Tests =============

void test_log_direct() {
    logger->setMaxLogsPerSecond(1);  // Very restrictive

    // Direct logging should bypass rate limiting
    logger->logDirect(ESP_LOG_INFO, "DIRECT", "Direct message 1");
    logger->logDirect(ESP_LOG_INFO, "DIRECT", "Direct message 2");

    TEST_ASSERT_EQUAL(2, testBackend->messages.size());

    // Reset
    logger->setMaxLogsPerSecond(MAX_LOGS_PER_SECOND);
}

// Test runner
void runLoggerTests() {
    UNITY_BEGIN();

    RUN_TEST(test_logger_initialization);
    RUN_TEST(test_log_level_setting);
    RUN_TEST(test_log_message_capture);
    RUN_TEST(test_log_with_format);
    RUN_TEST(test_log_level_filtering);
    RUN_TEST(test_logging_disabled);
    RUN_TEST(test_tag_level_filtering);
    RUN_TEST(test_is_level_enabled_for_tag);
    RUN_TEST(test_level_to_string);
    RUN_TEST(test_rate_limiting);
    RUN_TEST(test_buffer_pool_acquire_release);
    RUN_TEST(test_buffer_pool_exhaustion);
    RUN_TEST(test_multiple_backends);
    RUN_TEST(test_log_direct);

    UNITY_END();
}

void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println("\n=== Logger Unit Tests ===\n");
    runLoggerTests();
}

void loop() {}

#endif // UNIT_TEST
