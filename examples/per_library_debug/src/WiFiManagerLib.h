#ifndef WIFI_MANAGER_LIB_H
#define WIFI_MANAGER_LIB_H

class WiFiManagerLib {
private:
    bool connected;
    int connectionAttempts;
    
public:
    WiFiManagerLib();
    void begin();
    void connect(const char* ssid, const char* password);
    void getStatus();
    void simulateError();
};

#endif