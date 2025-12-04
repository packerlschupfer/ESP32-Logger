// ConsoleBackend.h
#pragma once
#include "ILogBackend.h"
#include <Arduino.h>

class ConsoleBackend : public ILogBackend {
public:
    void write(const std::string& logMessage) override {
        // Use Serial for ESP32/Arduino compatibility
        Serial.print(logMessage.c_str());
        // Note: newline is already included in the formatted message
    }
    
    void write(const char* logMessage, size_t length) override {
        // Memory-efficient version that avoids string allocation
        // Serial.print can handle null-terminated strings directly
        Serial.print(logMessage);
    }
    
    void flush() override {
        // Flush the serial buffer to ensure immediate output
        Serial.flush();
    }
};