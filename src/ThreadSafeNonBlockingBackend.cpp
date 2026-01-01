// ThreadSafeNonBlockingBackend.cpp
#include "ThreadSafeNonBlockingBackend.h"

// Static member initialization
std::atomic<SemaphoreHandle_t> ThreadSafeNonBlockingBackend::writeMutex_{nullptr};
std::atomic<bool> ThreadSafeNonBlockingBackend::mutexInitialized_{false};
