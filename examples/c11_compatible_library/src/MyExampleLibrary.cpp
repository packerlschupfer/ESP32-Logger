/*
 * MyExampleLibrary Implementation
 * 
 * Uses LogInterface.h for zero-overhead logging.
 * This is C++11 compatible - no C++17 features needed!
 */

#define LOG_TAG "MyExampleLib"
#include "LogInterface.h"  // NOT Logger.h!
#include "MyExampleLibrary.h"
#include <Arduino.h>

MyExampleLibrary::MyExampleLibrary() : counter(0), startTime(0) {
    LOGV("Constructor called");
}

void MyExampleLibrary::begin() {
    LOGI("Initializing library...");
    
    startTime = millis();
    counter = 0;
    
    // Simulate some initialization work
    LOGD("Setting up internal state");
    LOGD("Configuring parameters");
    LOGV("Verbose: Initial counter value = %d", counter);
    
    LOGI("Library initialized successfully");
}

void MyExampleLibrary::doWork() {
    LOGI("Starting work cycle %d", ++counter);
    
    // Simulate different log levels
    LOGV("Verbose: Processing step 1");
    LOGD("Debug: Current timestamp = %lu ms", millis());
    
    // Simulate work with progress
    for (int i = 0; i < 5; i++) {
        LOGV("Processing item %d of 5", i + 1);
        delay(10);
    }
    
    LOGI("Work cycle %d completed", counter);
}

void MyExampleLibrary::simulateError(int errorCode) {
    LOGW("Warning: Unusual condition detected");
    LOGE("Error occurred with code: %d (0x%02X)", errorCode, errorCode);
    
    // Show that formatting works
    LOGE("Extended error info: code=%d, counter=%d, uptime=%lu ms", 
         errorCode, counter, millis() - startTime);
}

void MyExampleLibrary::periodicStatus() {
    unsigned long uptime = millis() - startTime;
    
    LOGI("Status: counter=%d, uptime=%lu sec, avg rate=%.2f/sec",
         counter, 
         uptime / 1000,
         counter > 0 ? (float)counter / (uptime / 1000.0f) : 0.0f);
}