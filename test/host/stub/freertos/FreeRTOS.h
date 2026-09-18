#pragma once
#include <mutex>
#define configMAX_TASK_NAME_LEN 16
typedef std::mutex portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL_SAFE(m) (m)->lock()
#define portEXIT_CRITICAL_SAFE(m) (m)->unlock()
