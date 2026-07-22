/*
 * Stack Limit Test
 * 
 * This program tests the exact parameter limits for logging
 * and measures how different parameter types affect stack usage.
 */

#include <Arduino.h>

// Conditional compilation for logging
#ifndef USE_CUSTOM_LOGGER
    // ESP-IDF logging
    #include "esp_log.h"
    #define LOG_TAG "StackTest"
    #define LOG_INFO(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
    #define LOG_ERROR(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
    #define LOGGER_NAME "ESP-IDF"
#else
    // Custom Logger
    #include "Logger.h"
    #define LOG_TAG "StackTest"
    #define LOG_INFO(tag, ...) Logger::getInstance().log(ESP_LOG_INFO, tag, __VA_ARGS__)
    #define LOG_ERROR(tag, ...) Logger::getInstance().log(ESP_LOG_ERROR, tag, __VA_ARGS__)
    #define LOGGER_NAME "Custom Logger"
#endif

void printMemory(const char* label) {
    Serial.printf("%s - Free: %lu, Min: %lu\n", 
                  label,
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMinFreeHeap());
}

void testIntegers() {
    Serial.println("\n=== Testing Integer Parameters ===");
    
    // Test increasing numbers of integer parameters
    Serial.println("5 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d", 1, 2, 3, 4, 5);
    delay(100);
    
    Serial.println("10 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d %d %d %d %d %d", 
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    delay(100);
    
    Serial.println("15 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    delay(100);
    
    Serial.println("20 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20);
    delay(100);
    
    Serial.println("25 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25);
    delay(100);
    
    Serial.println("26 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26);
    delay(100);
    
    Serial.println("27 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27);
    delay(100);
    
    Serial.println("28 integers:");
    LOG_INFO(LOG_TAG, "Int test: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28);
    
    Serial.println("Integer test complete!");
}

void testFloats() {
    Serial.println("\n=== Testing Float Parameters ===");
    
    Serial.println("5 floats:");
    LOG_INFO(LOG_TAG, "Float test: %.2f %.2f %.2f %.2f %.2f", 
             1.1f, 2.2f, 3.3f, 4.4f, 5.5f);
    delay(100);
    
    Serial.println("10 floats:");
    LOG_INFO(LOG_TAG, "Float test: %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
             1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f, 9.9f, 10.1f);
    delay(100);
    
    Serial.println("15 floats:");
    LOG_INFO(LOG_TAG, "Float test: %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
             1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f, 9.9f, 10.1f,
             11.1f, 12.2f, 13.3f, 14.4f, 15.5f);
    delay(100);
    
    Serial.println("20 floats:");
    LOG_INFO(LOG_TAG, "Float test: %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
             1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f, 9.9f, 10.1f,
             11.1f, 12.2f, 13.3f, 14.4f, 15.5f, 16.6f, 17.7f, 18.8f, 19.9f, 20.2f);
    delay(100);
    
    Serial.println("25 floats:");
    LOG_INFO(LOG_TAG, "Float test: %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
             1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f, 9.9f, 10.1f,
             11.1f, 12.2f, 13.3f, 14.4f, 15.5f, 16.6f, 17.7f, 18.8f, 19.9f, 20.2f,
             21.1f, 22.2f, 23.3f, 24.4f, 25.5f);
    
    Serial.println("Float test complete!");
}

void testStrings() {
    Serial.println("\n=== Testing String Parameters ===");
    
    const char* str = "test";
    
    Serial.println("5 strings:");
    LOG_INFO(LOG_TAG, "String test: %s %s %s %s %s", 
             str, str, str, str, str);
    delay(100);
    
    Serial.println("10 strings:");
    LOG_INFO(LOG_TAG, "String test: %s %s %s %s %s %s %s %s %s %s",
             str, str, str, str, str, str, str, str, str, str);
    delay(100);
    
    Serial.println("15 strings:");
    LOG_INFO(LOG_TAG, "String test: %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
             str, str, str, str, str, str, str, str, str, str,
             str, str, str, str, str);
    delay(100);
    
    Serial.println("20 strings:");
    LOG_INFO(LOG_TAG, "String test: %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
             str, str, str, str, str, str, str, str, str, str,
             str, str, str, str, str, str, str, str, str, str);
    
    Serial.println("String test complete!");
}

void testMixed() {
    Serial.println("\n=== Testing Mixed Parameters ===");
    
    // Test with realistic mixed parameters
    Serial.println("Mixed 10 params (int, float, string):");
    LOG_INFO(LOG_TAG, "Mixed: i=%d f=%.2f s=%s i=%d f=%.2f s=%s i=%d f=%.2f s=%s p=%p",
             42, 3.14f, "hello", 100, 2.71f, "world", 200, 1.41f, "test", (void*)0x1234);
    delay(100);
    
    Serial.println("Mixed 15 params:");
    LOG_INFO(LOG_TAG, "Mixed: %d %.2f %s %d %.2f %s %d %.2f %s %p %d %.2f %s %c %lu",
             1, 1.1f, "a", 2, 2.2f, "b", 3, 3.3f, "c", (void*)0x1234,
             4, 4.4f, "d", 'X', 1000UL);
    delay(100);
    
    Serial.println("Mixed 20 params:");
    LOG_INFO(LOG_TAG, "Mixed: %d %.2f %s %d %.2f %s %d %.2f %s %p %d %.2f %s %c %lu %d %.2f %s %p %x",
             1, 1.1f, "a", 2, 2.2f, "b", 3, 3.3f, "c", (void*)0x1234,
             4, 4.4f, "d", 'X', 1000UL, 5, 5.5f, "e", (void*)0x5678, 0xABCD);
    delay(100);
    
    Serial.println("Mixed 25 params:");
    LOG_INFO(LOG_TAG, "Mixed: %d %.2f %s %d %.2f %s %d %.2f %s %p %d %.2f %s %c %lu %d %.2f %s %p %x %d %.2f %s %ld %llu",
             1, 1.1f, "a", 2, 2.2f, "b", 3, 3.3f, "c", (void*)0x1234,
             4, 4.4f, "d", 'X', 1000UL, 5, 5.5f, "e", (void*)0x5678, 0xABCD,
             6, 6.6f, "f", -999L, 12345ULL);
    
    Serial.println("Mixed test complete!");
}

void testLongStrings() {
    Serial.println("\n=== Testing Long String Impact ===");
    
    const char* short_str = "Hi";
    const char* medium_str = "Hello World";
    const char* long_str = "The quick brown fox jumps over the lazy dog";
    
    Serial.println("10 params with short strings:");
    LOG_INFO(LOG_TAG, "Short: %s %s %s %s %s %s %s %s %s %s",
             short_str, short_str, short_str, short_str, short_str,
             short_str, short_str, short_str, short_str, short_str);
    delay(100);
    
    Serial.println("10 params with medium strings:");
    LOG_INFO(LOG_TAG, "Medium: %s %s %s %s %s %s %s %s %s %s",
             medium_str, medium_str, medium_str, medium_str, medium_str,
             medium_str, medium_str, medium_str, medium_str, medium_str);
    delay(100);
    
    Serial.println("10 params with long strings:");
    LOG_INFO(LOG_TAG, "Long: %s %s %s %s %s %s %s %s %s %s",
             long_str, long_str, long_str, long_str, long_str,
             long_str, long_str, long_str, long_str, long_str);
    
    Serial.println("String length test complete!");
}

void testComplexFormats() {
    Serial.println("\n=== Testing Complex Format Specifiers ===");
    
    Serial.println("10 params with simple formats:");
    LOG_INFO(LOG_TAG, "Simple: %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    delay(100);
    
    Serial.println("10 params with width/precision:");
    LOG_INFO(LOG_TAG, "Complex: %03d %05u %+8ld %-10lu %#x %10.2f %15.6f %.10e %20s %*d",
             1, 2U, 3L, 4UL, 0xFF, 5.5f, 6.666666, 7.777777, "formatted", 8, 9);
    delay(100);
    
    Serial.println("20 params with simple formats:");
    LOG_INFO(LOG_TAG, "Simple: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20);
    delay(100);
    
    Serial.println("20 params with width/precision:");
    LOG_INFO(LOG_TAG, "Complex: %03d %05u %+8ld %-10lu %#x %10.2f %15.6f %.10e %20s %*d %03d %05u %+8ld %-10lu %#x %10.2f %15.6f %.10e %20s %*d",
             1, 2U, 3L, 4UL, 0xFF, 5.5f, 6.666666, 7.777777, "fmt", 8, 9,
             11, 12U, 13L, 14UL, 0xAA, 15.5f, 16.666666, 17.777777, "str", 18, 19);
    
    Serial.println("Complex format test complete!");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        delay(10);
    }
    
    Serial.println("\n\n========================================");
    Serial.println("    STACK LIMIT DETAILED TEST");
    Serial.println("========================================");
    Serial.printf("Logger: %s\n", LOGGER_NAME);
    Serial.printf("ESP32 Total Heap: %lu bytes\n", (unsigned long)ESP.getHeapSize());
    Serial.println("========================================");
    
    #ifdef USE_CUSTOM_LOGGER
    // Initialize custom logger
    Logger& logger = Logger::getInstance();
    logger.init(1024);
    logger.setLogLevel(ESP_LOG_VERBOSE);
    logger.enableLogging(true);
    #endif
    
    printMemory("Initial state");
    
    // Run tests
    testIntegers();
    testFloats();
    testStrings();
    testMixed();
    testLongStrings();
    testComplexFormats();
    
    Serial.println("\n========================================");
    Serial.println("Test completed successfully!");
    Serial.println("If you see this, no stack overflow occurred.");
    Serial.println("========================================");
    
    printMemory("Final state");
}

void loop() {
    static unsigned long lastReport = 0;
    
    if (millis() - lastReport > 10000) {
        lastReport = millis();
        Serial.printf("\n[%lu sec] Still running - Free heap: %lu\n", 
                      millis() / 1000, (unsigned long)ESP.getFreeHeap());
    }
    
    delay(100);
}