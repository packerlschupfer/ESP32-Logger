// Logger.h
// Professional Logger with tag-level filtering and memory optimization
// Designed for systems with 15-20 threads and limited stack space

#pragma once

#include <Arduino.h>
#include "ILogger.h"
#include "ILogBackend.h"
#include <memory>
#include <esp32-hal-log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <unordered_map>
#include <string_view>
#include <string>
#include <array>
#include <atomic>
#include <vector>
#include "LoggerConfig.h"

#ifndef CONFIG_LOG_BUFFER_SIZE
#define CONFIG_LOG_BUFFER_SIZE 256  // Reduced for memory efficiency
#endif

#ifndef CONFIG_LOG_MAX_TAGS
#define CONFIG_LOG_MAX_TAGS 32      // Maximum number of tag-specific levels
#endif

#ifndef CONFIG_LOG_BUFFER_POOL_SIZE
#define CONFIG_LOG_BUFFER_POOL_SIZE 8  // Buffer pool size for thread safety
#endif

#define MAX_LOGS_PER_SECOND 100

/**
 * @brief Buffer pool for memory-efficient logging
 *
 * Uses Meyer's singleton pattern to avoid static initialization order issues.
 */
class BufferPool {
public:
    static constexpr size_t BUFFER_SIZE = CONFIG_LOG_BUFFER_SIZE;

    struct Buffer {
        char data[BUFFER_SIZE];
        std::atomic<bool> inUse{false};
    };

    /**
     * @brief Get the singleton instance
     * @return Reference to the BufferPool singleton
     * @note Thread-safe (C++11 guarantee)
     */
    static BufferPool& getInstance() {
        static BufferPool instance;
        return instance;
    }

    // Delete copy/move operations
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    char* acquire();
    void release(char* buffer);

private:
    BufferPool();
    ~BufferPool() = default;

    std::array<Buffer, CONFIG_LOG_BUFFER_POOL_SIZE> pool;
    SemaphoreHandle_t poolMutex;
};

// Professional Logger with tag-level filtering
class Logger : public ILogger {
public:
    Logger();
    Logger(std::shared_ptr<ILogBackend> backend);
    ~Logger();

    static Logger& getInstance();

    // Configuration (thread-safe)
    void init(size_t bufferSize = CONFIG_LOG_BUFFER_SIZE);
    void configure(const LoggerConfig& config);
    void enableLogging(bool enable);
    bool getIsLoggingEnabled() const { return isLoggingEnabled.load(); }

    /**
     * @brief Check if the logger has been explicitly initialized
     * @return true if init() or configure() has been called
     * @note Logger is usable without explicit initialization, this tracks explicit setup
     */
    bool isInitialized() const noexcept { return initialized_.load(); }
    void setLogLevel(esp_log_level_t level);
    esp_log_level_t getLogLevel() const { return globalLogLevel.load(); }
    void setMaxLogsPerSecond(uint32_t maxLogs);
    void setBackend(std::shared_ptr<ILogBackend> newBackend);
    
    // Multiple backend support
    void addBackend(std::shared_ptr<ILogBackend> backend);
    void removeBackend(std::shared_ptr<ILogBackend> backend);
    void clearBackends();

    // Core logging methods
    void log(esp_log_level_t level, const char* tag, const char* format, ...) override;
    void logNnL(esp_log_level_t level, const char* tag, const char* format, ...) override;
    void logV(esp_log_level_t level, const char* tag, const char* format, va_list args) override;
    void logInL(const char* format, ...) override;

    // Direct mode for bypassing rate limiting
    void logDirect(esp_log_level_t level, const char* tag, const char* message);

    // Metrics
    uint32_t getDroppedLogs() const { return droppedLogs.load(); }
    void resetDroppedLogs();

    // Professional tag-level filtering
    void setTagLevel(const char* tag, esp_log_level_t level);
    esp_log_level_t getTagLevel(const char* tag) const;
    bool isLevelEnabledForTag(const char* tag, esp_log_level_t level) const;

    // Convert log level to string
    static const char* levelToString(esp_log_level_t level) {
        switch (level) {
            case ESP_LOG_NONE:    return "N";
            case ESP_LOG_ERROR:   return "E";
            case ESP_LOG_WARN:    return "W";
            case ESP_LOG_INFO:    return "I";
            case ESP_LOG_DEBUG:   return "D";
            case ESP_LOG_VERBOSE: return "V";
            default:              return "?";
        }
    }
    
    // Flush all backends
    void flush() override;
    
    // Enable ESP-IDF log redirection through this logger
    void enableESPLogRedirection();

private:
    bool checkRateLimit();
    void writeToBackends(const char* message, size_t length);

    // Core state with atomic operations for thread safety
    std::atomic<bool> initialized_{false};
    std::atomic<esp_log_level_t> globalLogLevel{ESP_LOG_INFO};
    std::atomic<bool> isLoggingEnabled{true};
    
    // Multiple backend support
    std::vector<std::shared_ptr<ILogBackend>> backends;
    mutable SemaphoreHandle_t backendMutex;
    
    // Tag-level filtering - uses std::string to own tag memory safely
    std::unordered_map<std::string, esp_log_level_t> tagLevels;
    mutable SemaphoreHandle_t tagMutex;
    
    // Rate limiting
    std::atomic<uint32_t> maxLogsPerSecond{MAX_LOGS_PER_SECOND};
    SemaphoreHandle_t rateLimitMutex;
    uint32_t lastLogTime;
    uint32_t logCounter;
    std::atomic<uint32_t> droppedLogs{0};
};

// Global logger instance getter
Logger& getLogger();