// include/LoggingMacros.h
#ifndef LOGGING_MACROS_H
#define LOGGING_MACROS_H

#include <esp_log.h>
#include "Logger.h"

// Get logger instance - uses singleton pattern
inline Logger& getLoggerInstance() {
    return Logger::getInstance();
}

// Define common logging macros that use the Logger instance
#ifndef LOG_ERROR
#define LOG_ERROR(tag, fmt, ...) getLoggerInstance().log(ESP_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_WARN
#define LOG_WARN(tag, fmt, ...) getLoggerInstance().log(ESP_LOG_WARN, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_INFO
#define LOG_INFO(tag, fmt, ...) getLoggerInstance().log(ESP_LOG_INFO, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(tag, fmt, ...) getLoggerInstance().log(ESP_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_VERBOSE
#define LOG_VERBOSE(tag, fmt, ...) getLoggerInstance().log(ESP_LOG_VERBOSE, tag, fmt, ##__VA_ARGS__)
#endif

// Conditional logging based on build mode
#ifdef LOG_MODE_RELEASE
    // In release mode, debug and verbose are no-ops
    #undef LOG_DEBUG
    #undef LOG_VERBOSE
    #define LOG_DEBUG(tag, fmt, ...) ((void)0)
    #define LOG_VERBOSE(tag, fmt, ...) ((void)0)
#endif

#ifdef LOG_MODE_DEBUG_SELECTIVE
    // In selective debug, verbose is no-op
    #undef LOG_VERBOSE
    #define LOG_VERBOSE(tag, fmt, ...) ((void)0)
#endif

// Helper macros for common patterns
#define LOG_FUNC_ENTER(tag) LOG_DEBUG(tag, "%s: enter", __func__)
#define LOG_FUNC_EXIT(tag) LOG_DEBUG(tag, "%s: exit", __func__)
#define LOG_FUNC_ERROR(tag, msg) LOG_ERROR(tag, "%s: %s", __func__, msg)

#endif // LOGGING_MACROS_H