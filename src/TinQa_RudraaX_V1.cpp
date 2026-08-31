#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include "pico/stdlib.h"
#include "hardware/rtc.h"
#include "config/AppConfig.h"
#include "drivers/TouchDriver.h"
#include "network/WifiWsServer.h"
#include "effects/IEffect.h"
#include "effects/thunder/ThunderEffect.h"
#include "effects/aurora/AuroraEffect.h"
#include "effects/auto/AutoEffect.h"
#include "effects/sunrise/SunriseEffect.h"

// Helper function to set system & RTC time from CMake host build timestamp
void initSystemClockFromBuildTime() {
    rtc_init();

    // __DATE__ format: "Mmm dd yyyy" (e.g. "Aug 31 2026")
    // __TIME__ format: "hh:mm:ss"   (e.g. "14:53:00")
    const char* build_date = __DATE__;
    const char* build_time = __TIME__;

    char month_str[4] = {0};
    int day = 0, year = 0, hour = 0, min = 0, sec = 0;
    
    sscanf(build_date, "%s %d %d", month_str, &day, &year);
    sscanf(build_time, "%d:%d:%d", &hour, &min, &sec);

    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month = 1;
    for (int i = 0; i < 12; ++i) {
        if (strncmp(month_str, months[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }

    // 1. Initialize Hardware RTC
    datetime_t t = {
        .year  = static_cast<int16_t>(year),
        .month = static_cast<int8_t>(month),
        .day   = static_cast<int8_t>(day),
        .dotw  = 1, // Day of week (0-6)
        .hour  = static_cast<int8_t>(hour),
        .min   = static_cast<int8_t>(min),
        .sec   = static_cast<int8_t>(sec)
    };
    rtc_set_datetime(&t);

    // 2. Sync Standard C time (time_t, std::time, localtime)
    struct tm tm_time = {};
    tm_time.tm_year = year - 1900;
    tm_time.tm_mon  = month - 1;
    tm_time.tm_mday = day;
    tm_time.tm_hour = hour;
    tm_time.tm_min  = min;
    tm_time.tm_sec  = sec;

    time_t epoch_time = mktime(&tm_time);
    struct timeval tv = { .tv_sec = epoch_time, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    printf("[RTC] System Clock synced to Build Time: %04d-%02d-%02d %02d:%02d:%02d\n",
           year, month, day, hour, min, sec);
}

enum AppState {
    STATE_IDLE,
    STATE_THUNDER,
    STATE_AURORA,
    STATE_SUNRISE,
    STATE_AUTO
};

// State Variables
bool isPoweredOn = true;
AppState currentState = STATE_AUTO;          // Default active effect when booted
AppState lastActiveState = STATE_AUTO;       // Stores last effect before powering OFF

IEffect* activeEffect = nullptr;
ThunderEffect thunderEffect;
AuroraEffect auroraEffect;
AutoEffect autoEffect;
SunriseEffect sunriseEffect;

void setEffectPointer(AppState state) {
    switch (state) {
        case STATE_THUNDER:
            activeEffect = &thunderEffect;
            printf("[EFFECT MANAGER] Switched to THUNDER\n");
            break;
        case STATE_AURORA:
            activeEffect = &auroraEffect;
            printf("[EFFECT MANAGER] Switched to AURORA\n");
            break;
        case STATE_AUTO:
            activeEffect = &autoEffect;
            printf("[EFFECT MANAGER] Switched to AUTO\n");
            break;
        case STATE_SUNRISE:
            activeEffect = &sunriseEffect;
            printf("[EFFECT MANAGER] Switched to SUNRISE\n");
            break;
        default:
            activeEffect = nullptr;
            printf("[EFFECT MANAGER] State set to IDLE\n");
            break;
    }

    if (activeEffect != nullptr) {
        activeEffect->init();
    }
}

void switchToEffect(AppState newState) {
    if (!isPoweredOn) {
        printf("[POWER] System is OFF. Press Power Button (Pad 5) to turn ON.\n");
        return;
    }

    if (currentState == newState) {
        printf("[EFFECT MANAGER] Re-initializing active effect...\n");
        if (activeEffect != nullptr) {
            activeEffect->init();
        }
        return;
    }

    currentState = newState;
    lastActiveState = newState;
    setEffectPointer(currentState);
}

void togglePower() {
    isPoweredOn = !isPoweredOn;

    if (!isPoweredOn) {
        if (currentState != STATE_IDLE) {
            lastActiveState = currentState;
        }
        currentState = STATE_IDLE;
        activeEffect = nullptr;
        printf("[POWER] System Powered OFF (Matrix Cleared)\n");
    } else {
        currentState = (lastActiveState != STATE_IDLE) ? lastActiveState : STATE_AUTO;
        setEffectPointer(currentState);
        printf("[POWER] System Powered ON (Restored Previous Effect)\n");
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Wait for serial monitor connection

    // Sync system clock to host system time at binary build
    initSystemClockFromBuildTime();

    // Initialize Wi-Fi / Server Stack
    WifiWsServer wsServer;
    if (!wsServer.init()) {
        printf("[ERROR] Failed to initialize Wi-Fi / WS Server!\n");
    }

    // Initialize Touch Inputs
    TouchDriver touchDriver;
    touchDriver.init();

    // Start with default effect initialized
    setEffectPointer(currentState);

    uint8_t frameBuffer[32 * 32 * 3] = {0};
    uint32_t lastTime = to_ms_since_boot(get_absolute_time());

    while (true) {
        uint32_t currentTime = to_ms_since_boot(get_absolute_time());
        uint32_t deltaMs = currentTime - lastTime;
        lastTime = currentTime;

        // 1. Process Network Operations
        wsServer.update();

        // 2. Poll Touch Driver Inputs
        touchDriver.update();

        if (touchDriver.wasPad1Pressed()) { 
            switchToEffect(STATE_AUTO);
        }
        if (touchDriver.wasPad2Pressed()) {
            switchToEffect(STATE_SUNRISE);
        }
        if (touchDriver.wasPad3Pressed()) {
            switchToEffect(STATE_THUNDER);
        }
        if (touchDriver.wasPad4Pressed()) {
            switchToEffect(STATE_AURORA);
        }
        if (touchDriver.wasPad5Pressed()) { 
            togglePower();
        }

        // 3. Render Active Effect or Output Blanking Frame (Off state)
        if (isPoweredOn && activeEffect != nullptr) {
            activeEffect->update(deltaMs);
            activeEffect->render(frameBuffer, 32, 32);
        } else {
            std::memset(frameBuffer, 0, sizeof(frameBuffer));
        }

        // 4. Send Frame over WebSocket
        wsServer.broadcastFrame(frameBuffer, sizeof(frameBuffer));

        sleep_ms(33); // ~30 FPS loop target
    }

    return 0;
}