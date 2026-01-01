# Non-Blocking Logger Backend Demo

This example demonstrates the critical difference between blocking and non-blocking logger backends.

## The Problem

The default ESP32 Serial implementation blocks when the transmit buffer (only 88 bytes!) is full. This can cause:
- System freezes for 10+ seconds
- Watchdog timeouts
- Unresponsive applications
- Missing critical logs

## The Solution

The `NonBlockingConsoleBackend` solves this by:
- Checking `Serial.availableForWrite()` before writing
- Dropping messages rather than blocking
- Never calling `Serial.flush()`
- Tracking dropped messages for diagnostics

## Demo Features

1. **Comparison Test**: Shows blocking vs non-blocking behavior
2. **Stress Test**: Multiple tasks flooding the logger
3. **Statistics**: Real-time reporting of dropped messages
4. **Responsiveness Check**: Verifies system remains responsive

## Running the Demo

```bash
cd example/non_blocking_demo
pio run -t upload && pio device monitor
```

## Expected Output

```
=== Demonstrating Blocking vs Non-Blocking Behavior ===

Test 1: Using BLOCKING ConsoleBackend
Blocking time: 1000+ ms

Test 2: Using NON-BLOCKING NonBlockingConsoleBackend  
Non-blocking time: 1 ms (message may be dropped)

=== Non-Blocking Backend Statistics ===
Dropped messages: 0
Dropped bytes: 0
Buffer available: 88 bytes
=====================================
```

## Key Observations

1. **Blocking backend**: Can freeze for 1+ seconds per message
2. **Non-blocking backend**: Never blocks, drops messages if needed
3. **System responsiveness**: Main loop and tasks continue running
4. **Trade-off**: Some logs dropped vs system freeze

## When to Use

**Always use NonBlockingConsoleBackend for:**
- Production systems
- Real-time applications  
- Systems with watchdog timers
- Multi-threaded applications

**Only use blocking backends for:**
- Simple debugging
- Single-threaded test programs
- When you absolutely need every log message

## Configuration

```cpp
// Recommended for all applications
LoggerConfig config;
config.primaryBackend = LoggerConfig::BackendType::NON_BLOCKING_CONSOLE;
logger.configure(config);

// Or use the preset
LoggerConfig config = LoggerConfig::createProduction();  // Uses non-blocking by default
```

## Platform Optimization

To reduce (but not eliminate) blocking issues:

```ini
build_flags = 
    -D SERIAL_TX_BUFFER_SIZE=2048  ; Increase from 88 bytes
    -D CONFIG_ARDUINO_SERIAL_BUFFER_SIZE=2048
monitor_speed = 921600  ; Faster transmission
```

However, even with these optimizations, blocking backends can still freeze your system. Always use `NonBlockingConsoleBackend` for reliable operation.