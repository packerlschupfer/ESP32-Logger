/*
 * LoggerConfig.h - part of the ESP32-Logger library
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

// LoggerConfig.h
// Configuration structure for Professional Logger
#pragma once

#include <esp_log.h>
#include <cstddef>

/**
 * @brief Logger configuration with static memory allocation
 * 
 * Designed for embedded systems with limited memory and 15-20 threads.
 * All allocations are static or pre-defined to avoid heap fragmentation.
 */
struct LoggerConfig {
    // Global settings
    esp_log_level_t defaultLevel = ESP_LOG_INFO;
    bool enableLogging = true;
    uint32_t maxLogsPerSecond = 100;  // 0 = unlimited
    
    // Backend configuration
    enum class BackendType {
        CONSOLE,                // Basic console output (BLOCKING - not recommended)
        SYNCHRONIZED_CONSOLE,   // Thread-safe console (BLOCKING - not recommended)
        NON_BLOCKING_CONSOLE,   // Non-blocking console (RECOMMENDED - prevents freezes)
        CUSTOM                  // User-provided backend
    };
    BackendType primaryBackend = BackendType::NON_BLOCKING_CONSOLE;  // Default to non-blocking
    
    // Tag-level configuration (static allocation)
    static constexpr size_t MAX_TAG_CONFIGS = 32;
    struct TagConfig {
        const char* tag;         // Tag name (must be static string)
        esp_log_level_t level;   // Log level for this tag
    };
    TagConfig tagConfigs[MAX_TAG_CONFIGS] = {};
    size_t tagConfigCount = 0;
    
    // Buffer pool configuration
    static constexpr size_t BUFFER_SIZE = 256;        // Size of each buffer
    static constexpr size_t BUFFER_COUNT = 8;         // Number of buffers in pool

    // Mutex timeout configuration (milliseconds)
    static constexpr uint32_t MUTEX_SHORT_TIMEOUT_MS = 10;    // Quick operations
    static constexpr uint32_t MUTEX_MEDIUM_TIMEOUT_MS = 50;   // Rate limit checks
    static constexpr uint32_t MUTEX_STANDARD_TIMEOUT_MS = 100; // Buffer pool, backend

    // Rate limit configuration
    static constexpr uint32_t RATE_LIMIT_WINDOW_MS = 1000;    // Rate limit window
    
    // Memory usage estimates
    static constexpr size_t estimatedMemoryUsage() {
        return sizeof(LoggerConfig) +                   // Config struct
               (BUFFER_SIZE * BUFFER_COUNT) +           // Buffer pool
               (MAX_TAG_CONFIGS * 32) +                 // Tag string storage
               1024;                                    // Logger internals
    }
    
    // Helper to add tag configuration
    bool addTagConfig(const char* tag, esp_log_level_t level) {
        if (tagConfigCount < MAX_TAG_CONFIGS && tag) {
            tagConfigs[tagConfigCount].tag = tag;
            tagConfigs[tagConfigCount].level = level;
            tagConfigCount++;
            return true;
        }
        return false;
    }
    
    // Predefined configurations for common use cases
    static LoggerConfig createMinimal() {
        LoggerConfig config;
        config.defaultLevel = ESP_LOG_WARN;
        config.maxLogsPerSecond = 50;
        config.primaryBackend = BackendType::NON_BLOCKING_CONSOLE;  // Always non-blocking
        return config;
    }
    
    static LoggerConfig createDevelopment() {
        LoggerConfig config;
        config.defaultLevel = ESP_LOG_INFO;
        config.maxLogsPerSecond = 0;  // Unlimited
        config.primaryBackend = BackendType::NON_BLOCKING_CONSOLE;  // Non-blocking for safety
        return config;
    }
    
    static LoggerConfig createProduction() {
        LoggerConfig config;
        config.defaultLevel = ESP_LOG_WARN;
        config.maxLogsPerSecond = 100;
        config.primaryBackend = BackendType::NON_BLOCKING_CONSOLE;  // Critical for production
        return config;
    }
};