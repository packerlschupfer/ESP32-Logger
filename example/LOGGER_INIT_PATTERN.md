# Logger Initialization Pattern

## Best Practice Initialization

```cpp
#include "Logger.h"
#include "ConsoleBackend.h"  // Or your preferred backend

bool setupLogger() {
    // Get logger singleton instance
    Logger& logger = Logger::getInstance();
    
    // Set backend (optional - ConsoleBackend is default)
    logger.setBackend(std::make_shared<ConsoleBackend>());
    
    // Initialize with buffer size
    logger.init(CONFIG_LOG_BUFFER_SIZE);  // Default: 256 bytes
    
    // Enable logging
    logger.enableLogging(true);
    
    // Set log level
    logger.setLogLevel(ESP_LOG_VERBOSE);  // Or your desired level
    
    // Configure rate limiting
    logger.setMaxLogsPerSecond(0);  // 0 = unlimited (good for startup)
    // Later: logger.setMaxLogsPerSecond(100);  // Normal operation
    
    return true;
}
```

## Usage Pattern

Always access Logger through getInstance():

```cpp
void myFunction() {
    Logger& logger = Logger::getInstance();
    
    logger.log(ESP_LOG_INFO, "MyTag", "Message: %s", someValue);
    logger.flush();  // Force immediate output if needed
}
```

## In FreeRTOS Tasks

```cpp
void myTask(void* param) {
    Logger& logger = Logger::getInstance();
    
    while (true) {
        logger.log(ESP_LOG_INFO, "TaskTag", "Task running");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Key Points

1. **Singleton Pattern**: Always use `Logger::getInstance()`, never create new instances
2. **Thread Safe**: The Logger is thread-safe with internal mutex protection
3. **Stack Safe**: Uses heap allocation to minimize stack usage (important for FreeRTOS tasks)
4. **Rate Limiting**: Disable during startup (0), then set reasonable limit (e.g., 100/sec)
5. **ESP-IDF Integration**: Automatically captures ESP-IDF logs (ESP_LOGI, etc.)

## Build Configuration

In `platformio.ini`, avoid duplicate CORE_DEBUG_LEVEL definitions:

```ini
[env:esp32dev]
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -std=c++14

[env:esp32dev_debug]
extends = env:esp32dev
build_type = debug
build_flags = 
    -DCORE_DEBUG_LEVEL=5  # Don't inherit parent flags
    -std=c++14
    -O0
    -g3
```