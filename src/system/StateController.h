#pragma once
#include <cstdio>
#include <cstdarg>
#include "pico/sync.h"
#include "pico/stdlib.h"

enum class LogLevel { INFO, WARN, ERROR, DEBUG };

class SystemLogger {
private:
    static recursive_mutex_t logMutex;
    static bool initialized;

    SystemLogger() {
        if (!initialized) {
            recursive_mutex_init(&logMutex);
            initialized = true;
        }
    }

public:
    static SystemLogger& getInstance() {
        static SystemLogger instance;
        return instance;
    }

    void log(LogLevel level, const char* tag, const char* format, ...) {
        recursive_mutex_enter_blocking(&logMutex);

        uint32_t ms = to_ms_since_boot(get_absolute_time());
        const char* lvlStr = "INFO";
        if (level == LogLevel::WARN) lvlStr = "WARN";
        if (level == LogLevel::ERROR) lvlStr = "ERR!";
        if (level == LogLevel::DEBUG) lvlStr = "DBG ";

        // Print header: [Timestamp] [Level] [Tag]
        printf("[%08lu ms] [%s] [%-12s] ", ms, lvlStr, tag);

        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);

        printf("\n");
        fflush(stdout); // Force immediate flush to USB serial

        recursive_mutex_exit(&logMutex);
    }

    void printFullLogToSerial() {
        log(LogLevel::INFO, "SYSTEM", "--- FULL LOG BUFFER DUMP COMPLETE ---");
    }
};