// main.cpp - Example application using custom Logger
#include <Arduino.h>

// 1. Define USE_CUSTOM_LOGGER before including any libraries
#define USE_CUSTOM_LOGGER

// 2. Define LOG_TAG BEFORE including LogInterface for convenience macros (LOGI, etc.)
#define LOG_TAG "Main"

// 3. Include Logger and LogInterface
#include "Logger.h"
#include "ConsoleBackend.h"
#include "LogInterface.h"
// Note: LogInterfaceImpl.cpp is automatically compiled by the library when USE_CUSTOM_LOGGER is defined

// Test library that uses LogInterface
class TestLibrary {
public:
    void doWork() {
        LOG_INFO("TestLib", "Doing work with custom Logger");
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
    
    Serial.println("\n=== Custom Logger Example ===");
    
    printMemoryStats("Before any logging");
    
    // Use LogInterface before Logger init (will queue or drop based on implementation)
    LOG_INFO("Setup", "Early log before Logger init");
    
    printMemoryStats("After using LogInterface");
    
    // Initialize Logger explicitly
    Logger& logger = Logger::getInstance();
    logger.setBackend(std::make_shared<ConsoleBackend>());
    logger.init(1024);
    logger.setLogLevel(ESP_LOG_DEBUG);
    logger.enableLogging(true);
    
    printMemoryStats("After Logger init");
    
    // Now all logs go through custom Logger
    LOG_INFO("Setup", "Logger initialized");
    LOGI("Using convenience macro");
    
    // Test library usage
    TestLibrary lib;
    lib.doWork();
    
    printMemoryStats("After library usage");
}

void loop() {
    static uint32_t counter = 0;
    static unsigned long lastLog = 0;
    
    if (millis() - lastLog > 5000) {
        lastLog = millis();
        LOG_INFO("Loop", "Counter: %d, Free heap: %d", counter++, ESP.getFreeHeap());
    }
    
    delay(100);
}