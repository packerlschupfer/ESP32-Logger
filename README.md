# Logger

A professional-grade, thread-safe logging library for embedded systems with tag-level filtering, memory optimization, and multi-threaded support. Originally designed for ESP32 with FreeRTOS, this library provides enterprise-level logging capabilities optimized for systems with 15-20 threads and limited memory.

> **CRITICAL FIX**: NonBlockingConsoleBackend now prevents 10+ second system freezes! See [How to Use Non-Blocking Logger](HOW_TO_USE_NONBLOCKING_LOGGER.md)
> **New in v3.0**: Professional Logger with tag-level filtering, buffer pool, and multiple backend support!
> **New in v2.0**: Zero-overhead [LogInterface](#zero-overhead-logging-interface) for libraries - no Logger instantiation unless explicitly requested! Works with C++11, no C++17 features needed.

## Memory Efficiency

**Important**: This Logger implementation uses heap allocation instead of Thread-Local Storage (TLS) to avoid the "17KB per task" memory waste problem. See [SINGLETON_MEMORY_WASTE_PROBLEM.md](docs/SINGLETON_MEMORY_WASTE_PROBLEM.md) for details.

## Features

### Professional Logger (v3.0)
- **Tag-Level Filtering**: O(1) per-tag log level control with hash map
- **Memory-Efficient Buffer Pool**: Zero malloc/free in hot path, 8×256 byte pool
- **Multiple Backend Support**: Simultaneous output to multiple destinations
- **Configuration-Driven**: Structured configuration with predefined profiles
- **Optimized for Multi-Threading**: Designed for 15-20 concurrent threads
- **< 4KB Static Memory**: Total memory footprint under 4KB
- **Zero Per-Thread Overhead**: No TLS usage
- **< 128 Bytes Stack Usage**: Minimal stack usage per log call

### Core Features
- **Non-Blocking Console Output**: NonBlockingConsoleBackend prevents system freezes (default)
- **Thread-Safe Logging**: Uses FreeRTOS mutex with C++11 thread-safe singleton pattern
- **Backend System**: NonBlockingConsoleBackend, ConsoleBackend, SynchronizedConsoleBackend, custom implementations
- **Log Subscriber Callbacks**: Forward logs to external systems (Syslog, MQTT, etc.) with async queue
- **Core Affinity Support**: Pin subscriber task to specific core for network-safe callbacks
- **Rate Limiting**: Configurable rate limiting to prevent log flooding (1-1000 logs/sec)
- **Direct Mode**: Bypass rate limiting for critical messages
- **ESP-IDF Integration**: Seamless integration with ESP-IDF logging system
- **Comprehensive Validation**: Null checks, bounds validation, and safe defaults
- **Zero-Overhead Interface**: LogInterface.h for libraries - no singleton instantiation

See [PROFESSIONAL_LOGGER.md](docs/PROFESSIONAL_LOGGER.md) for detailed professional features documentation.

---

## Table of Contents

- [Getting Started](#getting-started)
- [Usage](#usage)
- [Configuration](#configuration)
- [Log Subscriber Callbacks](#log-subscriber-callbacks)
- [Zero-Overhead Logging Interface](#zero-overhead-logging-interface)
- [API Reference](#api-reference)
- [Making Logger Optional in Libraries](#making-logger-optional-in-libraries)
- [Testing](#testing)
- [License](#license)

---

## Getting Started

### Prerequisites

- **ESP32 Development Board**
- **PlatformIO** or **Arduino IDE** for building and flashing
- ESP-IDF framework (included with PlatformIO)

### Installation

Clone this repository or copy the library files into your project.

### Building the Example

```bash
cd example/ESPlan-blueprint-libs-freertos-Logger-workspace
pio run                      # Build for default environment (esp32dev)
pio run -e esp32dev_debug   # Build with debug flags
pio run -t upload           # Upload to board
pio run -t monitor          # Monitor serial output
```

---

## Usage

> **Note for Library Developers**: If you're developing a library that uses Logger, see [Making Logger Optional in Libraries](#making-logger-optional-in-libraries) to make your library more flexible.

### Non-Blocking Console Backend (Default)

**NEW!** The Logger now uses `NonBlockingConsoleBackend` by default, which prevents system freezes when the serial buffer is full. This is critical for systems with multiple verbose libraries.

```cpp
#include "Logger.h"
#include "LogInterface.h"

void setup() {
    Serial.setTxBufferSize(1024);  // Prevent truncation (MUST be before begin)
    Serial.begin(921600);          // Use high baud rate to minimize drops
    
    // Logger automatically uses NonBlockingConsoleBackend
    Logger& logger = Logger::getInstance();
    
    // Configure per-library verbosity to prevent flooding
    logger.setTagLevel("NoisyLibrary", ESP_LOG_WARN);  // Only warnings
    logger.setTagLevel("MyApp", ESP_LOG_DEBUG);        // Keep app verbose
    
    LOG_INFO("MyApp", "System started - will never freeze!");
}
```

See [How to Use Non-Blocking Logger](HOW_TO_USE_NONBLOCKING_LOGGER.md) for detailed configuration and best practices.

### Basic Example

```cpp
#include "Logger.h"

void setup() {
    Serial.begin(115200);
    Logger& logger = getLogger();
    logger.init(256);                   // Initialize the logger with a buffer size
    logger.enableLogging(true);         // Enable logging
    logger.setLogLevel(ESP_LOG_INFO);   // Set log level to INFO

    logger.log(ESP_LOG_INFO, "Main", "Logger initialized successfully.");
}

void loop() {
    static unsigned long lastLogTime = 0;
    if (millis() - lastLogTime > 1000) {
        lastLogTime = millis();
        getLogger().log(ESP_LOG_INFO, "Main", "Logging every second...");
    }
}
```

### Async Logging Example

```cpp
#include "Logger.h"
#include "AsyncLogger.h"
#include "CircularBufferBackend.h"

void setup() {
    // Initialize logger with circular buffer backend
    auto circularBackend = std::make_shared<CircularBufferBackend>(100);
    Logger& logger = Logger(circularBackend);
    logger.init(256);
    
    // Create async logger wrapper
    AsyncLogger asyncLogger(logger, 50, AsyncLogger::OverflowStrategy::DROP_OLDEST);
    asyncLogger.start();
    
    // Use async logger for non-blocking logs
    asyncLogger.log(ESP_LOG_INFO, "Main", "Async logging active!");
}
```

---

## Configuration

### Log Levels

Set the desired log level to control verbosity:
```cpp
logger.setLogLevel(ESP_LOG_WARN);  // Only log WARN, ERROR, and NONE levels
```

Supported levels:
- `ESP_LOG_NONE`
- `ESP_LOG_ERROR`
- `ESP_LOG_WARN`
- `ESP_LOG_INFO`
- `ESP_LOG_DEBUG`
- `ESP_LOG_VERBOSE`

### Rate Limiting

Prevent excessive logging by limiting the number of logs per second:
```cpp
logger.setMaxLogsPerSecond(5);  // Allow up to 5 logs per second
```

### Log Buffer Size

Adjust the log buffer size during initialization:
```cpp
logger.init(512);  // Use a 512-byte buffer for logs
```

### Debug Mode

Enable or disable debug logs during compilation:
- Enable: Uncomment `#define LOGGER_DEBUG` in `Logger.h`.
- Disable: Comment out `#define LOGGER_DEBUG`.

---

## Log Subscriber Callbacks

Forward log messages to external systems like Syslog, MQTT, or custom handlers. Callbacks run on a dedicated FreeRTOS task with configurable core affinity, making them safe for network operations.

### Why Async Subscribers?

When logging from multiple cores, callbacks that perform network operations (UDP, TCP) can crash if called from the wrong core. The async subscriber system solves this by:

1. **Queueing messages** instead of calling callbacks directly
2. **Dedicated task** processes the queue and invokes callbacks
3. **Core pinning** ensures callbacks run on the same core as network stack

### Basic Usage

```cpp
#include "Logger.h"

// Define your callback
void syslogCallback(esp_log_level_t level, const char* tag, const char* message) {
    // Safe to use UDP/TCP here - runs on dedicated task
    syslog->send(level, tag, message);
}

void setup() {
    Logger& logger = Logger::getInstance();

    // Register callback
    logger.addLogSubscriber(syslogCallback);

    // Start subscriber task pinned to Core 1 (where Ethernet runs)
    logger.startSubscriberTask(1);

    // Now all log messages will be forwarded to syslogCallback
    LOG_INFO("App", "This goes to console AND syslog!");
}
```

### Core Affinity

Pin the subscriber task to the same core as your network stack:

```cpp
// Core 1 - typical for Ethernet/LAN8720
logger.startSubscriberTask(1);

// Core 0 - if network runs there
logger.startSubscriberTask(0);

// No affinity - FreeRTOS decides
logger.startSubscriberTask(-1);
```

### Multiple Subscribers

Register up to 4 callbacks (configurable):

```cpp
logger.addLogSubscriber(syslogCallback);
logger.addLogSubscriber(mqttCallback);
logger.addLogSubscriber(sdCardCallback);

// All receive every log message
```

### Cleanup

```cpp
// Remove specific callback
logger.removeLogSubscriber(syslogCallback);

// Stop subscriber task entirely
logger.stopSubscriberTask();
```

### Configuration

Adjust via `platformio.ini` build flags:

```ini
build_flags =
    -DCONFIG_LOG_SUBSCRIBER_QUEUE_SIZE=16    ; Queue depth (default: 16)
    -DCONFIG_LOG_SUBSCRIBER_TASK_STACK=3072  ; Task stack (default: 3072)
    -DCONFIG_LOG_SUBSCRIBER_TASK_PRIORITY=2  ; Task priority (default: 2)
    -DCONFIG_LOG_SUBSCRIBER_MSG_SIZE=200     ; Max message length (default: 200)
```

### Important Notes

- **Async delivery**: Small delay between log call and callback execution
- **Queue overflow**: Messages dropped silently if queue is full (non-blocking)
- **ISR safe**: Log calls from ISR are safe (messages not queued from ISR context)
- **Fallback mode**: If `startSubscriberTask()` not called, callbacks run synchronously (legacy behavior)

---

## Zero-Overhead Logging Interface

**New in v2.0**: LogInterface.h provides a zero-overhead logging solution for library authors.

### The Problem It Solves

When libraries include Logger.h and use `Logger::getInstance()`, it automatically creates the Logger singleton, consuming ~17KB of memory even if the application doesn't use custom logging.

### The Solution

LogInterface.h provides compile-time switching between ESP-IDF and custom Logger without forcing instantiation. This uses simple `#ifdef` conditional compilation that works with C++11 (no C++17 features needed).

### For Library Authors

Replace Logger usage with LogInterface:

```cpp
// OLD - Forces Logger instantiation
#include "Logger.h"
void MyLibrary::doWork() {
    Logger::getInstance().log(ESP_LOG_INFO, "MyLib", "Working...");
}

// NEW - Zero overhead when not using custom Logger
#include "LogInterface.h"
void MyLibrary::doWork() {
    LOG_INFO("MyLib", "Working...");
}
```

### For Application Developers

#### Default: ESP-IDF Logging (Zero Overhead)
```cpp
#include "MyLibrary.h"  // Library uses LogInterface
void setup() {
    MyLibrary lib;
    lib.doWork();  // Logs directly to ESP-IDF, no Logger created
}
```

#### With Custom Logger
```cpp
#define USE_CUSTOM_LOGGER  // Enable before includes
#include "Logger.h"
#include "LogInterface.h"
#include "LogInterfaceImpl.cpp"  // Link implementation
#include "MyLibrary.h"

void setup() {
    Logger::getInstance().init(1024);  // Now explicitly initialize
    MyLibrary lib;
    lib.doWork();  // Logs through custom Logger
}
```

### Memory Savings

- Without `USE_CUSTOM_LOGGER`: ~17KB saved (no Logger singleton)
- With `USE_CUSTOM_LOGGER`: Normal Logger memory usage

### Migration Guide

- [LogInterface Migration Guide](docs/LOGINTERFACE_MIGRATION_GUIDE.md) - Full migration instructions
- [C++11 Library Migration Guide](docs/C11_LIBRARY_MIGRATION.md) - Simplified guide for C++11 compatibility
- [Quick Migration Reference](docs/QUICK_LIBRARY_MIGRATION.md) - 5-minute migration guide
- [Per-Library Log Control](docs/PER_LIBRARY_LOG_CONTROL.md) - Advanced pattern for library-specific debug control

### Multi-Library Usage

Using Logger across multiple libraries? See [Multi-Library Usage Guide](docs/MULTI_LIBRARY_USAGE.md) for best practices and examples.

---

## API Reference

### `Logger`

- **`void init(size_t bufferSize)`**:
  Initialize the logger with a specified buffer size.

- **`void enableLogging(bool enable)`**:
  Enable or disable logging.

- **`void setLogLevel(esp_log_level_t level)`**:
  Set the log level.

- **`void setMaxLogsPerSecond(uint32_t maxLogs)`**:
  Configure the maximum number of logs per second.

- **`void log(esp_log_level_t level, const char* tag, const char* format, ...)`**:
  Log a message with the specified log level and tag.

- **`void logNnl(esp_log_level_t level, const char* tag, const char* format, ...)`**:
  Log a message without appending a newline.

- **`void logInL(const char* format, ...)`**:
  Inline logging (without log level or tag).

- **`uint32_t getFailedMutexAcquisitions()`**:
  Retrieve the count of failed mutex acquisitions.

- **`PerformanceMetrics getMetrics()`**:
  Get current performance metrics including total logs, dropped logs, and timing statistics.

- **`void resetMetrics()`**:
  Reset all performance metrics to zero.

- **`void setDirectMode(bool direct)`**:
  Enable/disable direct mode for minimal stack usage.

- **`bool addLogSubscriber(LogSubscriberCallback callback)`**:
  Register a callback to receive all log messages. Returns true if registered successfully.

- **`bool removeLogSubscriber(LogSubscriberCallback callback)`**:
  Unregister a previously registered callback. Returns true if found and removed.

- **`uint8_t getSubscriberCount()`**:
  Get number of active subscribers.

- **`bool startSubscriberTask(int coreId = -1)`**:
  Start the subscriber notification task. Pass 0 or 1 for core affinity, -1 for no affinity.

- **`void stopSubscriberTask()`**:
  Stop the subscriber notification task and clean up queue.

- **`bool isSubscriberTaskRunning()`**:
  Check if subscriber task is active.

### `LogTimer`

A utility to measure and log elapsed time:
```cpp
LogTimer timer(getLogger(), "TimerTest");
// Do something time-consuming
```

### `CallbackGuard`

RAII helper for automatic callback state management:
```cpp
void myCallback() {
    CallbackGuard guard(logger);  // Automatically enters callback state
    // Your callback code here
    // Callback state automatically cleared when guard goes out of scope
}
```

### `AsyncLogger`

Non-blocking logger wrapper for asynchronous logging:
- **`bool start()`**: Start the worker thread
- **`void stop(bool waitForCompletion)`**: Stop the worker thread
- **`void log(...)`**: Async version of standard log method
- **`QueueStats getStats()`**: Get queue statistics
- **`void setOverflowStrategy(OverflowStrategy)`**: Set queue overflow behavior
- **`bool flush(uint32_t timeoutMs)`**: Process all pending messages

### `CircularBufferBackend`

In-memory backend with automatic log rotation:
- **`std::vector<std::string> getAllLogs()`**: Get all stored logs
- **`std::vector<std::string> getRecentLogs(size_t count)`**: Get recent logs
- **`void clear()`**: Clear all logs
- **`void dumpToSerial(const char* tag)`**: Dump logs to serial output
- **`bool isFull()`**: Check if buffer is at capacity

---

## Making Logger Optional in Libraries

If you're developing a library that uses Logger, consider making it optional to increase your library's flexibility and reusability. This pattern allows your library to work with either the custom Logger or fall back to ESP-IDF logging.

### Why Make Logger Optional?

- **Flexibility**: Users can choose their preferred logging solution
- **No Hard Dependencies**: Libraries work in projects without Logger
- **Graceful Fallback**: Automatically uses ESP-IDF logging when Logger isn't available
- **Better Integration**: Works seamlessly in different project architectures

### Implementation Pattern

This pattern has been successfully implemented in several libraries including ModbusDevice and esp32ModbusRTU. Here's how to implement it:

#### 1. Define Conditional Logging Macros

In your library's main header file (e.g., `YourLibrary.h`), add:

```cpp
// Logging configuration
#ifdef YOURLIBRARY_USE_CUSTOM_LOGGER
    #include "Logger.h"
    #define YOURLIB_LOG_TAG "YourLib"
    #define YOURLIB_LOG_E(...) Logger::getInstance().log(ESP_LOG_ERROR, YOURLIB_LOG_TAG, __VA_ARGS__)
    #define YOURLIB_LOG_W(...) Logger::getInstance().log(ESP_LOG_WARN, YOURLIB_LOG_TAG, __VA_ARGS__)
    #define YOURLIB_LOG_I(...) Logger::getInstance().log(ESP_LOG_INFO, YOURLIB_LOG_TAG, __VA_ARGS__)
    #ifdef YOURLIBRARY_DEBUG
        #define YOURLIB_LOG_D(...) Logger::getInstance().log(ESP_LOG_DEBUG, YOURLIB_LOG_TAG, __VA_ARGS__)
        #define YOURLIB_LOG_V(...) Logger::getInstance().log(ESP_LOG_VERBOSE, YOURLIB_LOG_TAG, __VA_ARGS__)
    #else
        #define YOURLIB_LOG_D(...)
        #define YOURLIB_LOG_V(...)
    #endif
#else
    #include <esp_log.h>
    #define YOURLIB_LOG_TAG "YourLib"
    #define YOURLIB_LOG_E(...) ESP_LOGE(YOURLIB_LOG_TAG, __VA_ARGS__)
    #define YOURLIB_LOG_W(...) ESP_LOGW(YOURLIB_LOG_TAG, __VA_ARGS__)
    #define YOURLIB_LOG_I(...) ESP_LOGI(YOURLIB_LOG_TAG, __VA_ARGS__)
    #ifdef YOURLIBRARY_DEBUG
        #define YOURLIB_LOG_D(...) ESP_LOGD(YOURLIB_LOG_TAG, __VA_ARGS__)
        #define YOURLIB_LOG_V(...) ESP_LOGV(YOURLIB_LOG_TAG, __VA_ARGS__)
    #else
        #define YOURLIB_LOG_D(...)
        #define YOURLIB_LOG_V(...)
    #endif
#endif
```

#### 2. Use Macros in Your Code

Replace direct Logger calls with your macros:

```cpp
// Instead of:
Logger::getInstance().log(ESP_LOG_ERROR, "YourLib", "Error: %s", error);

// Use:
YOURLIB_LOG_E("Error: %s", error);
```

#### 3. Document Usage for Your Users

Add to your library's README:

```markdown
## Logging Configuration

### Default: ESP-IDF Logging
By default, this library uses ESP-IDF logging (ESP_LOGx macros).

### Using Custom Logger
To use the custom Logger library, add to your platformio.ini:
```ini
build_flags = -DYOURLIBRARY_USE_CUSTOM_LOGGER
```

### Enable Debug Logging
For verbose debug output:
```ini
build_flags = 
    -DYOURLIBRARY_USE_CUSTOM_LOGGER  ; Optional
    -DYOURLIBRARY_DEBUG
```
```

### Example Libraries Using This Pattern

- **ModbusDevice**: Uses `MODBUSDEVICE_USE_CUSTOM_LOGGER` flag
- **esp32ModbusRTU**: Uses `MODBUS_USE_CUSTOM_LOGGER` flag

### For More Information

See the [detailed migration guide](docs/OPTIONAL_LOGGER_PATTERN.md) for step-by-step instructions and best practices.

---

## Testing

### Run Tests

Include test cases in your `loop()` function to validate logger functionality:

```cpp
void loop() {
    static bool testsRun = false;
    if (!testsRun) {
        Serial.println("==== Starting Tests ====");
        
        Logger& logger = getLogger();
        logger.log(ESP_LOG_INFO, "Test", "Logger test started.");

        // Add test cases here...

        Serial.println("==== Tests Completed ====");
        testsRun = true;
    }
    delay(10000);  // Prevent re-execution
}
```

---

## Suggested Improvements

1. **Add Structured Logging Format**: Support for JSON or other structured formats
2. **File Backend Implementation**: Add file-based logging backend with rotation
3. **Network Backend**: Support for remote logging (syslog, HTTP, MQTT)
4. **Performance Metrics**: Add performance counters and statistics
5. **Compile-Time Configuration**: More compile-time options for code size optimization
6. **Color Support**: ANSI color codes for console output
7. **Timestamp Formats**: Configurable timestamp formats (ISO8601, Unix, etc.)
8. **Log Filtering Rules**: More sophisticated filtering (regex, wildcards)
9. **Async Logging**: Non-blocking logging with background worker thread
10. **Memory Pool**: Pre-allocated memory pool for zero-allocation logging

---

## Troubleshooting

### Macro Redefinition Warnings

If you see warnings like:
```
warning: "LOG_ERROR" redefined
warning: "LOG_WARN" redefined
warning: "LOG_INFO" redefined
warning: "LOG_DEBUG" redefined
```

This occurs when your application defines its own LOG_* macros that conflict with LogInterface.h. See [docs/APPLICATION_LOG_MACRO_CONFLICTS.md](docs/APPLICATION_LOG_MACRO_CONFLICTS.md) for the solution.

## Known Issues Fixed

### v1.2.1
- **Fixed double logging issue**: Removed redundant `esp_log_write()` calls in the `log()` method that were causing messages to be printed twice when ESP-IDF logging was redirected back to the logger.

## License

This project is licensed under the GPL-3 License. See the [LICENSE](LICENSE) file for details.

---
