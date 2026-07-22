/*
 * ThreadSafeNonBlockingBackend.cpp - part of the ESP32-Logger library
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

// ThreadSafeNonBlockingBackend.cpp
#include "ThreadSafeNonBlockingBackend.h"

// Static member initialization
std::atomic<SemaphoreHandle_t> ThreadSafeNonBlockingBackend::writeMutex_{nullptr};
std::atomic<bool> ThreadSafeNonBlockingBackend::mutexInitialized_{false};
