// main.cpp - Deferred formatting example (LOG_DEFERRED_FORMAT)
//
// LOG_* calls capture their arguments into a ring in RTC memory; the "Logger"
// task formats and prints them. Callers never run printf on their own stack.
//
// Serial commands:
//   p  - trigger a panic right after logging a few lines. After the reboot
//        they are printed with a "[pre-reset]" prefix.
//   r  - esp_restart() without flushing: same replay path
//   s  - print stack high-water marks of the worker and logger tasks
#include <Arduino.h>

#define LOG_TAG "Main"

#include "Logger.h"
#include "LogInterface.h"

static TaskHandle_t workerHandle = nullptr;

// A small-stack task that logs constantly: in deferred mode it only pays for
// the argument capture, not for newlib's formatter
static void workerTask(void*) {
    uint32_t n = 0;
    for (;;) {
        const float temperature = 20.0f + (n % 50) / 10.0f;
        char name[12];
        snprintf(name, sizeof(name), "zone-%lu", static_cast<unsigned long>(n % 4));
        LOG_INFO("Worker", "tick %lu: %s at %.1f C, flags=0x%02X, big=%lld",
                 static_cast<unsigned long>(n), name, temperature,
                 static_cast<unsigned>(n & 0xFF), static_cast<long long>(n) * 1000000000LL);
        n++;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Logger& logger = Logger::getInstance();
    logger.setLogLevel(ESP_LOG_DEBUG);
    logger.setMaxLogsPerSecond(0);

    // Start the logger task first: until it runs, messages wait in the ring.
    // This also prints the reset reason and replays pre-reset entries.
    logger.startLogTask(1);

    LOGI("Deferred formatting: %s", Logger::isDeferred() ? "on" : "off");
    LOG_DEBUG("Main", "width/precision: [%*d] [%-6s] [%.3s] [%08.3f] [%e] [%p] [%%] [%c]",
              5, 42, "ab", "abcdef", 3.14159, 12345.678, static_cast<void*>(&logger), 'x');
    const char* volatile missing = nullptr;
    LOG_DEBUG("Main", "null string: %s", missing);

    xTaskCreate(workerTask, "Worker", 2048, nullptr, 1, &workerHandle);
}

void loop() {
    if (Serial.available()) {
        const int c = Serial.read();
        if (c == 'p') {
            LOG_ERROR("Main", "about to panic on purpose (%d)", 1);
            LOG_ERROR("Main", "these lines should come back as [pre-reset] (%d)", 2);
            volatile int* bad = nullptr;
            *bad = 0;
        } else if (c == 'r') {
            LOG_WARN("Main", "restarting without flush (%d)", 3);
            esp_restart();
        } else if (c == 's') {
            LOG_INFO("Main", "stack free: Worker %u B, Logger task running=%d",
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(workerHandle)),
                     Logger::getInstance().isSubscriberTaskRunning() ? 1 : 0);
        }
    }
    delay(50);
}
