// LogInterface.h - Zero-overhead logging interface
#ifndef LOG_INTERFACE_H
#define LOG_INTERFACE_H

#include <esp_log.h>
#include <stdarg.h>

// Function pointer type for custom log implementation
typedef void (*custom_log_function_t)(esp_log_level_t level, const char* tag, const char* format, va_list args);

#ifdef USE_CUSTOM_LOGGER
    // When custom logger is enabled, use external functions
    extern "C" {
        void custom_log_write(esp_log_level_t level, const char* tag, const char* format, va_list args);
        bool custom_log_is_enabled(esp_log_level_t level);
        bool custom_log_is_enabled_for_tag(esp_log_level_t level, const char* tag);
    }
    
    // Core logging function that routes to custom implementation with tag-level filtering
    static inline void log_write_impl(esp_log_level_t level, const char* tag, const char* format, ...) {
        // Use tag-aware level checking for better performance
        if (custom_log_is_enabled_for_tag(level, tag)) {
            va_list args;
            va_start(args, format);
            custom_log_write(level, tag, format, args);
            va_end(args);
        }
    }
    
    #define LOG_WRITE(level, tag, format, ...) log_write_impl(level, tag, format, ##__VA_ARGS__)
#else
    // When custom logger is disabled, use ESP-IDF directly
    #define LOG_WRITE(level, tag, format, ...) ESP_LOG_LEVEL(level, tag, format, ##__VA_ARGS__)
    
    // ESP-IDF level checking
    #define custom_log_is_enabled(level) (level <= CONFIG_LOG_MAXIMUM_LEVEL)
#endif

// Standard logging macros used by all libraries
// Guard against redefinition to prevent warnings
#ifndef LOG_ERROR
#define LOG_ERROR(tag, format, ...)   LOG_WRITE(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__)
#endif

#ifndef LOG_WARN
#define LOG_WARN(tag, format, ...)    LOG_WRITE(ESP_LOG_WARN, tag, format, ##__VA_ARGS__)
#endif

#ifndef LOG_INFO
#define LOG_INFO(tag, format, ...)    LOG_WRITE(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(tag, format, ...)   LOG_WRITE(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#endif

#ifndef LOG_VERBOSE
#define LOG_VERBOSE(tag, format, ...) LOG_WRITE(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__)
#endif

// Convenience macros with automatic tags (optional)
// User must define LOG_TAG before including this header to use these
#ifdef LOG_TAG
    #define LOGE(format, ...) LOG_ERROR(LOG_TAG, format, ##__VA_ARGS__)
    #define LOGW(format, ...) LOG_WARN(LOG_TAG, format, ##__VA_ARGS__)
    #define LOGI(format, ...) LOG_INFO(LOG_TAG, format, ##__VA_ARGS__)
    #define LOGD(format, ...) LOG_DEBUG(LOG_TAG, format, ##__VA_ARGS__)
    #define LOGV(format, ...) LOG_VERBOSE(LOG_TAG, format, ##__VA_ARGS__)
#endif

#endif // LOG_INTERFACE_H