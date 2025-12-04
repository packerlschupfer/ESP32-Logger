# Logger - CLAUDE.md

## Overview
Professional logging system with tag-level filtering, memory optimization, and thread-safe buffer pool. Designed for systems with 15-20 threads and limited stack space.

## Key Features
- Tag-level log filtering
- Memory-efficient buffer pool (8 buffers x 256 bytes)
- Thread-safe logging from multiple tasks
- Rate limiting (100 logs/second default)
- ESP-IDF and custom backend support
- Meyer's singleton pattern

## Architecture

### Components
- `Logger` - Main singleton class
- `BufferPool` - Thread-safe buffer allocation
- `ILogger` - Interface for dependency injection
- `ILogBackend` - Backend abstraction

### Buffer Pool
```cpp
BufferPool::getInstance().acquire();  // Get buffer
BufferPool::getInstance().release(buffer);  // Return buffer
```

## Usage
```cpp
// Get singleton
Logger& log = Logger::getInstance();

// Set global level
log.setLogLevel(ESP_LOG_INFO);

// Set per-tag level
log.setTagLevel("MQTT", ESP_LOG_DEBUG);

// Log message
log.log(ESP_LOG_INFO, "TAG", "Message: %d", value);
```

## Log Levels
| Level | Use Case |
|-------|----------|
| ERROR | Failures preventing operation |
| WARN | Recoverable issues |
| INFO | Significant state changes |
| DEBUG | Detailed debugging flow |
| VERBOSE | High-frequency data |

## Integration
Libraries use `LogInterface.h` macros:
```cpp
#include <LogInterface.h>
LOG_WRITE(ESP_LOG_INFO, "TAG", "Message");
```

## Thread Safety
- Buffer pool uses mutex for allocation
- Rate limiter uses atomic counters
- Safe to call from ISR with restrictions

## Log Subscriber Callbacks
Forward logs to external systems (Syslog, MQTT, etc.) via async queue with core affinity.

### Usage
```cpp
// Define callback
void syslogCallback(esp_log_level_t level, const char* tag, const char* message) {
    syslog->send(level, tag, message);
}

// Register and start on Core 1 (same as Ethernet)
logger.addLogSubscriber(syslogCallback);
logger.startSubscriberTask(1);  // Pin to Core 1
```

### Key Points
- Callbacks run on dedicated FreeRTOS task (not caller's context)
- Core affinity prevents cross-core crashes with network operations
- Queue-based: non-blocking for callers, async delivery
- ISR safe: log calls from ISR don't queue (silently skipped)
- Max 4 subscribers (configurable)

### API
- `addLogSubscriber(callback)` - Register callback
- `removeLogSubscriber(callback)` - Unregister callback
- `startSubscriberTask(coreId)` - Start task (0, 1, or -1 for no affinity)
- `stopSubscriberTask()` - Stop task and cleanup

## Build Configuration
```ini
build_flags =
    -DCONFIG_LOG_BUFFER_SIZE=256
    -DCONFIG_LOG_MAX_TAGS=32
    -DCONFIG_LOG_BUFFER_POOL_SIZE=8
    -DCONFIG_LOG_SUBSCRIBER_QUEUE_SIZE=16
    -DCONFIG_LOG_SUBSCRIBER_TASK_STACK=3072
    -DCONFIG_LOG_SUBSCRIBER_MSG_SIZE=200
```
