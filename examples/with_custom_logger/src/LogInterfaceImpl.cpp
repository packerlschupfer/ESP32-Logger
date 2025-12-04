// LogInterfaceImpl.cpp - Implementation for custom Logger
#ifdef USE_CUSTOM_LOGGER

#include "LogInterface.h"
#include "Logger.h"

extern "C" {
    
void custom_log_write(esp_log_level_t level, const char* tag, const char* format, va_list args) {
    // Use the Logger singleton
    Logger::getInstance().logV(level, tag, format, args);
}

bool custom_log_is_enabled(esp_log_level_t level) {
    // Check if logging is enabled and level is appropriate
    Logger& logger = Logger::getInstance();
    return logger.getIsLoggingEnabled() && (level <= logger.getLogLevel());
}

} // extern "C"

#endif // USE_CUSTOM_LOGGER