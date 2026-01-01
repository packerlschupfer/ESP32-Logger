/*
 * Per-Library Debug Control Example
 * 
 * Demonstrates how to control logging verbosity for individual libraries
 * using compile-time flags.
 */

#include <Arduino.h>

// Include logger if using custom logger
#ifdef USE_CUSTOM_LOGGER
    #include "Logger.h"
    // Note: LogInterfaceImpl.cpp is automatically compiled by the library
#endif

// Include example libraries
#include "WiFiManagerLib.h"
#include "ModbusRTULib.h"
#include "SensorLib.h"

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        delay(10);
    }
    
    Serial.println("\n========================================");
    Serial.println("Per-Library Debug Control Example");
    Serial.println("========================================");
    
    // Show which debug flags are active
    Serial.println("Active debug flags:");
    #ifdef USE_CUSTOM_LOGGER
        Serial.println("- USE_CUSTOM_LOGGER");
    #endif
    #ifdef WIFI_MANAGER_DEBUG
        Serial.println("- WIFI_MANAGER_DEBUG");
    #endif
    #ifdef MODBUS_RTU_DEBUG
        Serial.println("- MODBUS_RTU_DEBUG");
    #endif
    #ifdef SENSOR_LIB_DEBUG
        Serial.println("- SENSOR_LIB_DEBUG");
    #endif
    
    Serial.println("========================================\n");
    
    #ifdef USE_CUSTOM_LOGGER
        // Initialize custom logger
        Logger& logger = Logger::getInstance();
        logger.init(1024);
        logger.setLogLevel(ESP_LOG_VERBOSE);  // Allow all levels
        logger.enableLogging(true);
        Serial.println("Custom logger initialized\n");
    #endif
    
    // Initialize libraries
    WiFiManagerLib wifi;
    ModbusRTULib modbus;
    SensorLib sensor;
    
    Serial.println("=== Library Initialization ===");
    wifi.begin();
    modbus.begin();
    sensor.begin();
    
    Serial.println("\n=== Library Operations ===");
    wifi.connect("TestNetwork", "password123");
    modbus.readRegister(0x01, 0x1000);
    sensor.readTemperature();
    
    Serial.println("\n=== Simulated Errors ===");
    wifi.simulateError();
    modbus.simulateTimeout();
    sensor.simulateOutOfRange();
    
    Serial.println("\n========================================");
    Serial.println("Notice how debug output varies based on");
    Serial.println("which debug flags are enabled!");
    Serial.println("========================================");
}

void loop() {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 10000) {
        lastUpdate = millis();
        
        Serial.println("\n=== Periodic Update ===");
        
        WiFiManagerLib wifi;
        ModbusRTULib modbus;
        SensorLib sensor;
        
        wifi.getStatus();
        modbus.getStatistics();
        sensor.readAllSensors();
    }
    
    delay(100);
}