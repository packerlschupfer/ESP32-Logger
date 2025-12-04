// ThreadSafeNonBlockingBackend.cpp
#include "ThreadSafeNonBlockingBackend.h"

// Static member initialization
SemaphoreHandle_t ThreadSafeNonBlockingBackend::writeMutex_ = nullptr;
bool ThreadSafeNonBlockingBackend::mutexInitialized_ = false;
