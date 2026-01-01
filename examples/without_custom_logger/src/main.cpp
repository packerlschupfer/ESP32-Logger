// main.cpp - Example using ESP-IDF logging (no custom Logger)
#include <Arduino.h>

// Define LOG_TAG BEFORE including LogInterface for convenience macros (LOGI, etc.)
#define LOG_TAG "Main"

// Include LogInterface WITHOUT defining USE_CUSTOM_LOGGER
// This routes logs directly to ESP-IDF logging
#include "LogInterface.h"

// Test library that uses LogInterface
class TestLibrary {
public:
    void doWork() {
        LOG_INFO("TestLib", "Doing work with ESP-IDF logging");
        LOG_DEBUG("TestLib", "Debug info: %d", 42);
    }
};

void printMemoryStats(const char* label) {
    Serial.printf("%s - Free heap: %d, Min free: %d\n", 
                  label, ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== ESP-IDF Logging Example (No Custom Logger) ===");
    
    printMemoryStats("Before any logging");
    
    // Use LogInterface - goes directly to ESP-IDF
    LOG_INFO("Setup", "Using ESP-IDF logging");
    LOGI("No Logger singleton created!");
    
    printMemoryStats("After using LogInterface");
    
    // Test library usage
    TestLibrary lib;
    lib.doWork();
    
    printMemoryStats("After library usage");
    
    // Set ESP-IDF log level
    esp_log_level_set("*", ESP_LOG_DEBUG);
    LOG_DEBUG("Setup", "Debug logging enabled");
    
    // Memory should remain stable - no Logger instantiation
    Serial.println("\nNotice: Memory usage remains low - no Logger singleton!");
}

void loop() {
    static uint32_t counter = 0;
    static unsigned long lastLog = 0;
    
    if (millis() - lastLog > 5000) {
        lastLog = millis();
        LOG_INFO("Loop", "Counter: %d, Free heap: %d", counter++, ESP.getFreeHeap());
        
        // This should show consistent memory - no Logger overhead
        Serial.println("No Logger singleton = ~17KB memory saved!");
    }
    
    delay(100);
}