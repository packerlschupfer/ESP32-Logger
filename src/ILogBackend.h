// ILogBackend.h
#pragma once

#include <string>

class ILogBackend {
public:
    virtual ~ILogBackend() {}
    virtual void write(const std::string& logMessage) = 0;
    // Memory-efficient overload that avoids string allocation
    virtual void write(const char* logMessage, size_t length) = 0;
    // Flush any buffered output
    virtual void flush() = 0;
};

