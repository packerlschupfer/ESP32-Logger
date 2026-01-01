// Logger.cpp
// Professional Logger implementation with tag-level filtering and memory optimization

#include "Logger.h"
#include "ConsoleBackend.h"
#include "SynchronizedConsoleBackend.h"
#include "NonBlockingConsoleBackend.h"
#include <cstring>
#include <algorithm>
#include <esp_log.h>
#include <soc/soc.h>  // For SOC_DRAM_LOW, SOC_DRAM_HIGH

// Helper to check if pointer is in readable memory (DRAM or flash-mapped)
// Supports ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6
static inline bool isPointerReadable(const void* ptr) {
    if (!ptr) return false;
    uintptr_t addr = (uintptr_t)ptr;

#if CONFIG_IDF_TARGET_ESP32
    // ESP32 DRAM: 0x3FFB0000 - 0x3FFFFFFF
    // ESP32 Flash-mapped: 0x3F400000 - 0x3F7FFFFF
    // ESP32 ROM: 0x3FF00000 - 0x3FF7FFFF
    return (addr >= 0x3F400000 && addr < 0x40000000);

#elif CONFIG_IDF_TARGET_ESP32S2
    // ESP32-S2 DRAM: 0x3FFB0000 - 0x3FFFFFFF
    // ESP32-S2 Flash-mapped: 0x3F000000 - 0x3FFFFFFF
    return (addr >= 0x3F000000 && addr < 0x40000000);

#elif CONFIG_IDF_TARGET_ESP32S3
    // ESP32-S3 DRAM: 0x3FC88000 - 0x3FCFFFFF
    // ESP32-S3 Flash-mapped: 0x3C000000 - 0x3DFFFFFF
    return (addr >= 0x3C000000 && addr < 0x3D000000) ||
           (addr >= 0x3FC00000 && addr < 0x3FD00000);

#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
    // ESP32-C3/C6 DRAM: 0x3FC80000 - 0x3FCFFFFF
    // ESP32-C3/C6 Flash-mapped: 0x3C000000 - 0x3C7FFFFF
    return (addr >= 0x3C000000 && addr < 0x3C800000) ||
           (addr >= 0x3FC80000 && addr < 0x3FD00000);

#else
    // Fallback: assume readable if not null (less safe, but functional)
    // This covers unknown/future ESP32 variants
    (void)addr;  // Suppress unused warning
    return true;
#endif
}

// ESP-IDF log redirection function
static int esp_log_redirect(const char* format, va_list args) {
    // Validate format pointer - ESP-IDF internal components may pass invalid pointers
    // (e.g., from IRAM/ROM) that cause LoadProhibited exceptions when accessed
    if (!format || !isPointerReadable(format)) {
        return 0;
    }

    // Get the logger instance
    Logger& logger = Logger::getInstance();

    // Use buffer pool instead of thread-local to reduce per-thread overhead
    // Small fixed buffer for tag extraction (on stack, only 33 bytes)
    char tag_buffer[CONFIG_LOG_SUBSCRIBER_TAG_SIZE + 1];

    // Extract tag and format from ESP-IDF log format
    // ESP-IDF format is typically: "TAG: message" or just "message"
    const char* tag = "ESP";
    const char* msg_start = format;

    // Look for colon to extract tag
    const char* colon = strchr(format, ':');
    if (colon && (colon - format) < CONFIG_LOG_SUBSCRIBER_TAG_SIZE) {
        // Extract tag
        size_t tag_len = colon - format;
        memcpy(tag_buffer, format, tag_len);
        tag_buffer[tag_len] = '\0';
        tag = tag_buffer;
        msg_start = colon + 1;
        while (*msg_start == ' ') msg_start++;  // Skip spaces after colon
    }

    // Use logger to output the message
    logger.logV(ESP_LOG_INFO, tag, msg_start, args);

    return 0;  // Return value is ignored by ESP-IDF
}

// BufferPool implementation
BufferPool::BufferPool() : poolMutex(nullptr) {
    // Only create mutex if FreeRTOS scheduler is running
    // This prevents issues if Logger is used before scheduler starts
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        poolMutex = xSemaphoreCreateMutex();
    }
    // Pool array is zero-initialized by default
}

char* BufferPool::acquire() {
    if (!poolMutex) {
        return (char*)malloc(BUFFER_SIZE);
    }

    // Always use mutex to prevent race conditions
    if (xSemaphoreTake(poolMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_STANDARD_TIMEOUT_MS)) == pdTRUE) {
        for (auto& buffer : pool) {
            bool expected = false;
            if (buffer.inUse.compare_exchange_strong(expected, true)) {
                xSemaphoreGive(poolMutex);
                return buffer.data;
            }
        }
        xSemaphoreGive(poolMutex);
    }

    // No buffer available - fallback to heap allocation
    return (char*)malloc(BUFFER_SIZE);
}

void BufferPool::release(char* buffer) {
    if (!buffer) return;

    // Check if it's from our pool
    for (auto& poolBuffer : pool) {
        if (buffer == poolBuffer.data) {
            poolBuffer.inUse.store(false);
            return;
        }
    }

    // Not from pool - must be heap allocated
    free(buffer);
}

// Helper to safely create mutex (returns nullptr if scheduler not running)
static SemaphoreHandle_t createMutexSafe() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return xSemaphoreCreateMutex();
    }
    return nullptr;
}

// Logger implementation
Logger::Logger()
    : backendMutex(createMutexSafe()),
      subscriberMutex(createMutexSafe()),
      tagMutex(createMutexSafe()),
      rateLimitMutex(createMutexSafe()),
      lastLogTime(0),
      logCounter(0) {
    // Add default non-blocking console backend to prevent freezes
    backends.push_back(std::make_shared<NonBlockingConsoleBackend>());

    // Initialize tag levels array
    memset(tagLevels_, 0, sizeof(tagLevels_));
}

Logger::Logger(std::shared_ptr<ILogBackend> backend)
    : backendMutex(createMutexSafe()),
      subscriberMutex(createMutexSafe()),
      tagMutex(createMutexSafe()),
      rateLimitMutex(createMutexSafe()),
      lastLogTime(0),
      logCounter(0) {
    if (backend) {
        backends.push_back(std::move(backend));
    } else {
        backends.push_back(std::make_shared<NonBlockingConsoleBackend>());
    }

    // Initialize tag levels array
    memset(tagLevels_, 0, sizeof(tagLevels_));
}

Logger::~Logger() {
    // Stop subscriber task first (this also cleans up queue)
    stopSubscriberTask();

    if (backendMutex) vSemaphoreDelete(backendMutex);
    if (subscriberMutex) vSemaphoreDelete(subscriberMutex);
    if (tagMutex) vSemaphoreDelete(tagMutex);
    if (rateLimitMutex) vSemaphoreDelete(rateLimitMutex);
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::init(size_t bufferSize) {
    // Buffer size is now fixed by CONFIG_LOG_BUFFER_SIZE
    // This method kept for API compatibility
    initialized_.store(true);
}

void Logger::configure(const LoggerConfig& config) {
    // Mark as initialized since configure() is explicit setup
    initialized_.store(true);

    // Apply global settings
    setLogLevel(config.defaultLevel);
    enableLogging(config.enableLogging);
    setMaxLogsPerSecond(config.maxLogsPerSecond);
    
    // Configure backend
    switch (config.primaryBackend) {
        case LoggerConfig::BackendType::CONSOLE:
            setBackend(std::make_shared<ConsoleBackend>());
            break;
        case LoggerConfig::BackendType::SYNCHRONIZED_CONSOLE:
            setBackend(std::make_shared<SynchronizedConsoleBackend>());
            break;
        case LoggerConfig::BackendType::NON_BLOCKING_CONSOLE:
            setBackend(std::make_shared<NonBlockingConsoleBackend>());
            break;
        case LoggerConfig::BackendType::CUSTOM:
            // User should set custom backend separately
            break;
    }
    
    // Apply tag configurations
    for (size_t i = 0; i < config.tagConfigCount; ++i) {
        const auto& tagConfig = config.tagConfigs[i];
        if (tagConfig.tag) {
            setTagLevel(tagConfig.tag, tagConfig.level);
        }
    }
}

void Logger::enableLogging(bool enable) {
    isLoggingEnabled.store(enable);
}

void Logger::setLogLevel(esp_log_level_t level) {
    globalLogLevel.store(level);
}

void Logger::setMaxLogsPerSecond(uint32_t maxLogs) {
    maxLogsPerSecond.store(maxLogs);
}

void Logger::setBackend(std::shared_ptr<ILogBackend> newBackend) {
    // Allow operation without mutex if scheduler not started (single-threaded)
    if (!backendMutex) {
        backends.clear();
        if (newBackend) {
            backends.push_back(std::move(newBackend));
        }
        return;
    }

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        backends.clear();
        if (newBackend) {
            backends.push_back(std::move(newBackend));
        }
        xSemaphoreGive(backendMutex);
    }
}

void Logger::addBackend(std::shared_ptr<ILogBackend> backend) {
    if (!backend) return;

    // Allow operation without mutex if scheduler not started (single-threaded)
    if (!backendMutex) {
        backends.push_back(std::move(backend));
        return;
    }

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        backends.push_back(std::move(backend));
        xSemaphoreGive(backendMutex);
    }
}

void Logger::removeBackend(std::shared_ptr<ILogBackend> backend) {
    if (!backend) return;

    // Allow operation without mutex if scheduler not started (single-threaded)
    if (!backendMutex) {
        backends.erase(
            std::remove(backends.begin(), backends.end(), backend),
            backends.end()
        );
        return;
    }

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        backends.erase(
            std::remove(backends.begin(), backends.end(), backend),
            backends.end()
        );
        xSemaphoreGive(backendMutex);
    }
}

void Logger::clearBackends() {
    // Allow operation without mutex if scheduler not started (single-threaded)
    if (!backendMutex) {
        backends.clear();
        return;
    }

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        backends.clear();
        xSemaphoreGive(backendMutex);
    }
}

void Logger::setTagLevel(const char* tag, esp_log_level_t level) {
    if (!tag || strlen(tag) == 0) return;

    size_t tagLen = strlen(tag);
    if (tagLen >= CONFIG_LOG_SUBSCRIBER_TAG_SIZE) {
        tagLen = CONFIG_LOG_SUBSCRIBER_TAG_SIZE - 1;  // Truncate if too long
    }

    // Helper lambda to set tag level (no heap allocation)
    auto doSetTagLevel = [this, tag, tagLen, level]() {
        size_t count = tagLevelCount_.load();

        // First, check if tag already exists and update it
        for (size_t i = 0; i < count; i++) {
            if (strncmp(tagLevels_[i].tag, tag, CONFIG_LOG_SUBSCRIBER_TAG_SIZE) == 0) {
                tagLevels_[i].level = level;
                esp_log_level_set(tag, level);
                return;
            }
        }

        // Tag not found - add new entry if space available
        if (count < CONFIG_LOG_MAX_TAGS) {
            memcpy(tagLevels_[count].tag, tag, tagLen);
            tagLevels_[count].tag[tagLen] = '\0';
            tagLevels_[count].level = level;
            tagLevelCount_.store(count + 1);
            esp_log_level_set(tag, level);
        }
    };

    // Allow operation without mutex if scheduler not started (single-threaded)
    if (!tagMutex) {
        doSetTagLevel();
        return;
    }

    if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        doSetTagLevel();
        xSemaphoreGive(tagMutex);
    } else {
        mutexTimeouts_.fetch_add(1);
    }
}

esp_log_level_t Logger::getTagLevel(const char* tag) const {
    if (!tag) return globalLogLevel.load();

    esp_log_level_t level = globalLogLevel.load();

    // Return global level if no mutex (pre-scheduler, can't safely access tagLevels_)
    if (!tagMutex) {
        return level;
    }

    if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        size_t count = tagLevelCount_.load();
        for (size_t i = 0; i < count; i++) {
            if (strncmp(tagLevels_[i].tag, tag, CONFIG_LOG_SUBSCRIBER_TAG_SIZE) == 0) {
                level = tagLevels_[i].level;
                break;
            }
        }
        xSemaphoreGive(tagMutex);
    }

    return level;
}

bool Logger::isLevelEnabledForTag(const char* tag, esp_log_level_t level) const {
    if (!isLoggingEnabled.load()) return false;

    // ESP_LOG_NONE should never be logged
    if (level == ESP_LOG_NONE) return false;

    // If no tag, use global level
    if (!tag) {
        return level <= globalLogLevel.load();
    }

    // Check tag-specific level first
    esp_log_level_t effectiveLevel = globalLogLevel.load();

    // Check if mutex exists before trying to take it (may be null if scheduler not started)
    if (tagMutex) {
        if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
            size_t count = tagLevelCount_.load();
            for (size_t i = 0; i < count; i++) {
                if (strncmp(tagLevels_[i].tag, tag, CONFIG_LOG_SUBSCRIBER_TAG_SIZE) == 0) {
                    effectiveLevel = tagLevels_[i].level;
                    break;
                }
            }
            xSemaphoreGive(tagMutex);
        }
    }
    // If no mutex, just use global level (can't check tag-specific levels safely)

    // Use tag-specific level if configured, otherwise use global level
    return level <= effectiveLevel;
}

bool Logger::checkRateLimit() {
    uint32_t maxPerSec = maxLogsPerSecond.load();
    if (maxPerSec == 0) return true;  // Unlimited

    // Allow logging if no mutex (pre-scheduler)
    if (!rateLimitMutex) {
        return true;
    }

    bool allowed = false;

    if (xSemaphoreTake(rateLimitMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_MEDIUM_TIMEOUT_MS)) == pdTRUE) {
        uint32_t currentTime = millis();
        // Handle millis() wrap-around gracefully (after ~49 days)
        // Unsigned subtraction handles wrap correctly: if currentTime wrapped,
        // elapsed will be a large value which correctly triggers reset
        uint32_t elapsed = currentTime - lastLogTime;

        if (elapsed >= LoggerConfig::RATE_LIMIT_WINDOW_MS) {
            // New window - reset counter
            lastLogTime = currentTime;
            logCounter = 1;
            allowed = true;
        } else if (logCounter < maxPerSec) {
            logCounter++;
            allowed = true;
        } else {
            droppedLogs.fetch_add(1);
            allowed = false;
        }

        xSemaphoreGive(rateLimitMutex);
    } else {
        // Mutex timeout - track for diagnostics
        mutexTimeouts_.fetch_add(1);
    }

    return allowed;
}

void Logger::writeToBackends(const char* message, size_t length) {
    // Fallback: write directly without mutex protection (pre-scheduler)
    if (!backendMutex) {
        for (auto& backend : backends) {
            if (backend) {
                backend->write(message, length);
            }
        }
        return;
    }

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_STANDARD_TIMEOUT_MS)) == pdTRUE) {
        for (auto& backend : backends) {
            if (backend) {
                backend->write(message, length);
            }
        }
        xSemaphoreGive(backendMutex);
    }
}

void Logger::log(esp_log_level_t level, const char* tag, const char* format, ...) {
    if (!isLevelEnabledForTag(tag, level)) return;
    
    va_list args;
    va_start(args, format);
    logV(level, tag, format, args);
    va_end(args);
}

void Logger::logV(esp_log_level_t level, const char* tag, const char* format, va_list args) {
    if (!isLevelEnabledForTag(tag, level)) return;
    if (!checkRateLimit()) return;

    auto& pool = BufferPool::getInstance();

    // Get buffer from pool
    char* formatBuffer = pool.acquire();
    if (!formatBuffer) return;

    // Format the message
    vsnprintf(formatBuffer, CONFIG_LOG_BUFFER_SIZE, format, args);

    // Notify subscribers with formatted message (before adding timestamp/task info)
    notifySubscribers(level, tag, formatBuffer);

    // Get another buffer for full message
    char* fullMessage = pool.acquire();
    if (fullMessage) {
        // Format complete message
        int len = snprintf(fullMessage, CONFIG_LOG_BUFFER_SIZE,
                          "[%lu][%s][%s] %s: %s\r\n",
                          millis(),
                          pcTaskGetName(NULL) ?: "?",
                          levelToString(level),
                          tag ?: "?",
                          formatBuffer);

        // Ensure message wasn't truncated
        if (len >= CONFIG_LOG_BUFFER_SIZE) {
            len = CONFIG_LOG_BUFFER_SIZE - 1;
            fullMessage[len] = '\0';  // Ensure null termination
        }

        // Write to all backends
        writeToBackends(fullMessage, len);

        pool.release(fullMessage);
    } else {
        // Fallback to ESP-IDF logging
        esp_log_write(level, tag, "%s", formatBuffer);
    }

    pool.release(formatBuffer);
}

void Logger::logNnL(esp_log_level_t level, const char* tag, const char* format, ...) {
    if (!isLevelEnabledForTag(tag, level)) return;
    if (!checkRateLimit()) return;

    auto& pool = BufferPool::getInstance();

    va_list args;
    va_start(args, format);

    char* formatBuffer = pool.acquire();
    if (!formatBuffer) {
        va_end(args);
        return;
    }

    vsnprintf(formatBuffer, CONFIG_LOG_BUFFER_SIZE, format, args);
    va_end(args);

    // Notify subscribers with formatted message (before adding timestamp/task info)
    notifySubscribers(level, tag, formatBuffer);

    char* fullMessage = pool.acquire();
    if (fullMessage) {
        int len = snprintf(fullMessage, CONFIG_LOG_BUFFER_SIZE,
                          "[%lu][%s][%s] %s: %s",  // No newline
                          millis(),
                          pcTaskGetName(NULL) ?: "?",
                          levelToString(level),
                          tag ?: "?",
                          formatBuffer);

        writeToBackends(fullMessage, len);
        pool.release(fullMessage);
    }

    pool.release(formatBuffer);
}

void Logger::logInL(const char* format, ...) {
    if (!isLoggingEnabled.load()) return;
    if (!checkRateLimit()) return;

    auto& pool = BufferPool::getInstance();

    va_list args;
    va_start(args, format);

    char* formatBuffer = pool.acquire();
    if (!formatBuffer) {
        va_end(args);
        return;
    }

    vsnprintf(formatBuffer, CONFIG_LOG_BUFFER_SIZE, format, args);
    va_end(args);

    // Notify subscribers (using INFO level and "INL" tag for inline logs)
    notifySubscribers(ESP_LOG_INFO, "INL", formatBuffer);

    writeToBackends(formatBuffer, strlen(formatBuffer));
    pool.release(formatBuffer);
}

void Logger::logDirect(esp_log_level_t level, const char* tag, const char* message) {
    if (!message) return;  // Prevent nullptr dereference
    if (!isLevelEnabledForTag(tag, level)) return;
    // Note: logDirect intentionally bypasses rate limiting but still notifies subscribers

    // Notify subscribers with the raw message
    notifySubscribers(level, tag, message);

    auto& pool = BufferPool::getInstance();

    char* fullMessage = pool.acquire();
    if (fullMessage) {
        int len = snprintf(fullMessage, CONFIG_LOG_BUFFER_SIZE,
                          "[%lu][%s][%s] %s: %s\r\n",
                          millis(),
                          pcTaskGetName(NULL) ?: "?",
                          levelToString(level),
                          tag ?: "?",
                          message);

        writeToBackends(fullMessage, len);
        pool.release(fullMessage);
    } else {
        esp_log_write(level, tag, "%s", message);
    }
}

void Logger::flush() {
    // Flush directly without mutex if scheduler not started (single-threaded)
    if (!backendMutex) {
        for (auto& backend : backends) {
            if (backend) {
                backend->flush();
            }
        }
        return;
    }

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        for (auto& backend : backends) {
            if (backend) {
                backend->flush();
            }
        }
        xSemaphoreGive(backendMutex);
    }
}

void Logger::resetDroppedLogs() {
    droppedLogs.store(0);
}

void Logger::enableESPLogRedirection() {
    // Redirect ESP-IDF logs through our logger
    esp_log_set_vprintf(&esp_log_redirect);
}

// Log subscriber implementation
bool Logger::addLogSubscriber(LogSubscriberCallback callback) {
    if (callback == nullptr) {
        return false;
    }

    // Thread safety - only if mutex available
    if (subscriberMutex && xSemaphoreTake(subscriberMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    bool result = false;
    uint8_t count = subscriberCount.load();

    // Check for duplicates and available slots
    if (count < MAX_SUBSCRIBERS) {
        bool isDuplicate = false;
        for (uint8_t i = 0; i < count; i++) {
            if (subscribers[i] == callback) {
                isDuplicate = true;
                break;
            }
        }

        if (!isDuplicate) {
            subscribers[count] = callback;
            subscriberCount.store(count + 1);
            result = true;
        }
    }

    if (subscriberMutex) {
        xSemaphoreGive(subscriberMutex);
    }

    return result;
}

bool Logger::removeLogSubscriber(LogSubscriberCallback callback) {
    if (callback == nullptr) {
        return false;
    }

    if (subscriberMutex && xSemaphoreTake(subscriberMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    bool result = false;
    uint8_t count = subscriberCount.load();

    for (uint8_t i = 0; i < count; i++) {
        if (subscribers[i] == callback) {
            // Shift remaining subscribers down
            for (uint8_t j = i; j < count - 1; j++) {
                subscribers[j] = subscribers[j + 1];
            }
            count--;
            subscribers[count] = nullptr;
            subscriberCount.store(count);
            result = true;
            break;
        }
    }

    if (subscriberMutex) {
        xSemaphoreGive(subscriberMutex);
    }

    return result;
}

bool Logger::startSubscriberTask(int coreId) {
    // Already running?
    if (subscriberTaskHandle != nullptr) {
        return true;
    }

    // Create queue if not exists
    if (subscriberQueue == nullptr) {
        subscriberQueue = xQueueCreate(CONFIG_LOG_SUBSCRIBER_QUEUE_SIZE, sizeof(LogSubscriberMessage));
        if (subscriberQueue == nullptr) {
            return false;
        }
    }

    subscriberTaskRunning.store(true);

    // Create task with optional core affinity
    BaseType_t result;
    if (coreId >= 0 && coreId <= 1) {
        result = xTaskCreatePinnedToCore(
            subscriberTaskFunc,
            "LogSub",
            CONFIG_LOG_SUBSCRIBER_TASK_STACK,
            this,
            CONFIG_LOG_SUBSCRIBER_TASK_PRIORITY,
            &subscriberTaskHandle,
            coreId
        );
    } else {
        result = xTaskCreate(
            subscriberTaskFunc,
            "LogSub",
            CONFIG_LOG_SUBSCRIBER_TASK_STACK,
            this,
            CONFIG_LOG_SUBSCRIBER_TASK_PRIORITY,
            &subscriberTaskHandle
        );
    }

    if (result != pdPASS) {
        subscriberTaskRunning.store(false);
        subscriberTaskHandle = nullptr;
        return false;
    }

    return true;
}

void Logger::stopSubscriberTask() {
    if (subscriberTaskHandle == nullptr) {
        return;
    }

    // Signal task to stop
    subscriberTaskRunning.store(false);

    // Send a dummy message to wake up the task if it's blocked on queue
    if (subscriberQueue) {
        LogSubscriberMessage dummy = {};
        xQueueSend(subscriberQueue, &dummy, 0);
    }

    // Wait for task to finish (with timeout)
    for (int i = 0; i < 50 && subscriberTaskHandle != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Force delete if still running
    if (subscriberTaskHandle != nullptr) {
        vTaskDelete(subscriberTaskHandle);
        subscriberTaskHandle = nullptr;
    }

    // Clean up queue
    if (subscriberQueue) {
        vQueueDelete(subscriberQueue);
        subscriberQueue = nullptr;
    }
}

void Logger::subscriberTaskFunc(void* param) {
    Logger* logger = static_cast<Logger*>(param);
    LogSubscriberMessage msg;

    while (logger->subscriberTaskRunning.load()) {
        // Wait for message (with timeout to check running flag)
        if (xQueueReceive(logger->subscriberQueue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Check if we should stop
            if (!logger->subscriberTaskRunning.load()) {
                break;
            }

            // Get a snapshot of callbacks
            Logger::LogSubscriberCallback localCallbacks[MAX_SUBSCRIBERS];
            uint8_t localCount = 0;

            if (logger->subscriberMutex &&
                xSemaphoreTake(logger->subscriberMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
                localCount = logger->subscriberCount.load();
                for (uint8_t i = 0; i < localCount; i++) {
                    localCallbacks[i] = logger->subscribers[i];
                }
                xSemaphoreGive(logger->subscriberMutex);
            }

            // Invoke callbacks (without mutex held)
            for (uint8_t i = 0; i < localCount; i++) {
                if (localCallbacks[i] != nullptr) {
                    localCallbacks[i](msg.level, msg.tag, msg.message);
                }
            }
        }
    }

    // Clean exit
    logger->subscriberTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void Logger::notifySubscribers(esp_log_level_t level, const char* tag, const char* message) {
    // Fast path - no subscribers registered (atomic load is cheap)
    if (subscriberCount.load() == 0) {
        return;
    }

    // Safety check - don't queue from ISR context
    if (xPortInIsrContext()) {
        return;
    }

    // If queue exists, use async notification (preferred)
    if (subscriberQueue) {
        LogSubscriberMessage msg;
        msg.level = level;

        // Safe string copy with null termination
        if (tag) {
            strncpy(msg.tag, tag, CONFIG_LOG_SUBSCRIBER_TAG_SIZE - 1);
            msg.tag[CONFIG_LOG_SUBSCRIBER_TAG_SIZE - 1] = '\0';
        } else {
            msg.tag[0] = '\0';
        }

        if (message) {
            strncpy(msg.message, message, CONFIG_LOG_SUBSCRIBER_MSG_SIZE - 1);
            msg.message[CONFIG_LOG_SUBSCRIBER_MSG_SIZE - 1] = '\0';
        } else {
            msg.message[0] = '\0';
        }

        // Non-blocking send - drop message if queue is full
        xQueueSend(subscriberQueue, &msg, 0);
        return;
    }

    // Fallback: synchronous notification (legacy behavior, not recommended)
    // Only used if startSubscriberTask() was never called

    // Copy callbacks while holding mutex to avoid calling with mutex held
    LogSubscriberCallback localCallbacks[MAX_SUBSCRIBERS];
    uint8_t localCount = 0;

    if (subscriberMutex && xSemaphoreTake(subscriberMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        localCount = subscriberCount.load();
        for (uint8_t i = 0; i < localCount; i++) {
            localCallbacks[i] = subscribers[i];
        }
        xSemaphoreGive(subscriberMutex);
    } else {
        return;  // Can't get mutex - skip notification
    }

    // Invoke callbacks WITHOUT holding mutex (prevents deadlock on reentrant logging)
    for (uint8_t i = 0; i < localCount; i++) {
        if (localCallbacks[i] != nullptr) {
            localCallbacks[i](level, tag, message);
        }
    }
}

// Global logger getter
Logger& getLogger() {
    return Logger::getInstance();
}