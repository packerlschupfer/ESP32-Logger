#ifndef MODBUS_RTU_LOGGING_H
#define MODBUS_RTU_LOGGING_H

#define MODBUS_LOG_TAG "ModbusRTU"

// Additional debug features for Modbus
#ifdef MODBUS_RTU_DEBUG
    #define MODBUS_LOG_PACKETS      // Enable packet logging
    #define MODBUS_LOG_TIMING       // Enable timing analysis
#endif

// Define log levels based on debug flag
#ifdef MODBUS_RTU_DEBUG
    #define MODBUS_LOG_LEVEL_E ESP_LOG_ERROR
    #define MODBUS_LOG_LEVEL_W ESP_LOG_WARN
    #define MODBUS_LOG_LEVEL_I ESP_LOG_INFO
    #define MODBUS_LOG_LEVEL_D ESP_LOG_DEBUG
    #define MODBUS_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    #define MODBUS_LOG_LEVEL_E ESP_LOG_ERROR
    #define MODBUS_LOG_LEVEL_W ESP_LOG_WARN
    #define MODBUS_LOG_LEVEL_I ESP_LOG_INFO
    #define MODBUS_LOG_LEVEL_D ESP_LOG_NONE
    #define MODBUS_LOG_LEVEL_V ESP_LOG_NONE
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include "LogInterface.h"
    #define MODBUS_LOG_E(...) LOG_WRITE(MODBUS_LOG_LEVEL_E, MODBUS_LOG_TAG, __VA_ARGS__)
    #define MODBUS_LOG_W(...) LOG_WRITE(MODBUS_LOG_LEVEL_W, MODBUS_LOG_TAG, __VA_ARGS__)
    #define MODBUS_LOG_I(...) LOG_WRITE(MODBUS_LOG_LEVEL_I, MODBUS_LOG_TAG, __VA_ARGS__)
    #define MODBUS_LOG_D(...) LOG_WRITE(MODBUS_LOG_LEVEL_D, MODBUS_LOG_TAG, __VA_ARGS__)
    #define MODBUS_LOG_V(...) LOG_WRITE(MODBUS_LOG_LEVEL_V, MODBUS_LOG_TAG, __VA_ARGS__)
#else
    #include <esp_log.h>
    #define MODBUS_LOG_E(...) ESP_LOGE(MODBUS_LOG_TAG, __VA_ARGS__)
    #define MODBUS_LOG_W(...) ESP_LOGW(MODBUS_LOG_TAG, __VA_ARGS__)
    #define MODBUS_LOG_I(...) ESP_LOGI(MODBUS_LOG_TAG, __VA_ARGS__)
    #ifdef MODBUS_RTU_DEBUG
        #define MODBUS_LOG_D(...) ESP_LOGD(MODBUS_LOG_TAG, __VA_ARGS__)
        #define MODBUS_LOG_V(...) ESP_LOGV(MODBUS_LOG_TAG, __VA_ARGS__)
    #else
        #define MODBUS_LOG_D(...) ((void)0)
        #define MODBUS_LOG_V(...) ((void)0)
    #endif
#endif

// Conditional packet logging
#ifdef MODBUS_LOG_PACKETS
    #define MODBUS_LOG_PACKET(msg, data, len) do { \
        MODBUS_LOG_D("%s:", msg); \
        for (int i = 0; i < len; i++) { \
            MODBUS_LOG_D("  [%02d] = 0x%02X", i, data[i]); \
        } \
    } while(0)
#else
    #define MODBUS_LOG_PACKET(msg, data, len) ((void)0)
#endif

// Conditional timing logging
#ifdef MODBUS_LOG_TIMING
    #define MODBUS_LOG_TIME(msg, ms) MODBUS_LOG_V("Timing: %s took %lu ms", msg, ms)
#else
    #define MODBUS_LOG_TIME(msg, ms) ((void)0)
#endif

#endif // MODBUS_RTU_LOGGING_H