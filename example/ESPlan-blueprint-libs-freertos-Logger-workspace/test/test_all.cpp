#include <Arduino.h>
#include <unity.h>
#include <memory>
#include <unordered_map>
#include "Logger.h"
#include "MockLogger.h"
#include "ConsoleBackend.h"

// ===== Basic Logger Tests =====

// Test the singleton Logger instance
void test_singleton_instance() {
    Logger& instance1 = Logger::getInstance();
    Logger& instance2 = Logger::getInstance();
    
    TEST_ASSERT_EQUAL(&instance1, &instance2);
}

// Test Logger basic functionality
void test_logger_basic() {
    Logger& logger = Logger::getInstance();
    logger.init(256);
    logger.enableLogging(true);
    logger.setLogLevel(ESP_LOG_VERBOSE);
    
    TEST_ASSERT_TRUE(logger.getIsLoggingEnabled());
    TEST_ASSERT_EQUAL(ESP_LOG_VERBOSE, logger.getLogLevel());
    
    // Test that logging doesn't crash
    logger.log(ESP_LOG_INFO, "Test", "Basic test message");
}

// Test mutex exists
void test_mutex_exists() {
    Logger& logger = Logger::getInstance();
    SemaphoreHandle_t mutex = logger.getMutex();
    TEST_ASSERT_NOT_NULL(mutex);
}

// Test enable/disable
void test_enable_disable() {
    Logger& logger = Logger::getInstance();
    
    logger.enableLogging(false);
    TEST_ASSERT_FALSE(logger.getIsLoggingEnabled());
    
    logger.enableLogging(true);
    TEST_ASSERT_TRUE(logger.getIsLoggingEnabled());
}

// Test log levels
void test_log_levels() {
    Logger& logger = Logger::getInstance();
    
    logger.setLogLevel(ESP_LOG_ERROR);
    TEST_ASSERT_EQUAL(ESP_LOG_ERROR, logger.getLogLevel());
    
    logger.setLogLevel(ESP_LOG_WARN);
    TEST_ASSERT_EQUAL(ESP_LOG_WARN, logger.getLogLevel());
    
    logger.setLogLevel(ESP_LOG_INFO);
    TEST_ASSERT_EQUAL(ESP_LOG_INFO, logger.getLogLevel());
}

// Test rate limiting configuration
void test_rate_limit_config() {
    Logger& logger = Logger::getInstance();
    
    // Test setting different rate limits
    logger.setMaxLogsPerSecond(10);
    logger.setMaxLogsPerSecond(100);
    logger.setMaxLogsPerSecond(1);
    
    // Test should pass if no crash occurs
    TEST_ASSERT_TRUE(true);
}

// Test direct mode
void test_direct_mode() {
    Logger& logger = Logger::getInstance();
    
    logger.setDirectMode(true);
    logger.log(ESP_LOG_INFO, "DirectTest", "Direct mode message");
    
    logger.setDirectMode(false);
    logger.log(ESP_LOG_INFO, "NormalTest", "Normal mode message");
    
    // Test should pass if no crash occurs
    TEST_ASSERT_TRUE(true);
}

// Test null safety
void test_null_safety() {
    Logger& logger = Logger::getInstance();
    
    // Test with null tag
    logger.log(ESP_LOG_INFO, nullptr, "Message with null tag");
    
    // Test with null format (should return early)
    logger.log(ESP_LOG_INFO, "Test", nullptr);
    
    // Test should pass if no crash occurs
    TEST_ASSERT_TRUE(true);
}

// Test context functionality
void test_context() {
    Logger& logger = Logger::getInstance();
    
    std::unordered_map<std::string, std::string> ctx;
    ctx["user"] = "testuser";
    ctx["session"] = "12345";
    logger.setContext(ctx);
    
    std::string context = logger.getContext();
    TEST_ASSERT_TRUE(context.length() > 0);
    TEST_ASSERT_TRUE(context.find("user=testuser") != std::string::npos);
    
    // Clear context by setting empty map
    logger.setContext(std::unordered_map<std::string, std::string>());
    context = logger.getContext();
    TEST_ASSERT_EQUAL_STRING("", context.c_str());
}

// Test tag levels
void test_tag_levels() {
    Logger& logger = Logger::getInstance();
    
    logger.setTagLevel("Network", ESP_LOG_DEBUG);
    esp_log_level_t level = logger.getTagLevel("Network");
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, level);
    
    // Unknown tag should return global level
    logger.setLogLevel(ESP_LOG_WARN);
    level = logger.getTagLevel("UnknownTag");
    TEST_ASSERT_EQUAL(ESP_LOG_WARN, level);
}

// ===== Backend Integration Tests =====

// Test backend receives messages
void test_backend_receives_messages() {
    auto mockBackend = std::make_shared<MockLogger>();
    Logger testLogger(mockBackend);
    testLogger.init(256);
    testLogger.enableLogging(true);
    
    // Log a message
    testLogger.log(ESP_LOG_INFO, "Test", "Backend test message");
    
    // Verify backend received the message
    TEST_ASSERT_EQUAL(1, mockBackend->getLogCount());
    
    auto logs = mockBackend->getLogs();
    TEST_ASSERT_TRUE(logs[0].find("Backend test message") != std::string::npos);
}

// Test backend respects log levels
void test_backend_log_levels() {
    auto mockBackend = std::make_shared<MockLogger>();
    Logger testLogger(mockBackend);
    testLogger.init(256);
    testLogger.setLogLevel(ESP_LOG_WARN);
    
    // Log at different levels
    testLogger.log(ESP_LOG_DEBUG, "Test", "Debug message");
    testLogger.log(ESP_LOG_INFO, "Test", "Info message");
    testLogger.log(ESP_LOG_WARN, "Test", "Warning message");
    testLogger.log(ESP_LOG_ERROR, "Test", "Error message");
    
    // Only WARN and ERROR should be logged
    TEST_ASSERT_EQUAL(2, mockBackend->getLogCount());
    
    auto logs = mockBackend->getLogs();
    TEST_ASSERT_TRUE(logs[0].find("Warning message") != std::string::npos);
    TEST_ASSERT_TRUE(logs[1].find("Error message") != std::string::npos);
}

// Test backend with direct mode
void test_backend_direct_mode() {
    auto mockBackend = std::make_shared<MockLogger>();
    Logger testLogger(mockBackend);
    testLogger.init(256);
    testLogger.setDirectMode(true);
    
    // Log in direct mode
    testLogger.log(ESP_LOG_INFO, "DirectTest", "Direct mode message");
    
    // Backend should still receive the message
    TEST_ASSERT_EQUAL(1, mockBackend->getLogCount());
    
    auto logs = mockBackend->getLogs();
    TEST_ASSERT_TRUE(logs[0].find("Direct mode message") != std::string::npos);
}

// Test backend formatting
void test_backend_formatting() {
    auto mockBackend = std::make_shared<MockLogger>();
    Logger testLogger(mockBackend);
    testLogger.init(256);
    
    // Log with formatting
    testLogger.log(ESP_LOG_INFO, "Format", "Number: %d, String: %s", 42, "test");
    
    TEST_ASSERT_EQUAL(1, mockBackend->getLogCount());
    
    auto logs = mockBackend->getLogs();
    // Check message contains expected formatting elements
    TEST_ASSERT_TRUE(logs[0].find("[I]") != std::string::npos); // Level
    TEST_ASSERT_TRUE(logs[0].find("Format:") != std::string::npos); // Tag
    TEST_ASSERT_TRUE(logs[0].find("Number: 42") != std::string::npos); // Formatted content
    TEST_ASSERT_TRUE(logs[0].find("String: test") != std::string::npos);
}

// Test multiple backends can be used
void test_backend_switching() {
    auto mock1 = std::make_shared<MockLogger>();
    auto mock2 = std::make_shared<MockLogger>();
    
    // Create logger with first backend
    Logger logger1(mock1);
    logger1.init(256);
    logger1.log(ESP_LOG_INFO, "Test", "Message to backend 1");
    
    // Create another logger with second backend
    Logger logger2(mock2);
    logger2.init(256);
    logger2.log(ESP_LOG_INFO, "Test", "Message to backend 2");
    
    // Verify each backend got its message
    TEST_ASSERT_EQUAL(1, mock1->getLogCount());
    TEST_ASSERT_EQUAL(1, mock2->getLogCount());
    
    TEST_ASSERT_TRUE(mock1->getLogs()[0].find("backend 1") != std::string::npos);
    TEST_ASSERT_TRUE(mock2->getLogs()[0].find("backend 2") != std::string::npos);
}

// Main test runner
void setup() {
    // Wait for serial
    Serial.begin(115200);
    delay(2000);
    
    UNITY_BEGIN();
    
    // Basic Logger Tests
    Serial.println("\n========== Basic Logger Tests ==========");
    RUN_TEST(test_singleton_instance);
    RUN_TEST(test_logger_basic);
    RUN_TEST(test_mutex_exists);
    RUN_TEST(test_enable_disable);
    RUN_TEST(test_log_levels);
    RUN_TEST(test_rate_limit_config);
    RUN_TEST(test_direct_mode);
    RUN_TEST(test_null_safety);
    RUN_TEST(test_context);
    RUN_TEST(test_tag_levels);
    
    // Backend Integration Tests
    Serial.println("\n========== Backend Integration Tests ==========");
    RUN_TEST(test_backend_receives_messages);
    RUN_TEST(test_backend_log_levels);
    RUN_TEST(test_backend_direct_mode);
    RUN_TEST(test_backend_formatting);
    RUN_TEST(test_backend_switching);
    
    UNITY_END();
}

void loop() {
    // Empty
}