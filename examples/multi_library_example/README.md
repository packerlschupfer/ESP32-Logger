# Multi-Library Logger Example

This example demonstrates how to use the custom Logger across multiple libraries efficiently.

## Features Demonstrated

1. **Single Logger Instance**: All libraries share one Logger singleton (~17KB total)
2. **LogInterface Usage**: Libraries use LogInterface.h for zero-overhead logging
3. **Various Log Levels**: Shows ERROR, WARN, INFO, DEBUG, and VERBOSE in action
4. **Real-world Patterns**: Simulates sensor readings, network operations, and data storage

## Project Structure

```
multi_library_example/
├── src/
│   └── main.cpp              # Main application
├── lib/
│   ├── SensorLibrary/        # Simulated sensor with logging
│   │   └── SensorLibrary.h
│   ├── NetworkLibrary/       # Simulated network with logging
│   │   └── NetworkLibrary.h
│   └── StorageLibrary/       # Simulated storage with logging
│       └── StorageLibrary.h
├── platformio.ini            # Build configuration
└── README.md                 # This file
```

## Building and Running

1. Open this directory in PlatformIO
2. Build and upload to your ESP32:
   ```bash
   pio run -t upload
   ```
3. Monitor the output:
   ```bash
   pio run -t monitor
   ```

## Expected Output

You'll see:
- Library initialization logs
- Periodic sensor readings
- Network communication logs
- Storage operations
- Status reports every 30 seconds
- Simulated errors for demonstration

## Memory Usage

Despite using three libraries with extensive logging:
- Only ONE Logger instance is created (~17KB)
- Each library adds 0 bytes for logging infrastructure
- Total memory overhead: ~17KB regardless of library count

## Key Takeaways

1. **USE_CUSTOM_LOGGER** is defined globally in platformio.ini
2. **LogInterfaceImpl.h** is included once in main.cpp
3. Libraries use **LogInterface.h** instead of Logger.h
4. All libraries automatically share the same Logger instance