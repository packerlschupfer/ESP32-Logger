/*
 * ILogger.h - part of the ESP32-Logger library
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

// ILogger.h
#pragma once

#include <esp_log.h> // Include the ESP-IDF logging header

class ILogger {
public:
    virtual ~ILogger() {}

    /**
     * @brief Logs a message.
     * 
     * @param level The log level of the message.
     * @param tag The tag associated with the log message.
     * @param format The format string for the log message, followed by arguments.
     */
    virtual void log(esp_log_level_t level, const char* tag, const char* format, ...) = 0;

    /**
     * @brief Logs a message with a newline appended.
     * 
     * @param format The format string for the log message, followed by arguments.
     */
    virtual void logInL(const char* format, ...) = 0;

    /**
     * @brief Logs a message without a newline appended.
     * 
     * @param level The log level of the message.
     * @param tag The tag associated with the log message.
     * @param format The format string for the log message, followed by arguments.
     */
    virtual void logNnL(esp_log_level_t level, const char* tag, const char* format, ...) = 0;

    /**
     * @brief Logs a formatted message using a variable argument list.
     *
     * @param level The log level of the message.
     * @param tag The tag associated with the log message.
     * @param format The format string for the log message.
     * @param args A va_list of arguments for the format string.
     */
    virtual void logV(esp_log_level_t level, const char* tag, const char* format, va_list args) = 0;

    virtual void setLogLevel(esp_log_level_t level) = 0;
    virtual esp_log_level_t getLogLevel() const = 0;
    
    /**
     * @brief Flush any buffered log output.
     */
    virtual void flush() = 0;
};
