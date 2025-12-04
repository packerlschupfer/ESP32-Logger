// Logger.cpp
// Professional Logger implementation with tag-level filtering and memory optimization

#include "Logger.h"
#include "ConsoleBackend.h"
#include "SynchronizedConsoleBackend.h"
#include "NonBlockingConsoleBackend.h"
#include <cstring>
#include <algorithm>
#include <esp_log.h>

// ESP-IDF log redirection function
static int esp_log_redirect(const char* format, va_list args) {
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
    if (backendMutex) vSemaphoreDelete(backendMutex);
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

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        backends.push_back(std::move(backend));
        xSemaphoreGive(backendMutex);
    }
}

void Logger::removeBackend(std::shared_ptr<ILogBackend> backend) {
    if (!backend) return;

    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        backends.erase(
            std::remove(backends.begin(), backends.end(), backend),
            backends.end()
        );
        xSemaphoreGive(backendMutex);
    }
}

void Logger::clearBackends() {
    if (xSemaphoreTake(backendMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        backends.clear();
        xSemaphoreGive(backendMutex);
    }
}

void Logger::setTagLevel(const char* tag, esp_log_level_t level) {
    if (!tag || strlen(tag) == 0) return;

    if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        // Check if we have space in the string pool
        size_t tagLen = strlen(tag) + 1;
        if (tagPoolOffset + tagLen < sizeof(tagStringPool)) {
            // Copy tag to our pool
            char* pooledTag = &tagStringPool[tagPoolOffset];
            strcpy(pooledTag, tag);
            tagPoolOffset += tagLen;
            
            // Store or update the level
            tagLevels[std::string_view(pooledTag)] = level;
        } else {
            // Pool full - use dynamic string (fallback)
            tagLevels[std::string_view(tag)] = level;
        }
        
        // Also update ESP-IDF for compatibility
        esp_log_level_set(tag, level);
        
        xSemaphoreGive(tagMutex);
    }
}

esp_log_level_t Logger::getTagLevel(const char* tag) const {
    if (!tag) return globalLogLevel.load();
    
    esp_log_level_t level = globalLogLevel.load();

    if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        auto it = tagLevels.find(std::string_view(tag));
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

    if (xSemaphoreTake(tagMutex, pdMS_TO_TICKS(LoggerConfig::MUTEX_SHORT_TIMEOUT_MS)) == pdTRUE) {
        auto it = tagLevels.find(std::string_view(tag));
        if (it != tagLevels.end()) {
            // Tag has specific level configured
            effectiveLevel = it->second;
        }
        xSemaphoreGive(tagMutex);
    }
    
    // Use tag-specific level if configured, otherwise use global level
    return level <= effectiveLevel;
}

bool Logger::checkRateLimit() {
    uint32_t maxPerSec = maxLogsPerSecond.load();
    if (maxPerSec == 0) return true;  // Unlimited

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

// Global logger getter
Logger& getLogger() {
    return Logger::getInstance();
}