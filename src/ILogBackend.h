/*
 * ILogBackend.h - part of the ESP32-Logger library
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

// ILogBackend.h
#pragma once

#include <string>

class ILogBackend {
public:
    virtual ~ILogBackend() {}
    virtual void write(const std::string& logMessage) = 0;
    // Memory-efficient overload that avoids string allocation
    virtual void write(const char* logMessage, size_t length) = 0;
    // Flush any buffered output
    virtual void flush() = 0;
};

