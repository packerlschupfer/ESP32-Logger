// MockLogger.h
#pragma once
#include "ILogBackend.h"
#include <vector>
#include <string>

class MockLogger : public ILogBackend {
private:
    std::vector<std::string> logMessages;

public:
    MockLogger() {}
    
    // ILogBackend interface
    void write(const std::string& logMessage) override {
        logMessages.push_back(logMessage);
    }
    
    void write(const char* logMessage, size_t length) override {
        logMessages.push_back(std::string(logMessage, length));
    }
    
    void flush() override {
        // Mock doesn't need to flush anything
    }
    
    // Mock-specific methods for testing
    const std::vector<std::string>& getLogs() const { 
        return logMessages; 
    }
    
    size_t getLogCount() const { 
        return logMessages.size(); 
    }
    
    void clearLogs() { 
        logMessages.clear(); 
    }
};
