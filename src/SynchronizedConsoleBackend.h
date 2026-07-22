/*
 * SynchronizedConsoleBackend.h - part of the ESP32-Logger library
 *
 * Copyright (C) 2025-2026 packerlschupfer
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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
    static std::atomic<SemaphoreHandle_t> serialMutex;
    static std::atomic<bool> mutexInitialized;

    static void ensureMutexInitialized() {
        // Double-checked locking with atomic to prevent race condition
        if (!mutexInitialized.load(std::memory_order_acquire)) {
            // Create mutex - if two threads race here, one will get nullptr
            // which is fine since we check before use
            SemaphoreHandle_t newMutex = xSemaphoreCreateMutex();
            if (newMutex) {
                // Try to set serialMutex atomically using std::atomic
                SemaphoreHandle_t expected = nullptr;
                if (serialMutex.compare_exchange_strong(expected, newMutex,
                                                         std::memory_order_seq_cst,
                                                         std::memory_order_seq_cst)) {
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
        if (!mutexInitialized.load() || !logMessage) return;

        SemaphoreHandle_t mutex = serialMutex.load();
        if (!mutex) return;

        // Take mutex with reasonable timeout
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_STANDARD_TIMEOUT_MS)) == pdTRUE) {
            // Write directly to Serial - already includes \r\n from Logger
            Serial.write(logMessage, length);
            Serial.flush(); // Ensure complete message is sent before releasing mutex
            xSemaphoreGive(mutex);
        }
    }

    void flush() override {
        // Serial.flush() can block for a long time
        // Only flush if we can get the mutex quickly
        if (!mutexInitialized.load()) return;

        SemaphoreHandle_t mutex = serialMutex.load();
        if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS / 2)) == pdTRUE) {
            Serial.flush();
            xSemaphoreGive(mutex);
        }
    }
};