#ifndef MODBUS_RTU_LIB_H
#define MODBUS_RTU_LIB_H

#include <stdint.h>

class ModbusRTULib {
private:
    uint32_t totalRequests;
    uint32_t timeouts;
    
public:
    ModbusRTULib();
    void begin();
    void readRegister(uint8_t slaveId, uint16_t address);
    void getStatistics();
    void simulateTimeout();
};

#endif