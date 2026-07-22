# C++11 Compatible LogInterface Example

This example demonstrates that LogInterface provides zero-overhead logging with C++11, without requiring any C++17 features like `__has_include`.

## Key Points

1. **Zero Overhead**: When `USE_CUSTOM_LOGGER` is not defined, libraries compile to direct ESP-IDF calls
2. **C++11 Compatible**: Uses simple `#ifdef` conditional compilation
3. **No Dependencies**: Libraries don't need to depend on Logger

## Building and Testing

### Test without Custom Logger (Zero Overhead)
```bash
pio run -e without_logger -t upload -t monitor
```

Expected output:
- Mode: ESP-IDF LOGGING
- Memory used: Minimal (just the library instance)
- Logs appear in ESP-IDF format

### Test with Custom Logger
```bash
pio run -e with_logger -t upload -t monitor
```

Expected output:
- Mode: CUSTOM LOGGER
- Memory used: ~17KB (Logger singleton)
- Logs appear in custom Logger format

## How It Works

The library (`MyExampleLibrary.cpp`) includes `LogInterface.h` and uses macros like `LOGI`, `LOGE`, etc.

When compiling:
- **Without** `-DUSE_CUSTOM_LOGGER`: Macros expand to `ESP_LOGI`, `ESP_LOGE`, etc.
- **With** `-DUSE_CUSTOM_LOGGER`: Macros expand to custom logger calls

This is pure compile-time switching using preprocessor directives that have existed since C89!

## Memory Comparison

| Configuration | Memory Usage | Logger Singleton |
|--------------|--------------|------------------|
| without_logger | ~500 bytes | No |
| with_logger | ~17.5KB | Yes |

The ~17KB difference is the Logger singleton that's only created when explicitly requested.