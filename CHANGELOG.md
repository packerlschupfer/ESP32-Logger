# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-12-06

### Added
- Initial public release
- Asynchronous logging with buffer pool (zero blocking in hot path)
- Multiple log levels (ERROR, WARN, INFO, DEBUG, TRACE, VERBOSE)
- Tag-based filtering with per-tag log level control
- Rate limiting to prevent log spam (configurable, default 100 logs/second)
- Thread-safe via FreeRTOS queues and mutexes
- ESP-IDF log redirection support (esp_log_redirect)
- ILogger interface for dependency injection and testing
- ILogBackend interface for custom output backends
- ConsoleBackend for Serial output
- BufferPool for efficient memory management (8 buffers × 256 bytes)
- Low memory footprint (~2KB total)
- Meyer's singleton pattern for thread-safe initialization

### Notes
- Production-tested handling high-throughput logging across 16 concurrent FreeRTOS tasks
- Field-proven in industrial control system with weeks of continuous operation
- Previous internal versions (v1.x-v3.x) not publicly released
- Reset to v0.1.0 for clean public release start
