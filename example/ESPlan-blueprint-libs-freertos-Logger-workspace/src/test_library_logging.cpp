// Test Library Logging Configuration
// Tests the configureLibraryLogging() pattern from boiler controller

#include <Arduino.h>
#include <Logger.h>
#include <SynchronizedConsoleBackend.h>
#include "LoggingMacros.h"
#include <LogInterfaceImpl.cpp>

// Define log tags for simulated libraries
const char* LOG_TAG_MAIN = "Main";
const char* LOG_TAG_MB8ART = "MB8ART";
const char* LOG_TAG_RYN4 = "RYN4";
const char* LOG_TAG_MODBUS_DEVICE = "ModbusDevice";
const char* LOG_TAG_MODBUS_RTU = "ModbusRTU";
const char* LOG_TAG_ETH = "EthernetManager";
const char* LOG_TAG_TASK_MANAGER = "TaskManager";
const char* LOG_TAG_SEMAPHORE = "SemaphoreGuard";
const char* LOG_TAG_BURNER = "BurnerControl";
const char* LOG_TAG_HEATING = "HeatingControl";
const char* LOG_TAG_PID = "PIDControl";

// Simulate compile flags
#define MB8ART_DEBUG
#define RYN4_DEBUG
// #define MODBUSDEVICE_DEBUG  // Not defined
// #define ESP32MODBUSRTU_DEBUG // Not defined
// #define ETH_DEBUG // Not defined
// #define TASK_MANAGER_DEBUG // Not defined

void configureLibraryLogging() {
    Logger& logger = Logger::getInstance();
    
    // ========== Critical System Components ==========
    logger.setTagLevel("BurnerControl", ESP_LOG_INFO);
    logger.setTagLevel("HeatingControl", ESP_LOG_INFO);
    logger.setTagLevel("PIDControl", ESP_LOG_INFO);
    logger.setTagLevel("SystemInit", ESP_LOG_INFO);
    logger.setTagLevel(LOG_TAG_MAIN, ESP_LOG_INFO);
    
    // ========== Hardware Devices ==========
    #ifdef MB8ART_DEBUG
        logger.setTagLevel("MB8ART", ESP_LOG_DEBUG);
    #else
        logger.setTagLevel("MB8ART", ESP_LOG_INFO);
    #endif
    
    #ifdef RYN4_DEBUG
        logger.setTagLevel("RYN4", ESP_LOG_DEBUG);
    #else
        logger.setTagLevel("RYN4", ESP_LOG_INFO);
    #endif
    
    // Modbus components - usually quite noisy
    #ifdef MODBUSDEVICE_DEBUG
        logger.setTagLevel("ModbusD", ESP_LOG_DEBUG);
        logger.setTagLevel("ModbusDevice", ESP_LOG_DEBUG);
    #else
        logger.setTagLevel("ModbusD", ESP_LOG_WARN);
        logger.setTagLevel("ModbusDevice", ESP_LOG_WARN);
    #endif
    
    #ifdef ESP32MODBUSRTU_DEBUG
        logger.setTagLevel("ModbusRTU", ESP_LOG_DEBUG);
    #else
        logger.setTagLevel("ModbusRTU", ESP_LOG_WARN);
    #endif
    
    // ========== Network Components ==========
    #ifdef ETH_DEBUG
        logger.setTagLevel("ETH", ESP_LOG_DEBUG);
        logger.setTagLevel("EthernetManager", ESP_LOG_DEBUG);
    #else
        logger.setTagLevel("ETH", ESP_LOG_WARN);
        logger.setTagLevel("EthernetManager", ESP_LOG_WARN);
    #endif
    
    // ========== Utility Libraries ==========
    #ifdef TASK_MANAGER_DEBUG
        logger.setTagLevel("TaskManager", ESP_LOG_DEBUG);
    #else
        logger.setTagLevel("TaskManager", ESP_LOG_ERROR);
    #endif
    
    logger.setTagLevel("SemaphoreGuard", ESP_LOG_ERROR);
    logger.setTagLevel("MutexGuard", ESP_LOG_ERROR);
    
    LOG_INFO(LOG_TAG_MAIN, "Library logging configured - Hardware: DEBUG, Network: WARN, Utility: ERROR");
}

void setQuietMode() {
    Logger& logger = Logger::getInstance();
    
    // Set global level to ERROR
    logger.setLogLevel(ESP_LOG_ERROR);
    
    // But keep critical components at WARN level
    logger.setTagLevel("BurnerControl", ESP_LOG_WARN);
    logger.setTagLevel("HeatingControl", ESP_LOG_WARN);
    logger.setTagLevel("SystemInit", ESP_LOG_WARN);
    logger.setTagLevel(LOG_TAG_MAIN, ESP_LOG_WARN);
    
    // Silence everything else
    logger.setTagLevel("MB8ART", ESP_LOG_ERROR);
    logger.setTagLevel("RYN4", ESP_LOG_ERROR);
    logger.setTagLevel("ModbusDevice", ESP_LOG_NONE);
    logger.setTagLevel("ModbusRTU", ESP_LOG_NONE);
    
    LOG_WARN(LOG_TAG_MAIN, "Quiet mode enabled - minimal logging");
}

void setVerboseMode() {
    Logger& logger = Logger::getInstance();
    
    // Set global level to VERBOSE
    logger.setLogLevel(ESP_LOG_VERBOSE);
    
    // Enable verbose logging for all major components
    logger.setTagLevel("MB8ART", ESP_LOG_VERBOSE);
    logger.setTagLevel("RYN4", ESP_LOG_VERBOSE);
    logger.setTagLevel("ModbusDevice", ESP_LOG_DEBUG);
    logger.setTagLevel("ModbusRTU", ESP_LOG_DEBUG);
    logger.setTagLevel("BurnerControl", ESP_LOG_DEBUG);
    logger.setTagLevel("HeatingControl", ESP_LOG_DEBUG);
    
    // But keep utility libraries reasonable
    logger.setTagLevel("TaskManager", ESP_LOG_INFO);
    logger.setTagLevel("SemaphoreGuard", ESP_LOG_WARN);
    
    LOG_INFO(LOG_TAG_MAIN, "Verbose mode enabled - detailed logging");
}

void testLibraryLogging() {
    Serial.println("\r\n=== Testing Library Logging ===\r\n");
    
    // Test logs from different libraries
    LOG_DEBUG(LOG_TAG_MB8ART, "MB8ART debug message - should show (DEBUG enabled)");
    LOG_INFO(LOG_TAG_MB8ART, "MB8ART info message - should show");
    LOG_WARN(LOG_TAG_MB8ART, "MB8ART warning - should show");
    
    LOG_DEBUG(LOG_TAG_RYN4, "RYN4 debug message - should show (DEBUG enabled)");
    LOG_INFO(LOG_TAG_RYN4, "RYN4 info message - should show");
    
    LOG_DEBUG(LOG_TAG_MODBUS_DEVICE, "ModbusDevice debug - should NOT show (WARN level)");
    LOG_WARN(LOG_TAG_MODBUS_DEVICE, "ModbusDevice warning - should show");
    LOG_ERROR(LOG_TAG_MODBUS_DEVICE, "ModbusDevice error - should show");
    
    LOG_DEBUG(LOG_TAG_TASK_MANAGER, "TaskManager debug - should NOT show (ERROR level)");
    LOG_INFO(LOG_TAG_TASK_MANAGER, "TaskManager info - should NOT show");
    LOG_ERROR(LOG_TAG_TASK_MANAGER, "TaskManager error - should show");
    
    LOG_INFO(LOG_TAG_BURNER, "BurnerControl operational - should show");
    LOG_INFO(LOG_TAG_HEATING, "HeatingControl active - should show");
    
    Serial.println("\r\n");
}

void testQuietMode() {
    Serial.println("=== Testing Quiet Mode ===\r\n");
    setQuietMode();
    
    // Test logs - most should be suppressed
    LOG_INFO(LOG_TAG_MB8ART, "MB8ART info - should NOT show (ERROR level in quiet)");
    LOG_ERROR(LOG_TAG_MB8ART, "MB8ART error - should show");
    
    LOG_INFO(LOG_TAG_MODBUS_DEVICE, "ModbusDevice info - should NOT show (NONE level)");
    LOG_ERROR(LOG_TAG_MODBUS_DEVICE, "ModbusDevice error - should NOT show (NONE level)");
    
    LOG_INFO(LOG_TAG_BURNER, "BurnerControl info - should NOT show");
    LOG_WARN(LOG_TAG_BURNER, "BurnerControl warning - should show (WARN level)");
    
    LOG_INFO(LOG_TAG_MAIN, "Main info - should NOT show");
    LOG_WARN(LOG_TAG_MAIN, "Main warning - should show");
    
    Serial.println("\r\n");
}

void testVerboseMode() {
    Serial.println("=== Testing Verbose Mode ===\r\n");
    setVerboseMode();
    
    // Test logs - all should show
    LOG_VERBOSE(LOG_TAG_MB8ART, "MB8ART verbose - should show");
    LOG_DEBUG(LOG_TAG_MB8ART, "MB8ART debug - should show");
    LOG_INFO(LOG_TAG_MB8ART, "MB8ART info - should show");
    
    LOG_DEBUG(LOG_TAG_MODBUS_DEVICE, "ModbusDevice debug - should show");
    LOG_INFO(LOG_TAG_MODBUS_DEVICE, "ModbusDevice info - should show");
    
    LOG_INFO(LOG_TAG_TASK_MANAGER, "TaskManager info - should show (INFO level in verbose)");
    LOG_DEBUG(LOG_TAG_TASK_MANAGER, "TaskManager debug - should NOT show (still INFO level)");
    
    LOG_DEBUG(LOG_TAG_BURNER, "BurnerControl debug - should show");
    
    Serial.println("\r\n");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1000) {
        delay(10);
    }
    delay(100);
    
    Serial.println("\r\n\r\n==================================");
    Serial.println("Library Logging Test");
    Serial.println("==================================\r\n");
    
    // Initialize logger
    Logger& logger = Logger::getInstance();
    logger.setBackend(std::make_shared<SynchronizedConsoleBackend>());
    logger.init(256);
    logger.setLogLevel(ESP_LOG_INFO);  // Default level
    
    Serial.println("Logger initialized\r\n");
    
    // Configure library logging
    configureLibraryLogging();
    
    // Verify tag levels
    Serial.println("=== Tag Level Configuration ===");
    Serial.printf("MB8ART level: %s\r\n", Logger::levelToString(logger.getTagLevel("MB8ART")));
    Serial.printf("RYN4 level: %s\r\n", Logger::levelToString(logger.getTagLevel("RYN4")));
    Serial.printf("ModbusDevice level: %s\r\n", Logger::levelToString(logger.getTagLevel("ModbusDevice")));
    Serial.printf("TaskManager level: %s\r\n", Logger::levelToString(logger.getTagLevel("TaskManager")));
    Serial.printf("BurnerControl level: %s\r\n", Logger::levelToString(logger.getTagLevel("BurnerControl")));
    Serial.println();
    
    // Test normal mode
    testLibraryLogging();
    
    // Test quiet mode
    testQuietMode();
    
    // Return to configured mode
    configureLibraryLogging();
    Serial.println("=== Returned to Normal Mode ===\r\n");
    
    // Test verbose mode
    testVerboseMode();
    
    // Final test - return to normal and verify
    configureLibraryLogging();
    Serial.println("=== Final Test - Normal Mode ===\r\n");
    LOG_DEBUG(LOG_TAG_MB8ART, "MB8ART debug - should show");
    LOG_INFO(LOG_TAG_MB8ART, "MB8ART info - should show");
    LOG_DEBUG(LOG_TAG_MODBUS_DEVICE, "ModbusDevice debug - should NOT show");
    LOG_WARN(LOG_TAG_MODBUS_DEVICE, "ModbusDevice warning - should show");
    
    Serial.println("\r\n=== All Tests Complete ===\r\n");
}

void loop() {
    static uint32_t lastLog = 0;
    uint32_t now = millis();
    
    // Periodic status
    if (now - lastLog > 10000) {
        lastLog = now;
        LOG_INFO(LOG_TAG_MAIN, "System uptime: %lu seconds", now / 1000);
        
        // Simulate library activity
        LOG_DEBUG(LOG_TAG_MB8ART, "MB8ART periodic debug");
        LOG_INFO(LOG_TAG_RYN4, "RYN4 status OK");
        LOG_ERROR(LOG_TAG_TASK_MANAGER, "TaskManager simulated error");
    }
    
    delay(100);
}