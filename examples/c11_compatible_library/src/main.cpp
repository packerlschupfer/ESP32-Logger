/*
 * C++11 Compatible Library Example
 * 
 * This example demonstrates that LogInterface works perfectly
 * with C++11 and provides zero overhead when not using custom logger.
 * 
 * No C++17 features like __has_include are needed!
 */

#include <Arduino.h>

// Conditional logger setup
#ifdef USE_CUSTOM_LOGGER
    #include "Logger.h"
    // Note: LogInterfaceImpl.cpp is automatically compiled by the library
#endif

// Example library that uses LogInterface
#include "MyExampleLibrary.h"

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        delay(10);
    }
    
    Serial.println("\n========================================");
    Serial.println("C++11 Compatible LogInterface Example");
    Serial.println("========================================");
    
    #ifdef USE_CUSTOM_LOGGER
        Serial.println("Mode: CUSTOM LOGGER");
        Serial.println("Expected: ~17KB memory overhead");
        
        // Initialize custom logger
        Logger& logger = Logger::getInstance();
        logger.init(1024);
        logger.setLogLevel(ESP_LOG_VERBOSE);
        logger.enableLogging(true);
    #else
        Serial.println("Mode: ESP-IDF LOGGING");
        Serial.println("Expected: Zero memory overhead");
    #endif
    
    Serial.println("========================================\n");
    
    // Measure memory before creating library
    uint32_t heapBefore = ESP.getFreeHeap();
    
    // Create and use library
    MyExampleLibrary lib;
    lib.begin();
    lib.doWork();
    lib.simulateError(42);
    
    // Measure memory after
    uint32_t heapAfter = ESP.getFreeHeap();
    
    Serial.println("\n========================================");
    Serial.println("Memory Usage:");
    Serial.printf("Before: %lu bytes\n", (unsigned long)heapBefore);
    Serial.printf("After:  %lu bytes\n", (unsigned long)heapAfter);
    Serial.printf("Used:   %lu bytes\n", (unsigned long)(heapBefore - heapAfter));
    Serial.println("========================================");
    
    #ifdef USE_CUSTOM_LOGGER
        Serial.println("\nWith custom logger, memory usage includes:");
        Serial.println("- Logger singleton (~17KB)");
        Serial.println("- Library instance");
    #else
        Serial.println("\nWithout custom logger:");
        Serial.println("- No Logger singleton created!");
        Serial.println("- Only library instance memory");
    #endif
}

void loop() {
    static unsigned long lastLog = 0;
    
    if (millis() - lastLog > 5000) {
        lastLog = millis();
        
        MyExampleLibrary lib;
        lib.periodicStatus();
        
        Serial.printf("[%lu sec] Free heap: %lu bytes\n",
                      millis() / 1000,
                      (unsigned long)ESP.getFreeHeap());
    }
    
    delay(100);
}