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
        // Use Serial.write to respect the length parameter
        // This correctly handles messages that may not be null-terminated
        // or contain embedded nulls
        if (logMessage && length > 0) {
            Serial.write((const uint8_t*)logMessage, length);
        }
    }
    
    void flush() override {
        // Flush the serial buffer to ensure immediate output
        Serial.flush();
    }
};