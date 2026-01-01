# Thread Safety Test for Logger

This example demonstrates and tests the thread safety of different Logger backends when multiple FreeRTOS tasks log concurrently.

## Purpose

1. **Reproduce** the message corruption issue reported in production
2. **Compare** different backends for thread safety
3. **Verify** that NonBlockingConsoleBackend is inherently thread-safe
4. **Demonstrate** proper configuration for multi-threaded applications

## Test Scenarios

### Phase 1: Normal Concurrent Logging
- 6 worker tasks logging simultaneously
- Tasks distributed across both ESP32 cores
- Identifiable message patterns to detect corruption

### Phase 2: Stress Test
- 2 stress tasks flooding the logger
- Rapid-fire logging without delays
- Tests backend behavior under extreme load

## Backends Tested

1. **ConsoleBackend** - Basic backend, expected to show corruption
2. **SynchronizedConsoleBackend** - Mutex-protected serial writes
3. **NonBlockingConsoleBackend** - Atomic write pattern (default)

## Expected Results

### ConsoleBackend
```
[1234][Worker1][I] MSG_001_START_The_qu[5678][Worker2][I] MSG_001_START
ick_brown_fox_jumps
```
Message corruption due to Serial.write() interleaving.

### SynchronizedConsoleBackend
```
[1234][Worker1][I] MSG_001_START_The_quick_brown_fox_jumps_over_the_lazy_dog_END_MSG_001
[1235][Worker2][I] MSG_001_START_The_quick_brown_fox_jumps_over_the_lazy_dog_END_MSG_001
```
Clean output, no corruption, but might block under load.

### NonBlockingConsoleBackend
```
[1234][Worker1][I] MSG_001_START_The_quick_brown_fox_jumps_over_the_lazy_dog_END_MSG_001
[1235][Worker2][I] MSG_001_START_The_quick_brown_fox_jumps_over_the_lazy_dog_END_MSG_001
```
Clean output, no corruption, no blocking (might drop messages if buffer full).

## Building and Running

```bash
cd example/test_thread_safety
pio run -t upload
pio device monitor
```

## What to Look For

1. **Interleaved Messages**: Parts of different messages mixed together
2. **Corrupted Timestamps**: Broken timestamp formatting
3. **Partial Messages**: Incomplete log lines
4. **Out-of-Order**: Messages appearing in wrong sequence

## Key Findings

1. **Logger is thread-safe** at the formatting level
2. **Serial.write() is NOT atomic** - the real cause of corruption
3. **NonBlockingConsoleBackend** prevents corruption by design
4. **SynchronizedConsoleBackend** adds serial-level mutex protection

## Recommended Configuration

For multi-threaded applications:

```cpp
void setup() {
    // Increase TX buffer
    Serial.setTxBufferSize(1024);
    Serial.begin(921600);
    
    // Logger automatically uses NonBlockingConsoleBackend
    Logger& logger = Logger::getInstance();
    
    // Configure as needed
    logger.setLogLevel(ESP_LOG_INFO);
    logger.setTagLevel("NoisyTask", ESP_LOG_WARN);
}
```

This provides thread safety without blocking!