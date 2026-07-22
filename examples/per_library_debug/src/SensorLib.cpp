#include "SensorLogging.h"
#include "SensorLib.h"
#include <Arduino.h>

SensorLib::SensorLib() : lastTemperature(0), lastHumidity(0) {
    SENSOR_LOG_V("Sensor constructor called");
}

void SensorLib::begin() {
    SENSOR_LOG_I("Initializing sensor library");
    SENSOR_LOG_D("Debug: Powering up sensors");
    SENSOR_LOG_V("Verbose: Waiting for sensor stabilization (500ms)");
    delay(500);
    SENSOR_LOG_D("Debug: Running self-test");
    SENSOR_LOG_V("Verbose: Self-test passed");
    SENSOR_LOG_I("Sensor library ready");
}

float SensorLib::readTemperature() {
    SENSOR_LOG_V("Verbose: Reading temperature sensor");
    SENSOR_LOG_V("Verbose: Sending I2C command 0x43");
    
    // Simulate reading
    lastTemperature = 22.5f + (random(20) - 10) / 10.0f;
    
    SENSOR_LOG_D("Debug: Raw ADC value: %d", random(2048, 2100));
    SENSOR_LOG_I("Temperature: %.1f°C", lastTemperature);
    
    return lastTemperature;
}

float SensorLib::readHumidity() {
    SENSOR_LOG_V("Verbose: Reading humidity sensor");
    
    // Simulate reading
    lastHumidity = 45.0f + (random(20) - 10) / 10.0f;
    
    SENSOR_LOG_D("Debug: Compensating for temperature");
    SENSOR_LOG_I("Humidity: %.1f%%", lastHumidity);
    
    return lastHumidity;
}

void SensorLib::readAllSensors() {
    SENSOR_LOG_I("Reading all sensors");
    SENSOR_LOG_V("Verbose: Starting batch read");
    
    float temp = readTemperature();
    float hum = readHumidity();
    
    SENSOR_LOG_I("All readings complete: T=%.1f°C, H=%.1f%%", temp, hum);
    SENSOR_LOG_D("Debug: Read cycle took %lu ms", random(50, 100));
}

void SensorLib::simulateOutOfRange() {
    SENSOR_LOG_E("Sensor reading out of range!");
    SENSOR_LOG_W("Temperature reading: 85.0°C (max: 60.0°C)");
    SENSOR_LOG_D("Debug: Possible causes: sensor fault, communication error");
    SENSOR_LOG_V("Verbose: Attempting sensor reset");
}