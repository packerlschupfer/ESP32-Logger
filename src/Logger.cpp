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
static inline bool isPointerReadable(const void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    // ESP32 DRAM: 0x3FFB0000 - 0x3FFFFFFF
    // ESP32 Flash-mapped: 0x3F400000 - 0x3F7FFFFF
    // Also allow ROM strings: 0x3FF00000 - 0x3FF7FFFF
    return (addr >= 0x3F400000 && addr < 0x40000000);
}

// ESP-IDF log redirection function
static int esp_log_redirect(const char* format, va_list args) {
    // Validate format pointer - ESP-IDF internal components may pass invalid pointers
    // (e.g., from IRAM/ROM) that cause LoadProhibited exceptions when accessed
    if (!format || !isPointerReadable(format)) {
        return 0;
    }

    // Thread-local buffer for tag extraction
    static thread_local char tag_buffer[33];

    // Get the logger instance
    Logger& logger = Logger::getInstance();

    // Extract tag and format from ESP-IDF log format
    // ESP-IDF format is typically: "TAG: message" or just "message"
    const char* tag = "ESP";
    const char* msg_start = format;

    // Look for colon to extract tag
    const char* colon = strchr(format, ':');
    if (colon && (colon - format) < 32) {  // Reasonable tag length
        // Extract tag
        size_t tag_len = colon - format;
        strncpy(tag_buffer, format, tag_len);
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

    // Pre-allocate tag map to expected size
    tagLevels.reserve(CONFIG_LOG_MAX_TAGS);
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

    // Pre-allocate tag map
    tagLevels.reserve(CONFIG_LOG_MAX_TAGS);
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

    // Helper lambda to set tag level
    auto doSetTagLevel = [this, tag, level]() {
        // Store tag as std::string (owns the memory)
        tagLevels[std::string(tag)] = level;

        // Also update ESP-IDF for compatibility
        esp_log_level_set(tag, level);
    };

    // Allow operation without mutex if scheduler not started (single-threaded)
    if (!tagMutex) {
        doSetTagLevel();
        return;
    }

    if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        doSetTagLevel();
        xSemaphoreGive(tagMutex);
    }
}

esp_log_level_t Logger::getTagLevel(const char* tag) const {
    if (!tag) return globalLogLevel.load();

    esp_log_level_t level = globalLogLevel.load();

    // Return global level if no mutex (pre-scheduler, can't safely access tagLevels)
    if (!tagMutex) {
        return level;
    }

    if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        auto it = tagLevels.find(std::string(tag));
        if (it != tagLevels.end()) {
            level = it->second;
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
            auto it = tagLevels.find(std::string(tag));
            if (it != tagLevels.end()) {
                // Tag has specific level configured
                effectiveLevel = it->second;
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
        uint32_t elapsed = currentTime - lastLogTime;

        if (elapsed >= LoggerConfig::RATE_LIMIT_WINDOW_MS) {
            // New second - reset counter
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

    writeToBackends(formatBuffer, strlen(formatBuffer));
    pool.release(formatBuffer);
}

void Logger::logDirect(esp_log_level_t level, const char* tag, const char* message) {
    if (!message) return;  // Prevent nullptr dereference
    if (!isLevelEnabledForTag(tag, level)) return;

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

    // Check for duplicates and available slots
    if (subscriberCount < MAX_SUBSCRIBERS) {
        bool isDuplicate = false;
        for (uint8_t i = 0; i < subscriberCount; i++) {
            if (subscribers[i] == callback) {
                isDuplicate = true;
                break;
            }
        }

        if (!isDuplicate) {
            subscribers[subscriberCount++] = callback;
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

    for (uint8_t i = 0; i < subscriberCount; i++) {
        if (subscribers[i] == callback) {
            // Shift remaining subscribers down
            for (uint8_t j = i; j < subscriberCount - 1; j++) {
                subscribers[j] = subscribers[j + 1];
            }
            subscribers[--subscriberCount] = nullptr;
            result = true;
            break;
        }
    }

    if (subscriberMutex) {
        xSemaphoreGive(subscriberMutex);
    }

    return result;
}

uint8_t Logger::getSubscriberCount() const {
    return subscriberCount;
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
                localCount = logger->subscriberCount;
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
    // Fast path - no subscribers registered
    if (subscriberCount == 0) {
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
        localCount = subscriberCount;
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