#ifndef NETWORK_LIBRARY_H
#define NETWORK_LIBRARY_H

#include "LogInterface.h"

class NetworkLibrary {
private:
    static constexpr const char* TAG = "Network";
    bool connected = false;
    int signalStrength = 0;
    const char* ssid = nullptr;
    unsigned long lastPingTime = 0;
    
public:
    bool begin(const char* networkSSID, const char* password) {
        LOG_INFO(TAG, "Starting network initialization...");
        ssid = networkSSID;
        
        LOG_DEBUG(TAG, "SSID: %s", ssid);
        LOG_VERBOSE(TAG, "Password length: %d", strlen(password));
        
        // Simulate WiFi connection process
        LOG_INFO(TAG, "Connecting to WiFi network...");
        
        for (int i = 0; i < 3; i++) {
            LOG_DEBUG(TAG, "Connection attempt %d/3", i + 1);
            delay(1000);
            
            // Simulate connection success on second attempt
            if (i == 1) {
                connected = true;
                signalStrength = -65 + random(20); // -65 to -45 dBm
                LOG_INFO(TAG, "Connected successfully! Signal: %d dBm", signalStrength);
                break;
            }
            
            LOG_WARN(TAG, "Connection attempt %d failed, retrying...", i + 1);
        }
        
        if (!connected) {
            LOG_ERROR(TAG, "Failed to connect after 3 attempts");
            return false;
        }
        
        // Get IP address
        LOG_INFO(TAG, "Obtained IP: 192.168.1.%d", 100 + random(50));
        LOG_DEBUG(TAG, "Gateway: 192.168.1.1");
        LOG_DEBUG(TAG, "DNS: 8.8.8.8");
        
        return true;
    }
    
    bool sendData(const char* data, size_t length) {
        if (!connected) {
            LOG_ERROR(TAG, "Cannot send data - not connected");
            return false;
        }
        
        LOG_VERBOSE(TAG, "Sending %d bytes of data", length);
        LOG_DEBUG(TAG, "Data preview: %.32s%s", data, length > 32 ? "..." : "");
        
        // Simulate network delay
        delay(50 + random(50));
        
        // Simulate occasional packet loss
        if (random(10) == 0) {
            LOG_WARN(TAG, "Packet transmission failed, retransmitting...");
            delay(100);
        }
        
        LOG_INFO(TAG, "Data sent successfully (%d bytes)", length);
        return true;
    }
    
    void updateSignalStrength() {
        if (!connected) return;
        
        int oldStrength = signalStrength;
        signalStrength = -70 + random(30); // -70 to -40 dBm
        
        if (abs(oldStrength - signalStrength) > 10) {
            LOG_WARN(TAG, "Significant signal change: %d -> %d dBm", oldStrength, signalStrength);
        } else {
            LOG_VERBOSE(TAG, "Signal strength: %d dBm", signalStrength);
        }
    }
    
    bool ping() {
        if (!connected) {
            LOG_ERROR(TAG, "Cannot ping - not connected");
            return false;
        }
        
        unsigned long now = millis();
        if (now - lastPingTime < 5000) {
            return true; // Don't ping too often
        }
        
        LOG_DEBUG(TAG, "Pinging gateway...");
        int latency = 5 + random(45); // 5-50ms
        
        if (latency > 30) {
            LOG_WARN(TAG, "High latency detected: %d ms", latency);
        } else {
            LOG_VERBOSE(TAG, "Ping successful: %d ms", latency);
        }
        
        lastPingTime = now;
        return true;
    }
    
    bool isConnected() const { return connected; }
    int getSignalStrength() const { return signalStrength; }
};

#endif // NETWORK_LIBRARY_H