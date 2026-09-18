/*
 * DeferredLog.h - part of the ESP32-Logger library
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

// DeferredLog.h - Deferred formatting ring (LOG_DEFERRED_FORMAT)
//
// Moves printf formatting off the calling task's stack. The caller walks the
// format string with a small hand-written parser, copies the raw argument
// values (and %s contents) into a ring entry, and returns. The logger task
// later formats each conversion with snprintf on its own stack.
//
// Why: newlib's formatter chain costs ~1 kB of stack on every task that logs
// (_svfprintf_r alone is an 800 B frame). With deferral the caller pays only
// the walker and the ring reservation.
//
// The ring lives in RTC slow memory (RTC_NOINIT_ATTR) by default: it costs no
// DRAM and survives panic, watchdog and software resets, so entries that were
// still queued at a crash are replayed on the next boot as "[pre-reset]".
// Entries are only replayed when the app ELF SHA matches, because stored
// format pointers point into the flash image that wrote them.
//
// Type safety relies on the format string: log_write_impl carries
// __attribute__((format(printf, 3, 4))), so the compiler checks that every
// LOG_* call's arguments match its conversions, which is exactly the
// information the walker uses to pull them off the va_list.

#pragma once

#ifdef LOG_DEFERRED_FORMAT

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_log.h>

// Ring capacity in bytes. A typical entry (tag + 0-3 args) is 40-64 bytes.
#ifndef CONFIG_LOG_DEFERRED_RING_SIZE
#define CONFIG_LOG_DEFERRED_RING_SIZE 2048
#endif

// Maximum bytes copied per %s argument (longer strings are truncated)
#ifndef CONFIG_LOG_DEFERRED_MAX_STRING
#define CONFIG_LOG_DEFERRED_MAX_STRING 128
#endif

// Start the logger task automatically: when the Logger is constructed, or on the
// first log call once the FreeRTOS scheduler is running. Without it nothing is
// ever printed until the application calls Logger::startLogTask().
#ifndef CONFIG_LOG_DEFERRED_AUTOSTART
#define CONFIG_LOG_DEFERRED_AUTOSTART 1
#endif

// Core for the auto-started logger task (-1 = no affinity). Pin it to the core
// your subscribers need (e.g. the network core for Syslog): a later
// startLogTask(core) call cannot move an already running task.
#ifndef CONFIG_LOG_DEFERRED_TASK_CORE
#define CONFIG_LOG_DEFERRED_TASK_CORE -1
#endif

// Place the ring in RTC slow memory (1) or in no-init DRAM (0)
#ifndef CONFIG_LOG_DEFERRED_RING_IN_RTC
#define CONFIG_LOG_DEFERRED_RING_IN_RTC 1
#endif

namespace DeferredLog {

// Entry flags
enum : uint8_t {
    FLAG_NO_NEWLINE = 0x01,  // logNnL: header, no trailing CRLF
    FLAG_NO_HEADER  = 0x02,  // logInL: raw message only
};

static constexpr size_t TAG_MAX = 31;
static constexpr size_t TASK_NAME_MAX = 16;

// Decoded entry handed to the logger task (owned copies, the ring slot is
// already released when this is returned)
struct Record {
    esp_log_level_t level;
    uint8_t flags;
    bool preReset;          // written before the last reset
    uint32_t timestamp;     // millis at capture time
    char tag[TAG_MAX + 1];
    char task[TASK_NAME_MAX];
};

enum class PopResult {
    EMPTY,  // nothing queued
    BUSY,   // oldest entry is still being written by its producer
    OK,     // record and message filled in
};

/**
 * @brief Validate the ring after a reset and prepare it for use
 * @note Idempotent. Called from the Logger constructor.
 */
void init();

/**
 * @brief Capture a log call into the ring (runs on the calling task)
 * @return false if the entry did not fit (counted in overflowCount())
 */
bool push(esp_log_level_t level, const char* tag, const char* format, va_list args, uint8_t flags);

/**
 * @brief Format and remove the oldest entry (runs on the logger task only)
 * @param skipBusy Discard an entry that is stuck in the writing state
 *                 (its producer was deleted mid-write)
 */
PopResult pop(Record& record, char* message, size_t messageSize, bool skipBusy);

/** @brief true if nothing is queued */
bool empty();

/** @brief Entries dropped because the ring was full or an entry was too large */
uint32_t overflowCount();

/** @brief Complete entries found in the ring at boot (before this reset) */
uint32_t recoveredCount();

/** @brief Incomplete entries found at boot (producer interrupted by the reset) */
uint32_t recoveredIncompleteCount();

/** @brief false if the ring was invalid at boot (power-on, brownout, new firmware) */
bool ringSurvivedReset();

} // namespace DeferredLog

#endif // LOG_DEFERRED_FORMAT
