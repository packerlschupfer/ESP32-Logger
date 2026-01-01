// LogInterfaceImpl.cpp - Implementation of LogInterface for custom Logger
// This file should be compiled once in the main application when using USE_CUSTOM_LOGGER

#ifdef USE_CUSTOM_LOGGER

#include "LogInterface.h"
#include "Logger.h"

extern "C" {

void custom_log_write(esp_log_level_t level, const char* tag, const char* format, va_list args) {
    // Use the Logger singleton with zero stack overhead
    Logger::getInstance().logV(level, tag, format, args);
}

bool custom_log_is_enabled(esp_log_level_t level) {
    // Quick global check first
    Logger& logger = Logger::getInstance();
    return logger.getIsLoggingEnabled() && (level <= logger.getLogLevel());
}

// Extended version that checks tag-specific levels
bool custom_log_is_enabled_for_tag(esp_log_level_t level, const char* tag) {
    return Logger::getInstance().isLevelEnabledForTag(tag, level);
}

} // extern "C"

#endif // USE_CUSTOM_LOGGER