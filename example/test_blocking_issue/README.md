# Logger Blocking Issue Test

This test suite verifies whether the blocking issues reported in production are caused by our Logger implementation or by the ESP32 Serial hardware itself.

## Problem Statement

Production systems are experiencing:
- 10+ second freezes when libraries flood the logger
- Watchdog timeouts in critical tasks
- Large gaps in log output
- System-wide unresponsiveness

## Test Description

The test suite includes 4 comprehensive tests:

### Test 1: Direct Serial Blocking
- Fills the serial buffer using direct `Serial.print()` calls
- Measures blocking duration when buffer is full
- Establishes baseline ESP32 behavior

### Test 2: Logger Blocking
- Same test using our Logger implementation
- Compares blocking time with direct Serial
- Identifies Logger-specific overhead

### Test 3: Library Flooding Simulation
- Simulates a library (like RYN4) flooding the logger
- Monitors for gaps in log output
- Checks if other tasks can run during flooding

### Test 4: Multi-Task Blocking
- Creates multiple tasks all logging simultaneously
- Measures maximum blocking time per task
- Counts watchdog timeout conditions

## Running the Test

1. Connect your ESP32 device
2. Open a terminal in this directory
3. Run: `pio run -t upload && pio device monitor`
4. Observe the test results

## Expected Output

The test will produce a detailed report showing:
- Blocking durations for Serial vs Logger
- Maximum gaps detected in logging
- Whether tasks were blocked
- Buffer usage statistics
- Final conclusion about blocking source

## Interpreting Results

### If Serial is the Primary Cause:
- Direct Serial blocking time will be significant (>100ms)
- Logger won't add much overhead
- Solution: Implement non-blocking serial writes

### If Logger Adds Overhead:
- Logger blocking time >> Serial blocking time
- Indicates mutex contention or implementation issues
- Solution: Fix Logger implementation + non-blocking serial

### Critical Issues:
- Gaps > 5 seconds confirm the production issue
- Multiple watchdog timeouts indicate severe blocking
- Tasks unable to run = system-wide freeze

## Test Parameters

You can modify these in the code:
- `TEST_BAUD_RATE`: Serial baud rate (default: 115200)
- `FLOOD_MESSAGE_COUNT`: Messages in flood test (default: 1000)
- `MULTI_TASK_COUNT`: Number of concurrent tasks (default: 5)

## Platform Configuration

The `platformio.ini` includes configurations for:
- Standard test (128-byte buffer)
- Large buffer test (2048 bytes) - uncomment to test
- Fast baud rate test (921600) - uncomment to test

## Next Steps

Based on test results, implement:
1. Non-blocking console backend using `Serial.availableForWrite()`
2. Message dropping when buffer is full
3. Per-tag rate limiting
4. Async logging with ring buffer
5. Never use `Serial.flush()` in production code