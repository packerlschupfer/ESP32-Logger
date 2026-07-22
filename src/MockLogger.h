/*
 * MockLogger.h - part of the ESP32-Logger library
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

// MockLogger.h
#pragma once
#include "ILogBackend.h"
#include <vector>
#include <string>
#include <mutex>

/**
 * @brief Thread-safe mock logger for testing
 *
 * Captures all log messages in memory for verification in unit tests.
 * Thread-safe for use in multi-threaded test scenarios.
 */
class MockLogger : public ILogBackend {
private:
    std::vector<std::string> logMessages;
    mutable std::mutex mutex_;  // Protects logMessages

public:
    MockLogger() {}

    // ILogBackend interface
    void write(const std::string& logMessage) override {
        std::lock_guard<std::mutex> lock(mutex_);
        logMessages.push_back(logMessage);
    }

    void write(const char* logMessage, size_t length) override {
        std::lock_guard<std::mutex> lock(mutex_);
        logMessages.push_back(std::string(logMessage, length));
    }

    void flush() override {
        // Mock doesn't need to flush anything
    }

    // Mock-specific methods for testing

    /**
     * @brief Get a copy of all logs (thread-safe)
     * @return Copy of the log messages vector
     */
    std::vector<std::string> getLogs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return logMessages;  // Returns a copy for thread safety
    }

    /**
     * @brief Get the number of logged messages
     * @return Number of messages
     */
    size_t getLogCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return logMessages.size();
    }

    /**
     * @brief Clear all captured logs
     */
    void clearLogs() {
        std::lock_guard<std::mutex> lock(mutex_);
        logMessages.clear();
    }

    /**
     * @brief Check if any log contains the given substring
     * @param substr Substring to search for
     * @return true if found in any message
     */
    bool containsLog(const std::string& substr) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : logMessages) {
            if (msg.find(substr) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get the last logged message
     * @return Last message or empty string if none
     */
    std::string getLastLog() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (logMessages.empty()) return "";
        return logMessages.back();
    }
};
