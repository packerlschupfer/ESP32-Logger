#pragma once
#define taskSCHEDULER_NOT_STARTED 1
#define taskSCHEDULER_RUNNING 2
inline int xTaskGetSchedulerState() { return taskSCHEDULER_RUNNING; }
extern thread_local const char* g_taskName;
inline const char* pcTaskGetName(void*) { return g_taskName; }
