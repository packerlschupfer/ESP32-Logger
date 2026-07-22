#ifndef WIFI_MANAGER_LOGGING_H
#define WIFI_MANAGER_LOGGING_H

#define WIFI_LOG_TAG "WiFiManager"

// Define log levels based on debug flag
#ifdef WIFI_MANAGER_DEBUG
    // Debug mode: Show all levels
    #define WIFI_LOG_LEVEL_E ESP_LOG_ERROR
    #define WIFI_LOG_LEVEL_W ESP_LOG_WARN
    #define WIFI_LOG_LEVEL_I ESP_LOG_INFO
    #define WIFI_LOG_LEVEL_D ESP_LOG_DEBUG
    #define WIFI_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    // Release mode: Only Error, Warn, Info
    #define WIFI_LOG_LEVEL_E ESP_LOG_ERROR
    #define WIFI_LOG_LEVEL_W ESP_LOG_WARN
    #define WIFI_LOG_LEVEL_I ESP_LOG_INFO
    #define WIFI_LOG_LEVEL_D ESP_LOG_NONE  // Suppress
    #define WIFI_LOG_LEVEL_V ESP_LOG_NONE  // Suppress
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include "LogInterface.h"
    #define WIFI_LOG_E(...) LOG_WRITE(WIFI_LOG_LEVEL_E, WIFI_LOG_TAG, __VA_ARGS__)
    #define WIFI_LOG_W(...) LOG_WRITE(WIFI_LOG_LEVEL_W, WIFI_LOG_TAG, __VA_ARGS__)
    #define WIFI_LOG_I(...) LOG_WRITE(WIFI_LOG_LEVEL_I, WIFI_LOG_TAG, __VA_ARGS__)
    #define WIFI_LOG_D(...) LOG_WRITE(WIFI_LOG_LEVEL_D, WIFI_LOG_TAG, __VA_ARGS__)
    #define WIFI_LOG_V(...) LOG_WRITE(WIFI_LOG_LEVEL_V, WIFI_LOG_TAG, __VA_ARGS__)
#else
    // ESP-IDF logging with compile-time suppression
    #include <esp_log.h>
    #define WIFI_LOG_E(...) ESP_LOGE(WIFI_LOG_TAG, __VA_ARGS__)
    #define WIFI_LOG_W(...) ESP_LOGW(WIFI_LOG_TAG, __VA_ARGS__)
    #define WIFI_LOG_I(...) ESP_LOGI(WIFI_LOG_TAG, __VA_ARGS__)
    #ifdef WIFI_MANAGER_DEBUG
        #define WIFI_LOG_D(...) ESP_LOGD(WIFI_LOG_TAG, __VA_ARGS__)
        #define WIFI_LOG_V(...) ESP_LOGV(WIFI_LOG_TAG, __VA_ARGS__)
    #else
        #define WIFI_LOG_D(...) ((void)0)
        #define WIFI_LOG_V(...) ((void)0)
    #endif
#endif

#endif // WIFI_MANAGER_LOGGING_H