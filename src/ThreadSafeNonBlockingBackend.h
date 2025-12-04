// ThreadSafeNonBlockingBackend.h
// Combines thread safety with non-blocking behavior
// Best of both SynchronizedConsoleBackend and NonBlockingConsoleBackend
#pragma once

#include "ILogBackend.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cstring>

/**
 * @brief Thread-safe non-blocking console backend
 *
 * Combines the best features of SynchronizedConsoleBackend and NonBlockingConsoleBackend:
 * - Thread-safe: Uses mutex to prevent message interleaving
 * - Non-blocking: Never waits for mutex or serial buffer
 * - Drops messages rather than blocking the calling task
 * - Tracks statistics for monitoring dropped messages
 *
 * This is the recommended backend for production systems with multiple logging tasks.
 *
 * Usage:
 *   auto backend = std::make_shared<ThreadSafeNonBlockingBackend>();
 *   logger.setBackend(backend);
 */
class ThreadSafeNonBlockingBackend : public ILogBackend {
private:
    static SemaphoreHandle_t writeMutex_;
    static bool mutexInitialized_;

    // Atomic counters for statistics (lock-free)
    std::atomic<uint32_t> droppedMessages_{0};
    std::atomic<uint32_t> droppedBytes_{0};
    std::atomic<uint32_t> mutexContention_{0};  // Times mutex was unavailable
    std::atomic<uint32_t> bufferFull_{0};        // Times buffer was too full

    // Configuration
    static constexpr size_t LOCAL_BUFFER_SIZE = 256;   // Per-write buffer
    static constexpr size_t MIN_BUFFER_SPACE = 20;     // Minimum serial buffer space needed
    static constexpr TickType_t MUTEX_TIMEOUT = 0;     // Zero timeout = non-blocking

    static void ensureMutexInitialized() {
        if (!mutexInitialized_) {
            writeMutex_ = xSemaphoreCreateMutex();
            mutexInitialized_ = (writeMutex_ != nullptr);
        }
    }

public:
    ThreadSafeNonBlockingBackend() {
        ensureMutexInitialized();
    }

    void write(const std::string& logMessage) override {
        write(logMessage.c_str(), logMessage.length());
    }

    void write(const char* logMessage, size_t length) override {
        if (!mutexInitialized_ || !logMessage || length == 0) return;

        // Copy message to local buffer first (ensures atomic composition)
        char localBuffer[LOCAL_BUFFER_SIZE];
        size_t copyLen = (length < LOCAL_BUFFER_SIZE - 1) ? length : LOCAL_BUFFER_SIZE - 1;
        memcpy(localBuffer, logMessage, copyLen);
        localBuffer[copyLen] = '\0';

        // Try to acquire mutex without blocking
        if (xSemaphoreTake(writeMutex_, MUTEX_TIMEOUT) != pdTRUE) {
            // Mutex busy - drop message rather than wait
            droppedMessages_.fetch_add(1, std::memory_order_relaxed);
            droppedBytes_.fetch_add(length, std::memory_order_relaxed);
            mutexContention_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        // Got mutex - check buffer space
        size_t available = Serial.availableForWrite();
        if (available < MIN_BUFFER_SPACE) {
            // Buffer too full - drop rather than block
            xSemaphoreGive(writeMutex_);
            droppedMessages_.fetch_add(1, std::memory_order_relaxed);
            droppedBytes_.fetch_add(length, std::memory_order_relaxed);
            bufferFull_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        // Write what fits
        size_t toWrite = (copyLen < available) ? copyLen : available;
        Serial.write(localBuffer, toWrite);

        // Track partial writes
        if (toWrite < length) {
            droppedBytes_.fetch_add(length - toWrite, std::memory_order_relaxed);
        }

        xSemaphoreGive(writeMutex_);
    }

    void flush() override {
        // NEVER call Serial.flush() - it blocks!
        // This is intentionally empty
    }

    // Statistics getters
    uint32_t getDroppedMessages() const {
        return droppedMessages_.load(std::memory_order_relaxed);
    }

    uint32_t getDroppedBytes() const {
        return droppedBytes_.load(std::memory_order_relaxed);
    }

    uint32_t getMutexContentionCount() const {
        return mutexContention_.load(std::memory_order_relaxed);
    }

    uint32_t getBufferFullCount() const {
        return bufferFull_.load(std::memory_order_relaxed);
    }

    void resetStats() {
        droppedMessages_.store(0, std::memory_order_relaxed);
        droppedBytes_.store(0, std::memory_order_relaxed);
        mutexContention_.store(0, std::memory_order_relaxed);
        bufferFull_.store(0, std::memory_order_relaxed);
    }

    // Check health
    bool isHealthy() const {
        // Consider unhealthy if significant drops
        return droppedMessages_.load(std::memory_order_relaxed) < 100;
    }

    // Print statistics (use carefully - direct Serial access)
    void printStats() {
        Serial.printf("\r\n=== ThreadSafeNonBlockingBackend Stats ===\r\n");
        Serial.printf("Dropped messages: %lu\r\n", getDroppedMessages());
        Serial.printf("Dropped bytes: %lu\r\n", getDroppedBytes());
        Serial.printf("Mutex contention: %lu\r\n", getMutexContentionCount());
        Serial.printf("Buffer full events: %lu\r\n", getBufferFullCount());
        Serial.printf("Buffer available: %u bytes\r\n", Serial.availableForWrite());
        Serial.printf("==========================================\r\n");
    }
};

// Static member definitions
// Note: These must be defined in a cpp file or made inline (C++17)
// For C++11 compatibility, define in SynchronizedConsoleBackend.cpp or similar
