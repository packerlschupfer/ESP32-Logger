#include "ModbusRTULogging.h"
#include "ModbusRTULib.h"
#include <Arduino.h>

ModbusRTULib::ModbusRTULib() : totalRequests(0), timeouts(0) {
    MODBUS_LOG_V("ModbusRTU constructor called");
}

void ModbusRTULib::begin() {
    MODBUS_LOG_I("Initializing Modbus RTU");
    MODBUS_LOG_D("Debug: Configuring UART");
    MODBUS_LOG_V("Verbose: Setting baud rate to 9600");
    MODBUS_LOG_V("Verbose: 8N1 configuration");
    MODBUS_LOG_D("Debug: Setting up timers");
    MODBUS_LOG_I("Modbus RTU initialized");
}

void ModbusRTULib::readRegister(uint8_t slaveId, uint16_t address) {
    unsigned long startTime = millis();
    
    MODBUS_LOG_I("Reading register 0x%04X from slave %d", address, slaveId);
    MODBUS_LOG_D("Debug: Building request packet");
    
    // Simulate packet
    uint8_t request[] = {slaveId, 0x03, (uint8_t)(address >> 8), (uint8_t)(address & 0xFF), 0x00, 0x01, 0xCC, 0xCC};
    MODBUS_LOG_PACKET("TX", request, sizeof(request));
    
    totalRequests++;
    MODBUS_LOG_V("Verbose: Total requests: %lu", totalRequests);
    
    // Simulate response
    delay(10);
    uint8_t response[] = {slaveId, 0x03, 0x02, 0x12, 0x34, 0xCC, 0xCC};
    MODBUS_LOG_PACKET("RX", response, sizeof(response));
    
    unsigned long elapsed = millis() - startTime;
    MODBUS_LOG_TIME("Read operation", elapsed);
    
    MODBUS_LOG_I("Register value: 0x%04X", (response[3] << 8) | response[4]);
}

void ModbusRTULib::getStatistics() {
    MODBUS_LOG_V("Verbose: getStatistics() called");
    MODBUS_LOG_I("Modbus statistics:");
    MODBUS_LOG_I("  Total requests: %lu", totalRequests);
    MODBUS_LOG_I("  Timeouts: %lu", timeouts);
    
    if (totalRequests > 0) {
        float successRate = 100.0f * (totalRequests - timeouts) / totalRequests;
        MODBUS_LOG_D("Debug: Success rate: %.1f%%", successRate);
    }
}

void ModbusRTULib::simulateTimeout() {
    MODBUS_LOG_E("Modbus timeout on slave 1!");
    MODBUS_LOG_W("No response after 1000ms");
    MODBUS_LOG_D("Debug: Timeout count: %lu", ++timeouts);
    
    uint8_t failedRequest[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
    MODBUS_LOG_PACKET("Failed TX", failedRequest, sizeof(failedRequest));
    
    MODBUS_LOG_V("Verbose: Resetting communication state");
}