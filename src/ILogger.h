// ILogger.h
#pragma once

#include <esp_log.h> // Include the ESP-IDF logging header

class ILogger {
public:
    virtual ~ILogger() {}

    /**
     * @brief Logs a message.
     * 
     * @param level The log level of the message.
     * @param tag The tag associated with the log message.
     * @param format The format string for the log message, followed by arguments.
     */
    virtual void log(esp_log_level_t level, const char* tag, const char* format, ...) = 0;

    /**
     * @brief Logs a message with a newline appended.
     * 
     * @param format The format string for the log message, followed by arguments.
     */
    virtual void logInL(const char* format, ...) = 0;

    /**
     * @brief Logs a message without a newline appended.
     * 
     * @param level The log level of the message.
     * @param tag The tag associated with the log message.
     * @param format The format string for the log message, followed by arguments.
     */
    virtual void logNnL(esp_log_level_t level, const char* tag, const char* format, ...) = 0;

    /**
     * @brief Logs a formatted message using a variable argument list.
     *
     * @param level The log level of the message.
     * @param tag The tag associated with the log message.
     * @param format The format string for the log message.
     * @param args A va_list of arguments for the format string.
     */
    virtual void logV(esp_log_level_t level, const char* tag, const char* format, va_list args) = 0;

    virtual void setLogLevel(esp_log_level_t level) = 0;
    virtual esp_log_level_t getLogLevel() const = 0;
    
    /**
     * @brief Flush any buffered log output.
     */
    virtual void flush() = 0;
};
