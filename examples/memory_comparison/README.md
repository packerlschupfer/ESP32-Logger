# Memory Comparison Example

This example measures and compares the runtime memory usage between ESP-IDF logging and the custom Logger.

## Purpose

Answer the question: "How much memory does the custom Logger use compared to ESP-IDF logging?"

## What It Measures

1. **Baseline memory** - Before any logging
2. **After first log** - Shows Logger singleton creation cost
3. **After initialization** - Shows buffer allocation impact
4. **During stress test** - 100 rapid log messages
5. **After cleanup** - Checks for memory leaks

## Running the Test

### Option 1: Automated Script (Recommended)
```bash
cd examples/memory_comparison
./run_test.sh
```

This will:
- Build and test both configurations
- Capture serial output
- Provide a summary comparison

### Option 2: Manual Testing

Test ESP-IDF logging:
```bash
pio run -e esp32dev_no_logger -t upload
pio device monitor
```

Test Custom Logger:
```bash
pio run -e esp32dev_with_logger -t upload
pio device monitor
```

## Expected Results

Based on the Logger implementation:

- **ESP-IDF Logging**: ~0 bytes heap usage (direct UART output)
- **Custom Logger**: ~17KB for singleton instance
  - Logger object: ~196 bytes
  - Default internal structures
  - Mutex and synchronization objects
  - String formatting buffers

## Interpreting Results

The test output shows:
```
=== 1. Baseline (before any logging) ===
Free heap: 298476 bytes

=== 2. After first log call ===
Free heap: 281012 bytes
Memory used by first log: 17464 bytes
```

The difference between steps 1 and 2 shows the Logger singleton creation cost.

## Key Findings

1. **Logger Singleton**: ~17KB one-time cost
2. **Percentage Impact**: ~5.8% of ESP32 heap
3. **Runtime Overhead**: Minimal during normal operation
4. **No Memory Leaks**: Memory remains stable after initialization

## Memory Breakdown

The ~17KB includes:
- Logger class instance
- Internal buffers
- FreeRTOS mutex
- Backend management
- Rate limiting structures
- Context storage

## Conclusion

The custom Logger adds ~17KB of heap usage compared to ESP-IDF's zero-overhead logging. This is a reasonable trade-off for the features provided:
- Thread-safe operation
- Rate limiting
- Multiple backends
- Structured logging
- Runtime configuration

For memory-constrained applications, use LogInterface.h to make Logger optional and fall back to ESP-IDF when needed.