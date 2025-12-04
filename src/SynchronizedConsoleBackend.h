// SynchronizedConsoleBackend.h
#pragma once

#include "ILogBackend.h"
#include "LoggerConfig.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief Console backend with thread synchronization for multi-threaded environments
 * 
 * This backend ensures that log messages from multiple threads don't interleave
 * by using a mutex to serialize console output. Optimized for systems with 15-20 threads.
 * 
 * Features:
 * - Thread-safe console output
 * - Minimal overhead (single static mutex)
 * - No dynamic memory allocation
 * - Proper line ending handling (\r\n)
 */
class SynchronizedConsoleBackend : public ILogBackend {
private:
    static SemaphoreHandle_t serialMutex;
    static bool mutexInitialized;
    
    static void ensureMutexInitialized() {
        if (!mutexInitialized) {
            serialMutex = xSemaphoreCreateMutex();
            mutexInitialized = (serialMutex != nullptr);
        }
    }
    
public:
    SynchronizedConsoleBackend() {
        ensureMutexInitialized();
    }
    
    void write(const std::string& logMessage) override {
        write(logMessage.c_str(), logMessage.length());
    }
    
    void write(const char* logMessage, size_t length) override {
        if (!mutexInitialized || !logMessage) return;
        
        // Take mutex with reasonable timeout
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_STANDARD_TIMEOUT_MS)) == pdTRUE) {
            // Write directly to Serial - already includes \r\n from Logger
            Serial.write(logMessage, length);
            Serial.flush(); // Ensure complete message is sent before releasing mutex
            xSemaphoreGive(serialMutex);
        }
    }

    void flush() override {
        // Serial.flush() can block for a long time
        // Only flush if we can get the mutex quickly
        if (mutexInitialized && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS / 2)) == pdTRUE) {
            Serial.flush();
            xSemaphoreGive(serialMutex);
        }
    }
};