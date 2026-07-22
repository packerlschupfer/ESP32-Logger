/*
 * Multi-Library Logger Example
 * 
 * This example demonstrates how to use the custom Logger across multiple
 * libraries with minimal resource usage. All libraries share a single
 * Logger instance (~17KB total).
 * 
 * The example includes three libraries:
 * - SensorLibrary: Simulates sensor readings with various log levels
 * - NetworkLibrary: Simulates network operations and connectivity
 * - StorageLibrary: Simulates data storage with statistics
 * 
 * All libraries use LogInterface.h which provides zero-overhead logging
 * when USE_CUSTOM_LOGGER is not defined.
 */

#include <Arduino.h>
#include <inttypes.h>

// Include Logger - LogInterfaceImpl.cpp is automatically compiled by the library
#include "Logger.h"

// Include all libraries that use LogInterface
#include "SensorLibrary.h"
#include "NetworkLibrary.h"
#include "StorageLibrary.h"

// Create library instances
SensorLibrary sensor;
NetworkLibrary network;
StorageLibrary storage;

// Timing variables
unsigned long lastStatusTime = 0;
unsigned long lastSaveTime = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Multi-Library Logger Example ===\n");
    
    // Initialize Logger once for all libraries
    Logger& logger = Logger::getInstance();
    logger.init(1024);                    // 1KB buffer
    logger.setLogLevel(ESP_LOG_VERBOSE);  // Show all log levels
    logger.enableLogging(true);
    logger.setMaxLogsPerSecond(100);      // Prevent log flooding
    
    // Log from main application
    logger.log(ESP_LOG_INFO, "Main", "Starting multi-library example...");
    logger.log(ESP_LOG_DEBUG, "Main", "Logger initialized with 1KB buffer");
    
    // Initialize all libraries - they will all use the same Logger instance
    Serial.println("\n--- Initializing Libraries ---");
    
    // Initialize sensor
    if (!sensor.begin()) {
        logger.log(ESP_LOG_ERROR, "Main", "Failed to initialize sensor!");
    }
    
    // Initialize network
    if (!network.begin("TestNetwork", "password123")) {
        logger.log(ESP_LOG_ERROR, "Main", "Failed to initialize network!");
    }
    
    // Initialize storage
    if (!storage.begin()) {
        logger.log(ESP_LOG_ERROR, "Main", "Failed to initialize storage!");
    }
    
    Serial.println("\n--- Setup Complete ---\n");
    logger.log(ESP_LOG_INFO, "Main", "All libraries initialized successfully");
    
    // Show initial memory usage
    logger.log(ESP_LOG_INFO, "Main", "Free heap: %d bytes", ESP.getFreeHeap());
}

void loop() {
    unsigned long currentTime = millis();
    
    // Update sensor readings
    if (sensor.update()) {
        // New sensor data available
        float temp = sensor.getTemperature();
        float humidity = sensor.getHumidity();
        
        // Send data over network
        char dataBuffer[64];
        snprintf(dataBuffer, sizeof(dataBuffer), 
                 "{\"temp\":%.1f,\"humidity\":%.1f,\"time\":%lu}", 
                 temp, humidity, currentTime);
        
        network.sendData(dataBuffer, strlen(dataBuffer));
        
        // Save to storage periodically
        if (currentTime - lastSaveTime > 10000) { // Every 10 seconds
            storage.saveRecord(temp, humidity);
            lastSaveTime = currentTime;
        }
    }
    
    // Update network signal strength
    network.updateSignalStrength();
    
    // Ping network periodically
    network.ping();
    
    // Print status every 30 seconds
    if (currentTime - lastStatusTime > 30000) {
        Serial.println("\n=== Status Report ===");
        
        // Log from main
        Logger::getInstance().log(ESP_LOG_INFO, "Main", 
                                  "System uptime: %lu seconds", currentTime / 1000);
        
        // Network status
        Logger::getInstance().log(ESP_LOG_INFO, "Main", 
                                  "Network connected: %s, Signal: %d dBm",
                                  network.isConnected() ? "Yes" : "No",
                                  network.getSignalStrength());
        
        // Storage statistics
        storage.printStats();
        
        // Memory usage
        Logger::getInstance().log(ESP_LOG_INFO, "Main", 
                                  "Free heap: %d bytes", ESP.getFreeHeap());
        
        // Logger statistics
        Logger::getInstance().log(ESP_LOG_INFO, "Main",
                                  "Logger stats - Dropped: %" PRIu32 ", Mutex timeouts: %" PRIu32,
                                  Logger::getInstance().getDroppedLogs(),
                                  Logger::getInstance().getMutexTimeouts());
        
        // Simulate occasional errors for demonstration
        if (random(3) == 0) {
            Serial.println("\n--- Simulating Errors ---");
            sensor.simulateError();
        }
        
        lastStatusTime = currentTime;
    }
    
    // Small delay to prevent watchdog issues
    delay(100);
}