/**
 * Boiler Controller Logger Example
 * 
 * This example shows how to properly configure the Logger for a real-world
 * application that uses multiple libraries (RYN4, ModbusRTU, MQTT, etc.)
 * to prevent system freezes from log flooding.
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Logger.h"
#include "NonBlockingConsoleBackend.h"
#include "LogInterface.h"

// Application log tags
const char* TAG_MAIN = "BoilerCtrl";
const char* TAG_SAFETY = "Safety";
const char* TAG_TEMP = "Temperature";
const char* TAG_PUMP = "Pump";

// Simulated libraries that flood during init
void simulateRYN4Init() {
    // RYN4 is notorious for flooding during init
    for (int i = 0; i < 100; i++) {
        LOG_DEBUG("RYN4", "Initializing module component %d...", i);
        LOG_VERBOSE("RYN4", "Setting register 0x%04X to value 0x%02X", i * 4, i);
    }
    LOG_INFO("RYN4", "RYN4 initialization complete");
}

void simulateModbusInit() {
    for (int i = 0; i < 50; i++) {
        LOG_DEBUG("ModbusRTU", "Scanning device at address %d", i);
    }
    LOG_INFO("ModbusRTU", "Modbus initialization complete");
}

void simulateMQTTInit() {
    LOG_INFO("MQTT", "Connecting to broker...");
    for (int i = 0; i < 30; i++) {
        LOG_DEBUG("MQTT", "Connection attempt %d", i);
    }
    LOG_INFO("MQTT", "MQTT connected successfully");
}

// Global logger reference
Logger& logger = Logger::getInstance();
std::shared_ptr<NonBlockingConsoleBackend> nbBackend;

// Task handles
TaskHandle_t monitorTaskHandle = NULL;
TaskHandle_t controlTaskHandle = NULL;

// Simulated sensor values
volatile float boilerTemp = 65.0;
volatile float returnTemp = 45.0;
volatile bool pumpRunning = false;
volatile bool overTempAlarm = false;

// Monitor task - logs system statistics
void monitorTask(void* param) {
    uint32_t lastStatsTime = 0;
    const uint32_t STATS_INTERVAL = 10000;  // 10 seconds
    
    while (true) {
        uint32_t now = millis();
        
        // Log system stats periodically
        if (now - lastStatsTime >= STATS_INTERVAL) {
            LOG_INFO(TAG_MAIN, "=== System Status ===");
            LOG_INFO(TAG_TEMP, "Boiler: %.1f°C, Return: %.1f°C", boilerTemp, returnTemp);
            LOG_INFO(TAG_PUMP, "Pump: %s", pumpRunning ? "RUNNING" : "STOPPED");
            LOG_INFO(TAG_MAIN, "Free heap: %d bytes", ESP.getFreeHeap());
            LOG_INFO(TAG_MAIN, "Uptime: %lu seconds", now / 1000);
            
            // Show logger statistics
            if (nbBackend) {
                uint32_t dropped = nbBackend->getDroppedMessages();
                if (dropped > 0) {
                    LOG_WARN(TAG_MAIN, "Logger dropped %lu messages", dropped);
                    LOG_INFO(TAG_MAIN, "Consider increasing baud rate or reducing log verbosity");
                }
            }
            
            lastStatsTime = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Control task - simulates boiler control with logging
void controlTask(void* param) {
    const float TEMP_SETPOINT = 70.0;
    const float TEMP_HYSTERESIS = 2.0;
    const float OVERHEAT_THRESHOLD = 85.0;
    
    while (true) {
        // Simulate temperature changes
        if (pumpRunning) {
            boilerTemp += 0.5;  // Heating
            returnTemp += 0.3;
        } else {
            boilerTemp -= 0.2;  // Cooling
            returnTemp -= 0.1;
        }
        
        // Safety check - this is critical and should always log
        if (boilerTemp > OVERHEAT_THRESHOLD) {
            if (!overTempAlarm) {
                LOG_ERROR(TAG_SAFETY, "OVERHEAT ALARM! Temperature: %.1f°C", boilerTemp);
                overTempAlarm = true;
                pumpRunning = false;  // Emergency stop
            }
        } else {
            overTempAlarm = false;
        }
        
        // Normal control logic
        if (boilerTemp < TEMP_SETPOINT - TEMP_HYSTERESIS && !overTempAlarm) {
            if (!pumpRunning) {
                LOG_INFO(TAG_PUMP, "Starting pump - temp below setpoint");
                pumpRunning = true;
            }
        } else if (boilerTemp > TEMP_SETPOINT + TEMP_HYSTERESIS) {
            if (pumpRunning) {
                LOG_INFO(TAG_PUMP, "Stopping pump - temp above setpoint");
                pumpRunning = false;
            }
        }
        
        // Debug logging (will be dropped if buffer full)
        LOG_DEBUG(TAG_TEMP, "Control loop: Boiler=%.1f, Return=%.1f, Pump=%d", 
                  boilerTemp, returnTemp, pumpRunning);
        
        vTaskDelay(pdMS_TO_TICKS(500));  // 2Hz control loop
    }
}

void setup() {
    // CRITICAL: Use fast baud rate to prevent dropping messages
    Serial.begin(921600);  // Much better than 115200!
    delay(2000);          // Let serial stabilize
    
    Serial.println("\n\n=== Boiler Controller Starting ===");
    Serial.println("Logger configuration example for high-volume logging\n");
    
    // The logger already uses NonBlockingConsoleBackend by default
    // But we'll get a reference for statistics
    nbBackend = std::make_shared<NonBlockingConsoleBackend>();
    logger.setBackend(nbBackend);
    
    // Configure global log level
    logger.setLogLevel(ESP_LOG_INFO);  // Default level
    
    // CRITICAL: Configure library log levels to prevent flooding
    Serial.println("Configuring library log levels to prevent flooding:");
    
    // Shut up the noisy libraries
    logger.setTagLevel("RYN4", ESP_LOG_WARN);       // Only warnings and errors
    logger.setTagLevel("ModbusRTU", ESP_LOG_WARN);  // Only warnings and errors
    logger.setTagLevel("WiFi", ESP_LOG_WARN);       // WiFi is chatty
    logger.setTagLevel("MQTT", ESP_LOG_INFO);       // Info and above
    
    // Keep our application verbose for debugging
    logger.setTagLevel(TAG_MAIN, ESP_LOG_DEBUG);    // Main app debug
    logger.setTagLevel(TAG_SAFETY, ESP_LOG_VERBOSE); // Safety always verbose
    logger.setTagLevel(TAG_TEMP, ESP_LOG_INFO);     // Temperature info
    logger.setTagLevel(TAG_PUMP, ESP_LOG_INFO);     // Pump info
    
    Serial.println("Library log levels configured\n");
    
    // Now initialize libraries - they won't freeze the system!
    Serial.println("Initializing libraries (this used to freeze for 10+ seconds)...");
    uint32_t startTime = millis();
    
    simulateRYN4Init();     // Would generate 200+ messages
    simulateModbusInit();   // Would generate 50+ messages  
    simulateMQTTInit();     // Would generate 30+ messages
    
    uint32_t initTime = millis() - startTime;
    LOG_INFO(TAG_MAIN, "All libraries initialized in %lu ms", initTime);
    
    // Check if we dropped messages during init
    if (nbBackend) {
        uint32_t dropped = nbBackend->getDroppedMessages();
        if (dropped > 0) {
            LOG_WARN(TAG_MAIN, "Dropped %lu messages during initialization", dropped);
            LOG_INFO(TAG_MAIN, "This is normal - better than freezing!");
        } else {
            LOG_INFO(TAG_MAIN, "No messages dropped - excellent baud rate!");
        }
        
        // Reset stats for runtime monitoring
        nbBackend->resetStats();
    }
    
    // Create application tasks
    LOG_INFO(TAG_MAIN, "Creating application tasks...");
    
    xTaskCreate(monitorTask, "Monitor", 4096, NULL, 1, &monitorTaskHandle);
    xTaskCreate(controlTask, "Control", 4096, NULL, 2, &controlTaskHandle);
    
    LOG_INFO(TAG_MAIN, "Boiler controller ready!");
    LOG_INFO(TAG_MAIN, "System will remain responsive even under heavy logging");
}

void loop() {
    static uint32_t lastLoopLog = 0;
    static uint32_t loopCounter = 0;
    
    loopCounter++;
    
    // Log loop statistics every 5 seconds
    if (millis() - lastLoopLog >= 5000) {
        LOG_DEBUG(TAG_MAIN, "Main loop alive - iterations: %lu", loopCounter);
        
        // Check logger health
        if (nbBackend && nbBackend->getDroppedMessages() > 100) {
            LOG_WARN(TAG_MAIN, "High number of dropped messages detected");
            // In a real system, you might adjust log levels dynamically here
        }
        
        lastLoopLog = millis();
    }
    
    // Main loop stays responsive!
    delay(10);
}