# Boiler Controller Logger Example

This example demonstrates how to properly configure the Logger for a real-world boiler controller application that uses multiple chatty libraries.

## The Problem This Solves

Your boiler controller was experiencing:
- 10+ second system freezes during initialization
- Watchdog timeouts killing critical tasks  
- Missing logs during important events
- Complete unresponsiveness when libraries flood the logger

## The Solution

1. **NonBlockingConsoleBackend** (now default) - Drops messages instead of freezing
2. **High baud rate** (921600) - 8x more throughput  
3. **Per-library log control** - Silence the noisy libraries
4. **Proper configuration** - Keep your app verbose, quiet the libraries

## Running the Example

```bash
cd example/boiler_controller_example
pio run -t upload && pio device monitor
```

## What This Example Shows

1. **Library Initialization Without Freezing**
   - Simulates RYN4, ModbusRTU, and MQTT initialization
   - These generate 280+ log messages in milliseconds
   - System remains responsive throughout

2. **Selective Log Levels**
   ```cpp
   // Noisy libraries - warnings only
   logger.setTagLevel("RYN4", ESP_LOG_WARN);
   logger.setTagLevel("ModbusRTU", ESP_LOG_WARN);
   
   // Your app stays verbose
   logger.setTagLevel("BoilerCtrl", ESP_LOG_DEBUG);
   logger.setTagLevel("Safety", ESP_LOG_VERBOSE);
   ```

3. **Real-time Control Loop**
   - Runs at 2Hz without interruption
   - Critical safety messages always get through
   - Debug messages dropped if necessary

4. **Statistics Monitoring**
   - Shows dropped message count
   - Monitors logger health
   - Suggests optimizations

## Key Configuration Points

### platformio.ini
```ini
; MUST use high baud rate
monitor_speed = 921600

; Increase serial buffers
-D SERIAL_TX_BUFFER_SIZE=512
```

### Code Setup
```cpp
// Fast serial is critical
Serial.begin(921600);

// Configure BEFORE initializing libraries
logger.setTagLevel("RYN4", ESP_LOG_WARN);

// Then initialize - won't freeze!
initRYN4();
```

## Expected Output

```
=== Boiler Controller Starting ===
Logger configuration example for high-volume logging

Configuring library log levels to prevent flooding:
Library log levels configured

Initializing libraries (this used to freeze for 10+ seconds)...
[INFO] RYN4: RYN4 initialization complete
[INFO] ModbusRTU: Modbus initialization complete  
[INFO] MQTT: MQTT connected successfully
[INFO] BoilerCtrl: All libraries initialized in 142 ms
[WARN] BoilerCtrl: Dropped 127 messages during initialization
[INFO] BoilerCtrl: This is normal - better than freezing!

=== System Status ===
[INFO] Temperature: Boiler: 65.0°C, Return: 45.0°C
[INFO] Pump: STOPPED
[INFO] BoilerCtrl: Free heap: 284756 bytes
[INFO] BoilerCtrl: Uptime: 10 seconds
```

## Lessons Learned

1. **Dropping messages is better than freezing** - Your control loop keeps running
2. **921600 baud prevents most drops** - 8x more bandwidth helps a lot
3. **Library verbosity control is essential** - Don't let libraries spam your logger
4. **Monitor dropped messages** - Know when you need to optimize

## Customization

### For Your Boiler Controller

1. Replace simulated libraries with real ones:
   ```cpp
   // Instead of simulateRYN4Init()
   RYN4.begin();  // Real RYN4 init
   ```

2. Add your actual sensors:
   ```cpp
   float readBoilerTemp() {
       return analogRead(BOILER_TEMP_PIN) * 0.1;
   }
   ```

3. Implement your control logic:
   ```cpp
   if (boilerTemp > setpoint) {
       stopBurner();
       LOG_INFO(TAG_CONTROL, "Burner stopped at %.1f°C", boilerTemp);
   }
   ```

### For Different Applications

This pattern works for any ESP32 application with multiple libraries:
- IoT devices with WiFi/MQTT/Sensors
- Industrial controllers with Modbus/CAN/Ethernet
- Home automation with BLE/Zigbee/WiFi

Just identify your noisy libraries and quiet them down!

## Troubleshooting

**Still seeing freezes?**
- Check you're using NonBlockingConsoleBackend (it's default now)
- Increase baud rate to 921600
- Reduce library verbosity further

**Too many dropped messages?**
- Increase baud rate (921600 or even 1500000)
- Set more libraries to WARN level
- Increase serial buffer size in platformio.ini

**Can't see important messages?**
- Make sure critical tags are set to appropriate levels
- Use LOG_ERROR for critical safety messages
- Consider separate logging for critical events

## Summary

Your boiler controller will never freeze from logging again! The combination of:
- NonBlockingConsoleBackend (default)
- High baud rate (921600)
- Smart log level configuration

Ensures your system stays responsive even when libraries misbehave. Happy logging! 🚀