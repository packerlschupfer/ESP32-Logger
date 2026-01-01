---

# ESP32 Logger

A lightweight, thread-safe, and customizable logging utility for ESP32 microcontrollers. The logger supports multiple log levels, rate limiting, mutex-based thread safety, and integration with the ESP-IDF logging system.

## Features

- **Thread-Safe Logging**: Uses FreeRTOS mutex for concurrency control.
- **Rate Limiting**: Prevents excessive logging to conserve resources.
- **Dynamic Log Levels**: Supports ESP-IDF log levels (e.g., `ERROR`, `WARN`, `INFO`, `DEBUG`, `VERBOSE`).
- **Custom Buffer Size**: Configurable log buffer size to suit application needs.
- **Truncation Handling**: Long log messages are truncated with an ellipsis.
- **Integration with ESP-IDF**: Redirects ESP-IDF logs via `vprintf` to this logger.
- **Debug Mode**: Optional compile-time debug logging using `#define LOGGER_DEBUG`.

---

## Table of Contents

- [Getting Started](#getting-started)
- [Usage](#usage)
- [Configuration](#configuration)
- [API Reference](#api-reference)
- [Testing](#testing)
- [License](#license)

---

## Getting Started

### Prerequisites

- **ESP32 Development Board**
- **PlatformIO** or **Arduino IDE** for building and flashing
- ESP-IDF framework (included with PlatformIO)

### Installation

Clone this repository or copy the `Logger.h` and `Logger.cpp` files into your project.

---

## Usage

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

### `LogTimer`

A utility to measure and log elapsed time:
```cpp
LogTimer timer(getLogger(), "TimerTest");
// Do something time-consuming
```

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

## License

This project is licensed under the GPL-3 License. See the [LICENSE](LICENSE) file for details.

---
