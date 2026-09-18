/*
 * DeferredLog.cpp - part of the ESP32-Logger library
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

// DeferredLog.cpp - Deferred formatting ring (LOG_DEFERRED_FORMAT)
//
// Ring layout: a byte ring of variable-length entries, each starting with a
// 32-bit control word  [magic:8][state:8][length:16].  Producers reserve an
// entry under a spinlock (control word = WRITING), fill it outside the lock,
// then publish it with a single release store (control word = READY). The
// logger task consumes in reservation order. An entry never wraps; when the
// space left before the end is too small, a PAD entry fills it.
//
// Entry layout (byte offsets, all fields read/written with memcpy):
//   0  control word
//   4  uint8  level
//   5  uint8  flags (| FLAG_FORMAT_INLINE)
//   6  uint16 offset of the argument stream
//   8  uint32 timestamp (ms)
//  12  uint32 format pointer (flash) - unused when the format is inline
//  16  char   task[16]
//  32  tag, NUL   [format, NUL - only when the format is not in flash]
//  ..  argument stream: per conversion, in order: '*' ints (4 B each), then
//      int32 (4) | int64 (8) | double (8) | pointer (4) |
//      string: uint16 length (0xFFFF = NULL), bytes, NUL

#ifdef LOG_DEFERRED_FORMAT

#include "DeferredLog.h"
#include <atomic>
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <esp_attr.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if __has_include(<esp_app_desc.h>)
#include <esp_app_desc.h>
#define DEFERRED_HAVE_APP_DESC 1
#endif

#if __has_include(<esp_memory_utils.h>)
#include <esp_memory_utils.h>
#define DEFERRED_HAVE_MEMORY_UTILS 1
#endif

namespace DeferredLog {
namespace {

constexpr uint32_t RING_SIZE = (CONFIG_LOG_DEFERRED_RING_SIZE + 3u) & ~3u;
constexpr uint32_t RING_MAGIC = 0x4C4F4752;  // "LOGR"
constexpr uint32_t ENTRY_MAGIC = 0xA7000000;
constexpr uint32_t ST_WRITING = 1;
constexpr uint32_t ST_READY = 2;
constexpr uint32_t ST_PAD = 3;
constexpr uint32_t HEADER_SIZE = 32;
constexpr uint32_t MAX_ENTRY = (RING_SIZE / 4 < 1024) ? ((RING_SIZE / 4) & ~3u) : 1024;
constexpr size_t FORMAT_INLINE_MAX = 160;
constexpr size_t STRING_LIMIT_FALLBACK = 16;
constexpr uint16_t NULL_STRING = 0xFFFF;
constexpr uint8_t FLAG_FORMAT_INLINE = 0x80;

static_assert(RING_SIZE >= 256, "CONFIG_LOG_DEFERRED_RING_SIZE too small");
static_assert(RING_SIZE <= 65532, "CONFIG_LOG_DEFERRED_RING_SIZE must fit the 16-bit entry length");
#ifdef DEFERRED_HAVE_MEMORY_UTILS
// Flash format pointers are stored in a 4-byte header field
static_assert(sizeof(const char*) == 4, "format pointer field is 4 bytes");
#endif
static_assert(TASK_NAME_MAX >= configMAX_TASK_NAME_LEN, "task name field too small");

struct Ring {
    uint32_t magic;
    uint32_t size;
    uint8_t elfSha[16];
    uint32_t head;  // next reservation offset (producers, under s_mux)
    uint32_t tail;  // oldest entry (logger task only)
    uint8_t data[RING_SIZE] __attribute__((aligned(4)));
};

// Survives panic / watchdog / software reset; not power-on or brownout
#if CONFIG_LOG_DEFERRED_RING_IN_RTC
RTC_NOINIT_ATTR Ring s_ring;
#else
__NOINIT_ATTR Ring s_ring;
#endif

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
bool s_initialized = false;
bool s_survived = false;
uint32_t s_recovered = 0;
uint32_t s_incomplete = 0;
uint32_t s_preResetPending = 0;  // logger task only
std::atomic<uint32_t> s_overflow{0};

// ---------------------------------------------------------------------------
// Format string parsing (shared by producer and consumer)
// ---------------------------------------------------------------------------

enum Kind : uint8_t { K_PERCENT, K_I32, K_I64, K_DOUBLE, K_PTR, K_STR, K_BAD };

struct Spec {
    uint8_t length;      // '%' through the conversion character
    uint8_t stars;       // '*' width/precision arguments
    bool hasPrecision;
    bool starPrecision;
    int precision;       // numeric precision when !starPrecision
    char lengthMod;      // 'H' hh, 'h', 'l', 'L' ll/q, 'j', 'z', 't', 'D' long double
    Kind kind;
};

Kind integerKind(char lengthMod) {
    switch (lengthMod) {
        case 'L': return K_I64;
        case 'l': return sizeof(long) == 8 ? K_I64 : K_I32;
        case 'j': return sizeof(intmax_t) == 8 ? K_I64 : K_I32;
        case 'z': return sizeof(size_t) == 8 ? K_I64 : K_I32;
        case 't': return sizeof(ptrdiff_t) == 8 ? K_I64 : K_I32;
        case 'D': return K_BAD;
        default:  return K_I32;
    }
}

// p points at '%'
Spec parseSpec(const char* p) {
    Spec s = {};
    const char* c = p + 1;

    while (*c == '-' || *c == '+' || *c == ' ' || *c == '#' || *c == '0' || *c == '\'') c++;

    if (*c == '*') {
        s.stars++;
        c++;
    } else {
        while (*c >= '0' && *c <= '9') c++;
    }

    if (*c == '.') {
        c++;
        s.hasPrecision = true;
        if (*c == '*') {
            s.stars++;
            s.starPrecision = true;
            c++;
        } else {
            while (*c >= '0' && *c <= '9') {
                if (s.precision < 100000) s.precision = s.precision * 10 + (*c - '0');
                c++;
            }
        }
    }

    switch (*c) {
        case 'h': c++; if (*c == 'h') { s.lengthMod = 'H'; c++; } else { s.lengthMod = 'h'; } break;
        case 'l': c++; if (*c == 'l') { s.lengthMod = 'L'; c++; } else { s.lengthMod = 'l'; } break;
        case 'q': s.lengthMod = 'L'; c++; break;
        case 'j': case 'z': case 't': s.lengthMod = *c; c++; break;
        case 'L': s.lengthMod = 'D'; c++; break;
        default: break;
    }

    const char conv = *c;
    const ptrdiff_t length = conv ? (c + 1 - p) : (c - p);
    s.length = static_cast<uint8_t>(length > 255 ? 255 : length);

    switch (conv) {
        case '%':
            s.kind = K_PERCENT;
            break;
        case 'd': case 'i': case 'u': case 'o': case 'x': case 'X':
            s.kind = integerKind(s.lengthMod);
            break;
        case 'c':
            s.kind = (s.lengthMod == 'l') ? K_BAD : K_I32;
            break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
            s.kind = (s.lengthMod == 'D') ? K_BAD : K_DOUBLE;
            break;
        case 's':
            s.kind = (s.lengthMod == 'l') ? K_BAD : K_STR;
            break;
        case 'p':
            s.kind = K_PTR;
            break;
        default:  // 'n', wide/unknown conversions, truncated format
            s.kind = K_BAD;
            break;
    }
    if (length > 255) s.kind = K_BAD;
    return s;
}

// ---------------------------------------------------------------------------
// Producer side (runs on the calling task - keep frames small)
// ---------------------------------------------------------------------------

inline void put(uint8_t* dst, size_t& n, size_t capacity, const void* src, size_t size) {
    if (dst && n + size <= capacity) memcpy(dst + n, src, size);
    n += size;
}

// Pulls every argument named by format off args. With dst == nullptr it only
// measures; otherwise it serializes into dst[0..capacity). Stops at the first
// unsupported conversion (the consumer stops at the same place).
size_t walkArgs(const char* format, va_list args, uint8_t* dst, size_t capacity, size_t stringLimit) {
    size_t n = 0;
    for (const char* p = format; *p; p++) {
        if (*p != '%') continue;
        const Spec s = parseSpec(p);
        if (s.kind == K_BAD) break;

        if (s.kind != K_PERCENT) {
            int32_t starValue = -1;
            for (uint8_t i = 0; i < s.stars; i++) {
                starValue = va_arg(args, int);
                put(dst, n, capacity, &starValue, sizeof(starValue));
            }

            switch (s.kind) {
                case K_I32: {
                    const int32_t v = va_arg(args, int);
                    put(dst, n, capacity, &v, sizeof(v));
                    break;
                }
                case K_I64: {
                    const int64_t v = va_arg(args, long long);
                    put(dst, n, capacity, &v, sizeof(v));
                    break;
                }
                case K_DOUBLE: {
                    const double v = va_arg(args, double);
                    put(dst, n, capacity, &v, sizeof(v));
                    break;
                }
                case K_PTR: {
                    const void* v = va_arg(args, void*);
                    put(dst, n, capacity, &v, sizeof(v));
                    break;
                }
                case K_STR: {
                    const char* str = va_arg(args, const char*);
                    uint16_t len = NULL_STRING;
                    if (str) {
                        size_t limit = stringLimit;
                        const int32_t precision = s.starPrecision ? starValue : s.precision;
                        if (s.hasPrecision && precision >= 0 && static_cast<size_t>(precision) < limit) {
                            limit = static_cast<size_t>(precision);
                        }
                        size_t copy = strnlen(str, limit);
                        // The string may have grown since the measuring pass
                        if (dst) {
                            const size_t room = capacity > n + 3 ? capacity - n - 3 : 0;
                            if (copy > room) copy = room;
                        }
                        len = static_cast<uint16_t>(copy);
                    }
                    put(dst, n, capacity, &len, sizeof(len));
                    if (len != NULL_STRING) {
                        put(dst, n, capacity, str, len);
                        const char nul = '\0';
                        put(dst, n, capacity, &nul, 1);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        p += s.length - 1;
    }
    return n;
}

inline uint32_t controlWord(uint32_t state, uint32_t length) {
    return ENTRY_MAGIC | (state << 16) | length;
}

inline uint32_t* controlAt(uint32_t offset) {
    return reinterpret_cast<uint32_t*>(&s_ring.data[offset]);
}

// Returns the entry start, or nullptr if the ring is full
uint8_t* reserve(uint32_t total) {
    uint8_t* entry = nullptr;
    portENTER_CRITICAL_SAFE(&s_mux);
    const uint32_t head = s_ring.head;
    const uint32_t tail = __atomic_load_n(&s_ring.tail, __ATOMIC_ACQUIRE);
    const uint32_t used = (head + RING_SIZE - tail) % RING_SIZE;
    const uint32_t freeBytes = RING_SIZE - used - 4;  // keep head != tail when full
    const uint32_t contiguous = RING_SIZE - head;

    if (contiguous >= total) {
        if (total <= freeBytes) {
            *controlAt(head) = controlWord(ST_WRITING, total);
            entry = &s_ring.data[head];
            __atomic_store_n(&s_ring.head, (head + total) % RING_SIZE, __ATOMIC_RELEASE);
        }
    } else if (contiguous + total <= freeBytes) {
        *controlAt(head) = controlWord(ST_PAD, contiguous);
        *controlAt(0) = controlWord(ST_WRITING, total);
        entry = &s_ring.data[0];
        __atomic_store_n(&s_ring.head, total, __ATOMIC_RELEASE);
    }
    portEXIT_CRITICAL_SAFE(&s_mux);
    return entry;
}

bool formatInFlash(const char* format) {
#ifdef DEFERRED_HAVE_MEMORY_UTILS
    return esp_ptr_in_drom(format);
#else
    (void)format;
    return false;
#endif
}

void appElfSha(uint8_t out[16]) {
#ifdef DEFERRED_HAVE_APP_DESC
    memcpy(out, esp_app_get_description()->app_elf_sha256, 16);
#else
    memset(out, 0, 16);
#endif
}

// ---------------------------------------------------------------------------
// Consumer side (logger task)
// ---------------------------------------------------------------------------

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    bool ok;

    template <typename T>
    T get() {
        T v{};
        if (ok && static_cast<size_t>(end - p) >= sizeof(T)) {
            memcpy(&v, p, sizeof(T));
            p += sizeof(T);
        } else {
            ok = false;
        }
        return v;
    }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
template <typename T>
int emitOne(char* out, size_t size, const char* spec, uint8_t stars, const int32_t* starValues, T value) {
    switch (stars) {
        case 0:  return snprintf(out, size, spec, value);
        case 1:  return snprintf(out, size, spec, static_cast<int>(starValues[0]), value);
        default: return snprintf(out, size, spec, static_cast<int>(starValues[0]),
                                 static_cast<int>(starValues[1]), value);
    }
}
#pragma GCC diagnostic pop

size_t appendRaw(char* out, size_t pos, size_t size, const char* text, size_t length) {
    const size_t room = size - 1 - pos;
    const size_t copy = length < room ? length : room;
    memcpy(out + pos, text, copy);
    return pos + copy;
}

// Formats one conversion at a time with snprintf, using the stored values
void formatEntry(const char* format, Reader& args, char* out, size_t size) {
    size_t pos = 0;
    const char* p = format;

    while (*p && pos + 1 < size) {
        if (*p != '%') {
            const char* next = strchr(p, '%');
            const size_t chunk = next ? static_cast<size_t>(next - p) : strlen(p);
            pos = appendRaw(out, pos, size, p, chunk);
            p += chunk;
            continue;
        }

        const Spec s = parseSpec(p);
        if (s.kind == K_PERCENT) {
            out[pos++] = '%';
            p += s.length;
            continue;
        }

        char spec[24];
        if (s.kind == K_BAD || s.length >= sizeof(spec)) {
            // Unsupported conversion: the producer stopped capturing here
            pos = appendRaw(out, pos, size, p, strlen(p));
            break;
        }
        memcpy(spec, p, s.length);
        spec[s.length] = '\0';

        int32_t starValues[2] = {0, 0};
        for (uint8_t i = 0; i < s.stars && i < 2; i++) {
            starValues[i] = args.get<int32_t>();
        }

        char* dst = out + pos;
        const size_t room = size - pos;
        int written = 0;
        switch (s.kind) {
            case K_I32: {
                const int32_t v = args.get<int32_t>();
                if (!args.ok) break;
                written = (s.lengthMod == 'l')
                    ? emitOne(dst, room, spec, s.stars, starValues, static_cast<long>(v))
                    : emitOne(dst, room, spec, s.stars, starValues, static_cast<int>(v));
                break;
            }
            case K_I64: {
                const int64_t v = args.get<int64_t>();
                if (!args.ok) break;
                written = emitOne(dst, room, spec, s.stars, starValues, static_cast<long long>(v));
                break;
            }
            case K_DOUBLE: {
                const double v = args.get<double>();
                if (!args.ok) break;
                written = emitOne(dst, room, spec, s.stars, starValues, v);
                break;
            }
            case K_PTR: {
                const void* v = args.get<const void*>();
                if (!args.ok) break;
                written = emitOne(dst, room, spec, s.stars, starValues, v);
                break;
            }
            case K_STR: {
                const uint16_t len = args.get<uint16_t>();
                if (!args.ok) break;
                const char* str = "(null)";
                if (len != NULL_STRING) {
                    if (static_cast<size_t>(args.end - args.p) < static_cast<size_t>(len) + 1) {
                        args.ok = false;
                        break;
                    }
                    str = reinterpret_cast<const char*>(args.p);
                    args.p += len + 1;
                }
                written = emitOne(dst, room, spec, s.stars, starValues, str);
                break;
            }
            default:
                break;
        }

        if (!args.ok) {
            pos = appendRaw(out, pos, size, "<?>", 3);
            break;
        }
        if (written > 0) {
            pos += (static_cast<size_t>(written) < room - 1) ? static_cast<size_t>(written) : room - 1;
        }
        p += s.length;
    }
    out[pos] = '\0';
}

void advanceTail(uint32_t tail, uint32_t length) {
    __atomic_store_n(&s_ring.tail, (tail + length) % RING_SIZE, __ATOMIC_RELEASE);
}

void discardAll() {
    portENTER_CRITICAL_SAFE(&s_mux);
    __atomic_store_n(&s_ring.tail, s_ring.head, __ATOMIC_RELEASE);
    portEXIT_CRITICAL_SAFE(&s_mux);
    s_preResetPending = 0;
    s_overflow.fetch_add(1);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void init() {
    if (s_initialized) return;
    s_initialized = true;

    uint8_t sha[16];
    appElfSha(sha);

    const uint32_t head = s_ring.head;
    const uint32_t tail = s_ring.tail;
    bool valid = s_ring.magic == RING_MAGIC &&
                 s_ring.size == RING_SIZE &&
                 memcmp(s_ring.elfSha, sha, sizeof(sha)) == 0 &&
                 head < RING_SIZE && tail < RING_SIZE &&
                 (head & 3u) == 0 && (tail & 3u) == 0;

    uint32_t complete = 0;
    uint32_t incomplete = 0;
    if (valid) {
        const uint32_t used = (head + RING_SIZE - tail) % RING_SIZE;
        uint32_t offset = tail;
        uint32_t walked = 0;
        while (offset != head) {
            const uint32_t word = *controlAt(offset);
            const uint32_t length = word & 0xFFFFu;
            const uint32_t state = (word >> 16) & 0xFFu;
            if ((word & 0xFF000000u) != ENTRY_MAGIC || length < 4 || (length & 3u) ||
                offset + length > RING_SIZE || walked + length > used) {
                valid = false;
                break;
            }
            if (state == ST_PAD) {
                if (offset + length != RING_SIZE) { valid = false; break; }
            } else if (state == ST_READY) {
                complete++;
            } else if (state == ST_WRITING) {
                incomplete++;
            } else {
                valid = false;
                break;
            }
            walked += length;
            offset = (offset + length) % RING_SIZE;
        }
    }

    if (!valid) {
        s_ring.magic = RING_MAGIC;
        s_ring.size = RING_SIZE;
        memcpy(s_ring.elfSha, sha, sizeof(sha));
        s_ring.head = 0;
        s_ring.tail = 0;
        complete = 0;
        incomplete = 0;
    }

    s_survived = valid;
    s_recovered = complete;
    s_incomplete = incomplete;
    s_preResetPending = complete + incomplete;
}

bool push(esp_log_level_t level, const char* tag, const char* format, va_list args, uint8_t flags) {
    if (!format) return false;
    if (!s_initialized) init();

    const bool formatInline = !formatInFlash(format);
    const size_t tagLength = tag ? strnlen(tag, TAG_MAX) : 0;
    const size_t formatLength = formatInline ? strnlen(format, FORMAT_INLINE_MAX) : 0;
    const size_t argOffset = HEADER_SIZE + tagLength + 1 + (formatInline ? formatLength + 1 : 0);

    size_t stringLimit = CONFIG_LOG_DEFERRED_MAX_STRING;
    va_list ap;
    va_copy(ap, args);
    size_t argBytes = walkArgs(format, ap, nullptr, 0, stringLimit);
    va_end(ap);

    if (argOffset + argBytes > MAX_ENTRY) {
        stringLimit = STRING_LIMIT_FALLBACK;
        va_copy(ap, args);
        argBytes = walkArgs(format, ap, nullptr, 0, stringLimit);
        va_end(ap);
    }

    const uint32_t total = static_cast<uint32_t>((argOffset + argBytes + 3u) & ~static_cast<size_t>(3u));
    if (total > MAX_ENTRY) {
        s_overflow.fetch_add(1);
        return false;
    }

    uint8_t* entry = reserve(total);
    if (!entry) {
        s_overflow.fetch_add(1);
        return false;
    }

    const uint8_t levelByte = static_cast<uint8_t>(level);
    const uint8_t flagByte = static_cast<uint8_t>(flags | (formatInline ? FLAG_FORMAT_INLINE : 0));
    const uint16_t argOffset16 = static_cast<uint16_t>(argOffset);
    const uint32_t timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    memcpy(entry + 4, &levelByte, 1);
    memcpy(entry + 5, &flagByte, 1);
    memcpy(entry + 6, &argOffset16, 2);
    memcpy(entry + 8, &timestamp, 4);
    memcpy(entry + 12, &format, 4);

    const char* taskName = (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        ? "boot" : pcTaskGetName(nullptr);
    if (!taskName) taskName = "?";
    const size_t taskLength = strnlen(taskName, TASK_NAME_MAX - 1);
    memcpy(entry + 16, taskName, taskLength);
    memset(entry + 16 + taskLength, 0, TASK_NAME_MAX - taskLength);

    uint8_t* cursor = entry + HEADER_SIZE;
    if (tagLength) memcpy(cursor, tag, tagLength);
    cursor[tagLength] = '\0';
    cursor += tagLength + 1;
    if (formatInline) {
        memcpy(cursor, format, formatLength);
        cursor[formatLength] = '\0';
    }

    va_copy(ap, args);
    walkArgs(format, ap, entry + argOffset, total - argOffset, stringLimit);
    va_end(ap);

    // Publish: everything above must be visible before the READY word
    __atomic_store_n(reinterpret_cast<uint32_t*>(entry), controlWord(ST_READY, total), __ATOMIC_RELEASE);
    return true;
}

PopResult pop(Record& record, char* message, size_t messageSize, bool skipBusy) {
    for (;;) {
        const uint32_t tail = __atomic_load_n(&s_ring.tail, __ATOMIC_ACQUIRE);
        const uint32_t head = __atomic_load_n(&s_ring.head, __ATOMIC_ACQUIRE);
        if (tail == head) return PopResult::EMPTY;

        const uint32_t word = __atomic_load_n(controlAt(tail), __ATOMIC_ACQUIRE);
        const uint32_t length = word & 0xFFFFu;
        const uint32_t state = (word >> 16) & 0xFFu;
        if ((word & 0xFF000000u) != ENTRY_MAGIC || length < 4 || (length & 3u) ||
            tail + length > RING_SIZE) {
            discardAll();  // corrupt - cannot find the next entry boundary
            return PopResult::EMPTY;
        }

        if (state == ST_PAD) {
            advanceTail(tail, length);
            continue;
        }

        const bool preReset = s_preResetPending > 0;
        if (state == ST_WRITING) {
            // Before the reset: its producer is gone. Now: wait unless told to skip.
            if (!preReset && !skipBusy) return PopResult::BUSY;
            if (preReset) s_preResetPending--;
            advanceTail(tail, length);
            continue;
        }
        if (state != ST_READY || length < HEADER_SIZE + 1) {
            discardAll();
            return PopResult::EMPTY;
        }

        const uint8_t* entry = &s_ring.data[tail];
        uint8_t levelByte;
        uint8_t flagByte;
        uint16_t argOffset;
        const char* format = nullptr;
        memcpy(&levelByte, entry + 4, 1);
        memcpy(&flagByte, entry + 5, 1);
        memcpy(&argOffset, entry + 6, 2);
        memcpy(&record.timestamp, entry + 8, 4);
        memcpy(&format, entry + 12, 4);

        record.level = static_cast<esp_log_level_t>(levelByte);
        record.flags = flagByte & static_cast<uint8_t>(~FLAG_FORMAT_INLINE);
        record.preReset = preReset;
        memcpy(record.task, entry + 16, TASK_NAME_MAX);
        record.task[TASK_NAME_MAX - 1] = '\0';

        const char* tag = reinterpret_cast<const char*>(entry + HEADER_SIZE);
        const size_t tagLength = strnlen(tag, TAG_MAX);
        memcpy(record.tag, tag, tagLength);
        record.tag[tagLength] = '\0';

        if (flagByte & FLAG_FORMAT_INLINE) {
            format = tag + tagLength + 1;
        }

        if (argOffset > length || argOffset < HEADER_SIZE + tagLength + 1) {
            message[0] = '\0';
        } else if (!(flagByte & FLAG_FORMAT_INLINE) && !formatInFlash(format)) {
            // Never dereference a stored pointer that does not point into flash
            snprintf(message, messageSize, "<bad format pointer %p>", format);
        } else {
            Reader args = {entry + argOffset, entry + length, true};
            formatEntry(format, args, message, messageSize);
        }

        if (preReset) s_preResetPending--;
        advanceTail(tail, length);
        return PopResult::OK;
    }
}

bool empty() {
    return __atomic_load_n(&s_ring.tail, __ATOMIC_ACQUIRE) ==
           __atomic_load_n(&s_ring.head, __ATOMIC_ACQUIRE);
}

uint32_t overflowCount() { return s_overflow.load(); }
uint32_t recoveredCount() { return s_recovered; }
uint32_t recoveredIncompleteCount() { return s_incomplete; }
bool ringSurvivedReset() { return s_survived; }

} // namespace DeferredLog

#endif // LOG_DEFERRED_FORMAT
