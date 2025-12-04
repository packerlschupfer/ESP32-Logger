#include <Arduino.h>
#include <SPI.h>
#include "KMPProDinoESP32.h"
#include <KMPCommon.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include "Logger.h"

const char *tag = "Main";

Logger logger;

// Enter a MAC address and IP address for your controller below.
byte MAC_ADDRESS[] = { 0x00, 0x08, 0xDC, 0x72, 0xA5, 0x29 };

unsigned long lastEventTime = 0;  // Time of the last significant event

/*
 * Initialize and set up the logger.
 */
bool setupLogger() {
    logger.init(256); // Initialize Logger with default 256 buffer size
    logger.enableLogging(true);          // Enable logging
    logger.setLogLevel(ESP_LOG_VERBOSE); // Set default log level to include all levels
    logger.setMaxLogsPerSecond(5);       // Set max logs per second
    Serial.println("Logger initialized.");
    return true;
}



void setup() {
    delay(1000);
    Serial.begin(115200);

    if (!setupLogger()) {
        Serial.println("Failed to setup Logger");
    }

    KMPProDinoESP32.begin(ProDino_ESP32_Ethernet);
    KMPProDinoESP32.setStatusLed(blue);

    logger.log(ESP_LOG_INFO, tag, "Initialize Ethernet with DHCP:");
    
    if (Ethernet.begin(MAC_ADDRESS) == 0) {
        logger.log(ESP_LOG_INFO, tag, "Failed to configure Ethernet using DHCP");
        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        logger.log(ESP_LOG_INFO, tag, "Ethernet shield was not found.  Sorry, can't run without hardware. :(");
        } else if (Ethernet.linkStatus() == LinkOFF) {
        logger.log(ESP_LOG_INFO, tag, "Ethernet cable is not connected.");
        }
    } else {
        logger.log(ESP_LOG_INFO, tag, "  DHCP assigned IP ");
        // logger.log(ESP_LOG_INFO, tag, Ethernet.localIP());
        // logger.log(ESP_LOG_INFO, tag, "IP Address: %s", ip.toString().c_str());
    }

    // start the OTEthernet library with internal (flash) based storage
    ArduinoOTA.begin(Ethernet.localIP(), "Arduino", "password", InternalStorage);

    // KMPProDinoESP32.offStatusLed();
}

void loop() {
  // put your main code here, to run repeatedly:
  KMPProDinoESP32.processStatusLed(green, 1000);
	KMPProDinoESP32.setStatusLed(yellow);
  // KMPProDinoESP32.offStatusLed();
 
  // check for updates
  ArduinoOTA.poll();
  //constantly maintain DHCP ip in the long run
  Ethernet.maintain();
  //every second, print link status and toggle LED
  if(millis() - lastEventTime >= 1000) {
    lastEventTime = millis();
    auto link = Ethernet.linkStatus();
    
    const char* statusStr = "";
    switch (link) {
        case Unknown: statusStr = "Unknown"; break;
        case LinkON:  statusStr = "ON"; break;
        case LinkOFF: statusStr = "OFF"; break;
    }
    
    logger.log(ESP_LOG_INFO, tag, "Link status: %s", statusStr);
  }
}