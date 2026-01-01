// NonBlockingConsoleBackend.h
// Non-blocking console backend that prevents system freezes
// Drops messages rather than blocking when serial buffer is full
#pragma once

#include "ILogBackend.h"
#include <Arduino.h>
#include <atomic>
#include <inttypes.h>

/**
 * @brief Non-blocking console backend that prevents serial buffer blocking
 * 
 * This backend solves the critical blocking issue where Serial.print() can
 * freeze the system for 10+ seconds when the serial buffer is full.
 * 
 * Features:
 * - Never blocks - drops messages instead
 * - Tracks dropped messages and bytes
 * - Uses Serial.availableForWrite() to check buffer space
 * - NEVER calls Serial.flush() which blocks
 * - Adds truncation markers when messages are partially written
 * 
 * The ESP32 serial buffer is only 88 bytes by default, which fills up
 * almost instantly with normal logging. This backend ensures the system
 * remains responsive even under heavy logging load.
 */
class NonBlockingConsoleBackend : public ILogBackend {
private:
    // Atomic counters for thread safety
    std::atomic<uint32_t> droppedBytes{0};
    std::atomic<uint32_t> droppedMessages{0};
    std::atomic<uint32_t> partialWrites{0};
    
    // Configuration
    static constexpr size_t MIN_BUFFER_SPACE = 20;  // Don't write if less than this available
    static constexpr const char* TRUNCATION_MARKER = "...\r\n";
    static constexpr size_t TRUNCATION_MARKER_LEN = 5;
    
public:
    void write(const std::string& logMessage) override {
        write(logMessage.c_str(), logMessage.length());
    }
    
    void write(const char* logMessage, size_t length) override {
        if (!logMessage || length == 0) return;
        
        // Check available buffer space
        size_t available = Serial.availableForWrite();
        
        // If buffer is too full, drop the entire message
        if (available < MIN_BUFFER_SPACE) {
            droppedMessages.fetch_add(1);
            droppedBytes.fetch_add(length);
            return;
        }
        
        // Write what we can without blocking
        size_t written = 0;
        
        if (available >= length) {
            // Entire message fits
            written = Serial.write(logMessage, length);
        } else {
            // Partial write - leave room for truncation marker
            size_t toWrite = available - TRUNCATION_MARKER_LEN;
            if (toWrite > 0) {
                written = Serial.write(logMessage, toWrite);
                Serial.write(TRUNCATION_MARKER, TRUNCATION_MARKER_LEN);
                partialWrites.fetch_add(1);
            } else {
                // Not enough room even for truncated message
                droppedMessages.fetch_add(1);
                droppedBytes.fetch_add(length);
                return;
            }
        }
        
        // Track any unwritten bytes
        if (written < length) {
            droppedBytes.fetch_add(length - written);
        }
    }
    
    void flush() override {
        // CRITICAL: Do NOT call Serial.flush() - it blocks!
        // This is intentionally empty to prevent blocking
        // Data will be transmitted by hardware at its own pace
    }
    
    // Statistics methods
    uint32_t getDroppedMessages() const { 
        return droppedMessages.load(); 
    }
    
    uint32_t getDroppedBytes() const { 
        return droppedBytes.load(); 
    }
    
    uint32_t getPartialWrites() const {
        return partialWrites.load();
    }
    
    void resetStats() {
        droppedMessages.store(0);
        droppedBytes.store(0);
        partialWrites.store(0);
    }
    
    // Get current buffer status
    size_t getAvailableBuffer() const {
        return Serial.availableForWrite();
    }
    
    // Check if we're in a critical state (buffer nearly full)
    bool isBufferCritical() const {
        return getAvailableBuffer() < MIN_BUFFER_SPACE;
    }
    
    // Print statistics (useful for debugging)
    void printStats() {
        // Use direct Serial.printf to avoid recursion
        Serial.printf("\r\n=== NonBlockingConsoleBackend Stats ===\r\n");
        Serial.printf("Dropped messages: %" PRIu32 "\r\n", droppedMessages.load());
        Serial.printf("Dropped bytes: %" PRIu32 "\r\n", droppedBytes.load());
        Serial.printf("Partial writes: %" PRIu32 "\r\n", partialWrites.load());
        Serial.printf("Current buffer available: %u bytes\r\n", (unsigned int)Serial.availableForWrite());
        Serial.printf("=====================================\r\n");
    }
};