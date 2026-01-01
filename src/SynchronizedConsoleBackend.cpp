// SynchronizedConsoleBackend.cpp
#include "SynchronizedConsoleBackend.h"

// Static member initialization
std::atomic<SemaphoreHandle_t> SynchronizedConsoleBackend::serialMutex{nullptr};
std::atomic<bool> SynchronizedConsoleBackend::mutexInitialized{false};