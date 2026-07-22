/*
 * LogInterfaceImpl.cpp - part of the ESP32-Logger library
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

// LogInterfaceImpl.cpp - Implementation of LogInterface for custom Logger
// This file should be compiled once in the main application when using USE_CUSTOM_LOGGER

#ifdef USE_CUSTOM_LOGGER

#include "LogInterface.h"
#include "Logger.h"

extern "C" {

void custom_log_write(esp_log_level_t level, const char* tag, const char* format, va_list args) {
    // Use the Logger singleton with zero stack overhead
    Logger::getInstance().logV(level, tag, format, args);
}

bool custom_log_is_enabled(esp_log_level_t level) {
    // Quick global check first
    Logger& logger = Logger::getInstance();
    return logger.getIsLoggingEnabled() && (level <= logger.getLogLevel());
}

// Extended version that checks tag-specific levels
bool custom_log_is_enabled_for_tag(esp_log_level_t level, const char* tag) {
    return Logger::getInstance().isLevelEnabledForTag(tag, level);
}

} // extern "C"

#endif // USE_CUSTOM_LOGGER