// SynchronizedConsoleBackend.cpp
#include "SynchronizedConsoleBackend.h"

// Static member initialization
SemaphoreHandle_t SynchronizedConsoleBackend::serialMutex = nullptr;
bool SynchronizedConsoleBackend::mutexInitialized = false;