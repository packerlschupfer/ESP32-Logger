#include "WiFiManagerLogging.h"
#include "WiFiManagerLib.h"
#include <Arduino.h>

WiFiManagerLib::WiFiManagerLib() : connected(false), connectionAttempts(0) {
    WIFI_LOG_V("WiFiManager constructor called");
}

void WiFiManagerLib::begin() {
    WIFI_LOG_I("Initializing WiFi Manager");
    WIFI_LOG_D("Debug: Setting up WiFi hardware");
    WIFI_LOG_V("Verbose: Configuring registers...");
    WIFI_LOG_V("Verbose: Setting power management...");
    WIFI_LOG_D("Debug: Hardware ready");
    WIFI_LOG_I("WiFi Manager initialized");
}

void WiFiManagerLib::connect(const char* ssid, const char* password) {
    WIFI_LOG_I("Attempting to connect to: %s", ssid);
    WIFI_LOG_D("Debug: Password length: %d", strlen(password));
    
    connectionAttempts++;
    WIFI_LOG_V("Verbose: Connection attempt #%d", connectionAttempts);
    
    // Simulate connection process
    WIFI_LOG_V("Verbose: Scanning for networks...");
    WIFI_LOG_V("Verbose: Found network, RSSI: -67dBm");
    WIFI_LOG_D("Debug: Authenticating...");
    
    connected = true;
    WIFI_LOG_I("Successfully connected to %s", ssid);
    WIFI_LOG_D("Debug: IP assigned: 192.168.1.100");
}

void WiFiManagerLib::getStatus() {
    WIFI_LOG_V("Verbose: getStatus() called");
    
    if (connected) {
        WIFI_LOG_I("Status: Connected");
        WIFI_LOG_D("Debug: Connection attempts: %d", connectionAttempts);
        WIFI_LOG_V("Verbose: Signal strength: -65dBm");
    } else {
        WIFI_LOG_W("Status: Disconnected");
    }
}

void WiFiManagerLib::simulateError() {
    WIFI_LOG_E("Connection lost!");
    WIFI_LOG_W("Attempting reconnection...");
    WIFI_LOG_D("Debug: Error code: 0x1234");
    WIFI_LOG_V("Verbose: Clearing connection state");
    connected = false;
}