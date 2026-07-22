# Per-Library Debug Control Example

This example demonstrates how to control logging verbosity on a per-library basis using compile-time flags.

## Key Features

1. **Master Control**: `USE_CUSTOM_LOGGER` enables/disables custom logger globally
2. **Per-Library Debug**: Each library has its own debug flag (`<LIBRARY>_DEBUG`)
3. **Zero Overhead**: Debug logs are completely removed at compile time when disabled
4. **Works with Both Backends**: Pattern works with ESP-IDF and custom Logger

## Build Configurations

### Production (Minimal Logging)
```bash
pio run -e production -t upload -t monitor
```
- Only Error, Warning, and Info messages
- No debug or verbose output
- Minimal code size and best performance

### Debug WiFi Only
```bash
pio run -e debug_wifi -t upload -t monitor
```
- Enables debug/verbose for WiFiManager library only
- Other libraries stay at production log levels

### Debug Modbus Only
```bash
pio run -e debug_modbus -t upload -t monitor
```
- Enables debug/verbose for ModbusRTU library only
- Includes packet dumps and timing information

### Debug All Libraries
```bash
pio run -e debug_all -t upload -t monitor
```
- Enables debug/verbose for all libraries
- Maximum verbosity for troubleshooting

## Implementation Pattern

Each library has its own logging header:

```cpp
// WiFiManagerLogging.h
#ifdef WIFI_MANAGER_DEBUG
    // Debug enabled - show all log levels
    #define WIFI_LOG_D(...) LOG_DEBUG("WiFi", __VA_ARGS__)
#else
    // Debug disabled - compile out
    #define WIFI_LOG_D(...) ((void)0)
#endif
```

## Adding Debug to Your Library

1. Create `YourLibraryLogging.h`:
```cpp
#define YOURLIB_LOG_TAG "YourLib"

#ifdef YOURLIB_DEBUG
    // Enable debug/verbose
#else
    // Disable debug/verbose
#endif
```

2. Use in your library:
```cpp
#include "YourLibraryLogging.h"

void YourLibrary::begin() {
    YOURLIB_LOG_I("Initializing");
    YOURLIB_LOG_D("Debug info...");  // Only if YOURLIB_DEBUG
}
```

3. Enable in platformio.ini:
```ini
build_flags = -DYOURLIB_DEBUG
```

## Advanced Features

### Modbus Example: Feature-Specific Debug
```cpp
#ifdef MODBUS_RTU_DEBUG
    #define MODBUS_LOG_PACKETS  // Enable packet dumps
    #define MODBUS_LOG_TIMING   // Enable timing logs
#endif
```

### Conditional Features
```cpp
#ifdef MODBUS_LOG_PACKETS
    #define MODBUS_LOG_PACKET(data, len) dumpPacket(data, len)
#else
    #define MODBUS_LOG_PACKET(data, len) ((void)0)
#endif
```

## Benefits

1. **Targeted Debugging**: Debug only the library you're working on
2. **Production Ready**: Simply remove debug flags for release
3. **No Runtime Overhead**: Compile-time optimization
4. **Clear Logs**: Avoid log spam from unrelated libraries
5. **Flexible**: Different debug levels for different deployment scenarios