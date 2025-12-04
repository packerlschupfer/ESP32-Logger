// SynchronizedConsoleBackend.h
#pragma once

#include "ILogBackend.h"
#include "LoggerConfig.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

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
    static std::atomic<bool> mutexInitialized;

    static void ensureMutexInitialized() {
        // Double-checked locking with atomic to prevent race condition
        if (!mutexInitialized.load(std::memory_order_acquire)) {
            // Create mutex - if two threads race here, one will get nullptr
            // which is fine since we check before use
            SemaphoreHandle_t newMutex = xSemaphoreCreateMutex();
            if (newMutex) {
                // Try to set serialMutex atomically
                SemaphoreHandle_t expected = nullptr;
                if (__atomic_compare_exchange_n(&serialMutex, &expected, newMutex,
                                                 false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                    mutexInitialized.store(true, std::memory_order_release);
                } else {
                    // Another thread won the race, delete our mutex
                    vSemaphoreDelete(newMutex);
                }
            }
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