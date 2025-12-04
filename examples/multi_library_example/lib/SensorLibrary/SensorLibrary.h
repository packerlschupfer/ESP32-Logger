#ifndef SENSOR_LIBRARY_H
#define SENSOR_LIBRARY_H

#include "LogInterface.h"

class SensorLibrary {
private:
    static constexpr const char* TAG = "Sensor";
    float temperature = 0.0;
    float humidity = 0.0;
    unsigned long lastReadTime = 0;
    const unsigned long READ_INTERVAL = 2000; // 2 seconds
    
public:
    bool begin() {
        LOG_INFO(TAG, "Initializing sensor library...");
        
        // Simulate sensor initialization
        LOG_DEBUG(TAG, "Checking sensor communication...");
        delay(100);
        
        LOG_DEBUG(TAG, "Configuring sensor parameters...");
        delay(50);
        
        LOG_INFO(TAG, "Sensor initialized successfully");
        return true;
    }
    
    bool update() {
        unsigned long currentTime = millis();
        
        if (currentTime - lastReadTime < READ_INTERVAL) {
            return false; // Not time to read yet
        }
        
        LOG_VERBOSE(TAG, "Reading sensor data...");
        
        // Simulate sensor reading
        temperature = 20.0 + (random(100) / 10.0); // 20.0 - 30.0
        humidity = 40.0 + (random(400) / 10.0);     // 40.0 - 80.0
        
        LOG_DEBUG(TAG, "Raw sensor values - Temp: %.2f, Humidity: %.2f", temperature, humidity);
        
        // Simulate occasional sensor errors
        if (random(20) == 0) {
            LOG_WARN(TAG, "Sensor read retry required");
            delay(10);
            temperature += 0.5; // Adjust after retry
        }
        
        lastReadTime = currentTime;
        LOG_INFO(TAG, "Sensor update complete - Temp: %.1f°C, Humidity: %.1f%%", temperature, humidity);
        
        return true;
    }
    
    float getTemperature() const { 
        LOG_VERBOSE(TAG, "Temperature requested: %.1f", temperature);
        return temperature; 
    }
    
    float getHumidity() const { 
        LOG_VERBOSE(TAG, "Humidity requested: %.1f", humidity);
        return humidity; 
    }
    
    void simulateError() {
        LOG_ERROR(TAG, "Simulated sensor communication error!");
        LOG_ERROR(TAG, "Error details: No ACK received, I2C address: 0x%02X", 0x76);
    }
};

#endif // SENSOR_LIBRARY_H