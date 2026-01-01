#ifndef SENSOR_LIB_H
#define SENSOR_LIB_H

class SensorLib {
private:
    float lastTemperature;
    float lastHumidity;
    
public:
    SensorLib();
    void begin();
    float readTemperature();
    float readHumidity();
    void readAllSensors();
    void simulateOutOfRange();
};

#endif