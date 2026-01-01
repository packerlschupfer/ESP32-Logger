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
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string>
#include <inttypes.h>
#include <cstring>
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

#ifndef CONFIG_LOG_SUBSCRIBER_QUEUE_SIZE
#define CONFIG_LOG_SUBSCRIBER_QUEUE_SIZE 16  // Queue depth for async subscriber notifications
#endif

#ifndef CONFIG_LOG_SUBSCRIBER_TASK_STACK
#define CONFIG_LOG_SUBSCRIBER_TASK_STACK 3072  // Stack size for subscriber task
#endif

#ifndef CONFIG_LOG_SUBSCRIBER_TASK_PRIORITY
#define CONFIG_LOG_SUBSCRIBER_TASK_PRIORITY 2  // Priority for subscriber task
#endif

#ifndef CONFIG_LOG_SUBSCRIBER_TAG_SIZE
#define CONFIG_LOG_SUBSCRIBER_TAG_SIZE 32  // Max tag length in queued messages
#endif

#ifndef CONFIG_LOG_SUBSCRIBER_MSG_SIZE
#define CONFIG_LOG_SUBSCRIBER_MSG_SIZE 200  // Max message length in queued messages
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
    ~BufferPool() {
        // Clean up mutex if it was created
        if (poolMutex) {
            vSemaphoreDelete(poolMutex);
            poolMutex = nullptr;
        }
    }

    std::array<Buffer, CONFIG_LOG_BUFFER_POOL_SIZE> pool;
    SemaphoreHandle_t poolMutex;
};

/**
 * @brief RAII wrapper for BufferPool to ensure proper release
 *
 * Usage:
 *   BufferGuard guard;
 *   if (guard.get()) {
 *       snprintf(guard.get(), BufferPool::BUFFER_SIZE, "...");
 *   }
 */
class BufferGuard {
public:
    BufferGuard() : buffer_(BufferPool::getInstance().acquire()) {}
    ~BufferGuard() {
        if (buffer_) {
            BufferPool::getInstance().release(buffer_);
        }
    }

    // Non-copyable
    BufferGuard(const BufferGuard&) = delete;
    BufferGuard& operator=(const BufferGuard&) = delete;

    // Movable
    BufferGuard(BufferGuard&& other) noexcept : buffer_(other.buffer_) {
        other.buffer_ = nullptr;
    }
    BufferGuard& operator=(BufferGuard&& other) noexcept {
        if (this != &other) {
            if (buffer_) BufferPool::getInstance().release(buffer_);
            buffer_ = other.buffer_;
            other.buffer_ = nullptr;
        }
        return *this;
    }

    char* get() noexcept { return buffer_; }
    const char* get() const noexcept { return buffer_; }
    explicit operator bool() const noexcept { return buffer_ != nullptr; }

private:
    char* buffer_;
};

/**
 * @brief Message structure for async subscriber notifications
 *
 * Fixed-size struct for queue-based callback invocation.
 * Callbacks execute on dedicated task, ensuring thread/core safety.
 */
struct LogSubscriberMessage {
    esp_log_level_t level;
    char tag[CONFIG_LOG_SUBSCRIBER_TAG_SIZE];
    char message[CONFIG_LOG_SUBSCRIBER_MSG_SIZE];
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

    // Log subscriber callbacks (lightweight forwarding to external systems)
    typedef void (*LogSubscriberCallback)(
        esp_log_level_t level,
        const char* tag,
        const char* message
    );

    /**
     * @brief Register a callback to receive all log messages
     * @param callback Function to call for each log message
     * @return true if registered successfully, false if max subscribers reached
     * @note Callbacks execute on dedicated task - safe for network operations
     */
    bool addLogSubscriber(LogSubscriberCallback callback);

    /**
     * @brief Unregister a previously registered callback
     * @param callback Function to unregister
     * @return true if found and removed, false otherwise
     */
    bool removeLogSubscriber(LogSubscriberCallback callback);

    /**
     * @brief Get number of active subscribers
     * @return Number of registered callbacks
     * @note Thread-safe (uses atomic counter)
     */
    uint8_t getSubscriberCount() const noexcept { return subscriberCount.load(); }

    /**
     * @brief Start the subscriber notification task
     * @param coreId Core to pin task to (-1 for no affinity, 0 or 1 for specific core)
     * @return true if task started successfully
     * @note Call this after registering subscribers that need core affinity (e.g., network)
     */
    bool startSubscriberTask(int coreId = -1);

    /**
     * @brief Stop the subscriber notification task
     */
    void stopSubscriberTask();

    /**
     * @brief Check if subscriber task is running
     * @return true if task is active
     */
    bool isSubscriberTaskRunning() const { return subscriberTaskHandle != nullptr; }

    // Core logging methods
    void log(esp_log_level_t level, const char* tag, const char* format, ...) override;
    void logNnL(esp_log_level_t level, const char* tag, const char* format, ...) override;
    void logV(esp_log_level_t level, const char* tag, const char* format, va_list args) override;
    void logInL(const char* format, ...) override;

    // Direct mode for bypassing rate limiting
    void logDirect(esp_log_level_t level, const char* tag, const char* message);

    // Metrics
    uint32_t getDroppedLogs() const noexcept { return droppedLogs.load(); }
    uint32_t getMutexTimeouts() const noexcept { return mutexTimeouts_.load(); }
    void resetDroppedLogs();
    void resetMutexTimeouts() { mutexTimeouts_.store(0); }

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
    void notifySubscribers(esp_log_level_t level, const char* tag, const char* message);

    // Core state with atomic operations for thread safety
    std::atomic<bool> initialized_{false};
    std::atomic<esp_log_level_t> globalLogLevel{ESP_LOG_INFO};
    std::atomic<bool> isLoggingEnabled{true};

    // Multiple backend support
    std::vector<std::shared_ptr<ILogBackend>> backends;
    mutable SemaphoreHandle_t backendMutex;

    // Log subscribers (async queue-based callbacks)
    static constexpr uint8_t MAX_SUBSCRIBERS = 4;
    LogSubscriberCallback subscribers[MAX_SUBSCRIBERS] = {nullptr};
    std::atomic<uint8_t> subscriberCount{0};
    mutable SemaphoreHandle_t subscriberMutex;
    QueueHandle_t subscriberQueue = nullptr;
    TaskHandle_t subscriberTaskHandle = nullptr;
    std::atomic<bool> subscriberTaskRunning{false};

    // Static task function for subscriber notifications
    static void subscriberTaskFunc(void* param);

    // Tag-level filtering - uses fixed array for O(n) lookup without heap allocation
    // Better than unordered_map for small N due to cache locality and no allocation per lookup
    struct TagLevel {
        char tag[CONFIG_LOG_SUBSCRIBER_TAG_SIZE];  // Owned copy of tag string
        esp_log_level_t level;
    };
    TagLevel tagLevels_[CONFIG_LOG_MAX_TAGS];
    std::atomic<size_t> tagLevelCount_{0};
    mutable SemaphoreHandle_t tagMutex;

    // Rate limiting
    std::atomic<uint32_t> maxLogsPerSecond{MAX_LOGS_PER_SECOND};
    SemaphoreHandle_t rateLimitMutex;
    uint32_t lastLogTime;
    uint32_t logCounter;
    std::atomic<uint32_t> droppedLogs{0};

    // Diagnostic counters for mutex timeouts
    std::atomic<uint32_t> mutexTimeouts_{0};
};

// Global logger instance getter
Logger& getLogger();