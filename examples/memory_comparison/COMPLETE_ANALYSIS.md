# Complete Memory and Stack Analysis

## Test Results Summary

Both ESP-IDF and Custom Logger **crashed with stack overflow** during extreme formatting tests.

### Memory Usage (Normal Operation)
- **ESP-IDF**: 264 bytes heap (temporary)
- **Custom Logger**: 620 bytes heap (persistent)
- **Difference**: 356 bytes (0.09% of heap)

### Stack Usage (Heavy Formatting)
Both systems crashed at the same test case with 30 parameters in a single log call.

## Root Cause Analysis

The crash occurs in `vsnprintf` - the standard C library function for string formatting:

### ESP-IDF Call Stack
```
strlen -> _svfprintf_r -> vsnprintf -> log_printfv
```

### Custom Logger Call Stack
```
strlen -> _svfprintf_r -> vsnprintf -> Logger::log
```

The issue is that `vsnprintf` itself uses significant stack space for:
1. Format string parsing
2. Argument processing (va_list)
3. Type conversions
4. Internal buffers

## Key Findings

1. **Logger DOES use heap allocation** for output buffers:
   ```cpp
   char* formatBuffer = (char*)malloc(CONFIG_LOG_BUFFER_SIZE);
   vsnprintf(formatBuffer, CONFIG_LOG_BUFFER_SIZE, format, args);
   ```

2. **But vsnprintf still uses stack** internally before writing to the heap buffer

3. **The crash point** is identical in both systems - during format string processing

## Practical Limits

Based on the test results, both systems can handle:
- Simple formatting with 5-10 parameters ✓
- Moderate complexity with mixed types ✓
- Extreme formatting with 30+ parameters ✗ (stack overflow)

## Recommendations

1. **Avoid extreme formatting** in production code
   - Split complex logs into multiple simpler calls
   - Limit parameters to 10-15 per call

2. **Increase task stack size** if needed:
   ```cpp
   xTaskCreate(task, "name", 4096, NULL, 5, NULL); // 4KB stack
   ```

3. **Use simpler formatting** for critical paths:
   ```cpp
   // Instead of one complex call:
   LOG_INFO(TAG, "30 parameters: %d %u %ld ...", ...);
   
   // Use multiple simpler calls:
   LOG_INFO(TAG, "Part 1: %d %u %ld", a, b, c);
   LOG_INFO(TAG, "Part 2: %f %s %p", d, e, f);
   ```

## Conclusion

The Custom Logger's 356-byte heap overhead provides:
- Thread safety (mutex)
- Rate limiting
- Backend abstraction
- Cleaner formatted output

However, it cannot prevent stack overflow from `vsnprintf` itself when using extreme formatting. This is a limitation of the C standard library, not the Logger implementation.

The heap allocation in Logger does help by:
- Reducing some stack usage (output buffer on heap)
- Preventing corruption (mutex protection)
- But cannot eliminate all stack usage from vsnprintf

For typical logging scenarios, both systems work well. The Custom Logger's small overhead is worth the additional features and safety it provides.