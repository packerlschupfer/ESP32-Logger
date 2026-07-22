#ifndef SENSOR_LOGGING_H
#define SENSOR_LOGGING_H

#define SENSOR_LOG_TAG "Sensor"

// Define log levels based on debug flag
#ifdef SENSOR_LIB_DEBUG
    #define SENSOR_LOG_LEVEL_E ESP_LOG_ERROR
    #define SENSOR_LOG_LEVEL_W ESP_LOG_WARN
    #define SENSOR_LOG_LEVEL_I ESP_LOG_INFO
    #define SENSOR_LOG_LEVEL_D ESP_LOG_DEBUG
    #define SENSOR_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    #define SENSOR_LOG_LEVEL_E ESP_LOG_ERROR
    #define SENSOR_LOG_LEVEL_W ESP_LOG_WARN
    #define SENSOR_LOG_LEVEL_I ESP_LOG_INFO
    #define SENSOR_LOG_LEVEL_D ESP_LOG_NONE
    #define SENSOR_LOG_LEVEL_V ESP_LOG_NONE
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include "LogInterface.h"
    #define SENSOR_LOG_E(...) LOG_WRITE(SENSOR_LOG_LEVEL_E, SENSOR_LOG_TAG, __VA_ARGS__)
    #define SENSOR_LOG_W(...) LOG_WRITE(SENSOR_LOG_LEVEL_W, SENSOR_LOG_TAG, __VA_ARGS__)
    #define SENSOR_LOG_I(...) LOG_WRITE(SENSOR_LOG_LEVEL_I, SENSOR_LOG_TAG, __VA_ARGS__)
    #define SENSOR_LOG_D(...) LOG_WRITE(SENSOR_LOG_LEVEL_D, SENSOR_LOG_TAG, __VA_ARGS__)
    #define SENSOR_LOG_V(...) LOG_WRITE(SENSOR_LOG_LEVEL_V, SENSOR_LOG_TAG, __VA_ARGS__)
#else
    #include <esp_log.h>
    #define SENSOR_LOG_E(...) ESP_LOGE(SENSOR_LOG_TAG, __VA_ARGS__)
    #define SENSOR_LOG_W(...) ESP_LOGW(SENSOR_LOG_TAG, __VA_ARGS__)
    #define SENSOR_LOG_I(...) ESP_LOGI(SENSOR_LOG_TAG, __VA_ARGS__)
    #ifdef SENSOR_LIB_DEBUG
        #define SENSOR_LOG_D(...) ESP_LOGD(SENSOR_LOG_TAG, __VA_ARGS__)
        #define SENSOR_LOG_V(...) ESP_LOGV(SENSOR_LOG_TAG, __VA_ARGS__)
    #else
        #define SENSOR_LOG_D(...) ((void)0)
        #define SENSOR_LOG_V(...) ((void)0)
    #endif
#endif

#endif // SENSOR_LOGGING_H