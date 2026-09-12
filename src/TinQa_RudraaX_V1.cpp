#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <sys/time.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/rtc.h"
#include "config/AppConfig.h"
#include "drivers/TouchDriver.h"
#include "network/WifiWsServer.h"
#include "effects/IEffect.h"
#include "effects/thunder/ThunderEffect.h"
#include "effects/aurora/AuroraEffect.h"
#include "effects/auto/AutoEffect.h"
#include "effects/sunrise/SunriseEffect.h"
#include "effects/sunset/SunsetEffect.h"
#include "effects/test_pattern/TestPatternEffect.h"
#include "drivers/BH1750Driver.h"
#include "drivers/LedMatrixDriver.h"

constexpr size_t MATRIX_BUFFER_BYTES = Config::MATRIX_WIDTH * Config::MATRIX_HEIGHT * 3;
uint8_t renderBuffer[MATRIX_BUFFER_BYTES] = {0};
uint8_t displayBuffer[MATRIX_BUFFER_BYTES] = {0};
volatile bool frameReadyFlag = false;

LedMatrixDriver hardwareMatrix;

bool isCore1Ready = false;
bool isWsServerReady = false;
bool isTouchDriverReady = false;
bool isAmbientSensorReady = false;

void core1_entry()
{
    hardwareMatrix.init();

    while (true)
    {
        uint32_t msg = multicore_fifo_pop_blocking();
        if (msg == 0xDEADBEEF)
        {
            hardwareMatrix.show(displayBuffer, MATRIX_BUFFER_BYTES);
        }
    }
}

void initSystemClockFromBuildTime()
{
    rtc_init();

    const char *build_date = __DATE__;
    const char *build_time = __TIME__;

    char month_str[4] = {0};
    int day = 0, year = 0, hour = 0, min = 0, sec = 0;

    if (sscanf(build_date, "%s %d %d", month_str, &day, &year) == 3 &&
        sscanf(build_time, "%d:%d:%d", &hour, &min, &sec) == 3)
    {
        const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        int month = 1;
        for (int i = 0; i < 12; ++i)
        {
            if (strncmp(month_str, months[i], 3) == 0)
            {
                month = i + 1;
                break;
            }
        }

        datetime_t t = {
            .year = static_cast<int16_t>(year),
            .month = static_cast<int8_t>(month),
            .day = static_cast<int8_t>(day),
            .dotw = 1,
            .hour = static_cast<int8_t>(hour),
            .min = static_cast<int8_t>(min),
            .sec = static_cast<int8_t>(sec)};
        rtc_set_datetime(&t);

        struct tm tm_time = {};
        tm_time.tm_year = year - 1900;
        tm_time.tm_mon = month - 1;
        tm_time.tm_mday = day;
        tm_time.tm_hour = hour;
        tm_time.tm_min = min;
        tm_time.tm_sec = sec;

        time_t epoch_time = mktime(&tm_time);
        struct timeval tv = {.tv_sec = epoch_time, .tv_usec = 0};
        settimeofday(&tv, nullptr);

        printf("[RTC] System Clock synced to Build Time: %04d-%02d-%02d %02d:%02d:%02d\n",
               year, month, day, hour, min, sec);
    }
    else
    {
        printf("[RTC ERROR] Failed to parse build date or time strings.\n");
    }
}

// State Variables
bool isPoweredOn = true;
AppState currentState = Config::INITIAL_APP_STATE;
AppState lastActiveState = Config::INITIAL_APP_STATE;

IEffect *activeEffect = nullptr;
ThunderEffect thunderEffect;
AuroraEffect auroraEffect;
AutoEffect autoEffect;
SunriseEffect sunriseEffect;
SunsetEffect sunsetEffect;
TestPatternEffect testPatternEffect;

void setEffectPointer(AppState state)
{
    switch (state)
    {
    case STATE_TEST:
        activeEffect = &testPatternEffect;
        printf("[EFFECT MANAGER] Switched to PANEL TEST PATTERN\n");
        break;
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
    case STATE_SUNSET:
        activeEffect = &sunsetEffect;
        printf("[EFFECT MANAGER] Switched to SUNSET\n");
        break;
    default:
        activeEffect = nullptr;
        printf("[EFFECT MANAGER] State set to IDLE\n");
        break;
    }

    if (activeEffect != nullptr)
    {
        activeEffect->init();
    }
}

void switchToEffect(AppState newState)
{
    if (!isPoweredOn)
    {
        printf("[POWER] System is OFF. Press Power Button (Pad 5) to turn ON.\n");
        return;
    }

    if (currentState == newState)
    {
        printf("[EFFECT MANAGER] Re-initializing active effect...\n");
        if (activeEffect != nullptr)
        {
            activeEffect->init();
        }
        return;
    }

    currentState = newState;
    lastActiveState = newState;
    setEffectPointer(currentState);
}

void togglePower()
{
    isPoweredOn = !isPoweredOn;

    if (!isPoweredOn)
    {
        if (currentState != STATE_IDLE)
        {
            lastActiveState = currentState;
        }
        currentState = STATE_IDLE;
        activeEffect = nullptr;
        printf("[POWER] System Powered OFF (Matrix Cleared)\n");
    }
    else
    {
        currentState = (lastActiveState != STATE_IDLE) ? lastActiveState : STATE_TEST;
        setEffectPointer(currentState);
        printf("[POWER] System Powered ON (Restored Previous Effect)\n");
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    printf("=========================================\n");
    printf("   TinQa RudraaX V1 Firmware Starting   \n");
    printf("=========================================\n");

    initSystemClockFromBuildTime();

    multicore_launch_core1(core1_entry);
    isCore1Ready = true;
    printf("[SYSTEM] Core 1 LED Push Engine launched successfully.\n");

    WifiWsServer wsServer;
    if (Config::TEST_MODE)
    {
        if (wsServer.init())
        {
            isWsServerReady = true;
            printf("[NETWORK] Wi-Fi / WebSockets initialized successfully.\n");
        }
        else
        {
            printf("[WARNING] Wi-Fi / WS Server failed to initialize. Continuing without network capabilities...\n");
        }
    }

    TouchDriver touchDriver;
    touchDriver.init();
    isTouchDriverReady = true;
    printf("[HARDWARE] Touch Driver initialized successfully.\n");

    BH1750Driver ambientSensor;
    ambientSensor.init();
    if (ambientSensor.isOperational())
    {
        isAmbientSensorReady = true;
        printf("[HARDWARE] BH1750 Ambient Light Sensor initialized successfully.\n");
    }
    else
    {
        printf("[WARNING] BH1750 Light Sensor not responding. Falling back to default brightness...\n");
    }

    setEffectPointer(currentState);

    uint32_t lastTime = to_ms_since_boot(get_absolute_time());

    while (true)
    {
        uint32_t currentTime = to_ms_since_boot(get_absolute_time());
        uint32_t deltaMs = currentTime - lastTime;
        lastTime = currentTime;

        if (Config::TEST_MODE && isWsServerReady)
        {
            wsServer.update();
        }

        if (isTouchDriverReady)
        {
            touchDriver.update();

            if (touchDriver.wasPad1Pressed())
            {
                switchToEffect(STATE_AUTO);
            }
            if (touchDriver.wasPad2SingleClicked())
            {
                switchToEffect(STATE_SUNRISE);
            }
            if (touchDriver.wasPad2LongPressed())
            {
                switchToEffect(STATE_SUNSET);
            }
            if (touchDriver.wasPad3Pressed())
            {
                switchToEffect(STATE_THUNDER);
            }
            if (touchDriver.wasPad4Pressed())
            {
                switchToEffect(STATE_AURORA);
            }
            if (touchDriver.wasPad5Pressed())
            {
                togglePower();
            }
        }

        if (isAmbientSensorReady)
        {
            ambientSensor.update();
        }

        uint8_t rawScale = (isAmbientSensorReady && ambientSensor.isOperational()) 
                            ? ambientSensor.getCalculatedBrightness() 
                            : Config::DEFAULT_BRIGHTNESS;

        uint8_t targetScale = std::min(rawScale, Config::DEFAULT_BRIGHTNESS);
        float factor = targetScale / 255.0f;

        if (isPoweredOn && activeEffect != nullptr)
        {
            activeEffect->update(deltaMs);

            // --- AUTONOMOUS CYCLIC TRANSITION ENGINE ---
            if (currentState == STATE_SUNRISE && sunriseEffect.getProgress() >= 1.0f)
            {
                printf("[AUTONOMOUS CYCLE] Sunrise finished! Transitioning directly into Sunset...\n");
                currentState = STATE_SUNSET;
                lastActiveState = STATE_SUNSET;
                activeEffect = &sunsetEffect;
                activeEffect->init(); // Starts at m_progress = 1.0f (Warm Daylight matching Sunrise end)
            }
            else if (currentState == STATE_SUNSET && sunsetEffect.getProgress() <= 0.0f)
            {
                printf("[AUTONOMOUS CYCLE] Sunset finished! Transitioning directly into Sunrise...\n");
                currentState = STATE_SUNRISE;
                lastActiveState = STATE_SUNRISE;
                activeEffect = &sunriseEffect;
                activeEffect->init(); // Starts at m_progress = 0.0f (Night Black matching Sunset end)
            }

            activeEffect->render(renderBuffer, Config::MATRIX_WIDTH, Config::MATRIX_HEIGHT);

            for (size_t i = 0; i < MATRIX_BUFFER_BYTES; i++)
            {
                renderBuffer[i] = static_cast<uint8_t>(renderBuffer[i] * factor);
            }
        }
        else
        {
            std::memset(renderBuffer, 0, sizeof(renderBuffer));
        }

        if (isCore1Ready)
        {
            std::memcpy(displayBuffer, renderBuffer, MATRIX_BUFFER_BYTES);
            multicore_fifo_push_blocking(0xDEADBEEF);
        }

        if (Config::TEST_MODE && isWsServerReady)
        {
            MatrixTelemetryHeader telemetry;
            telemetry.brightness = static_cast<uint8_t>((targetScale / 255.0f) * 100.0f);
            telemetry.actual_fps = Config::TARGET_FPS;
            telemetry.effect_id = static_cast<uint8_t>(currentState);

            wsServer.broadcastFrame(displayBuffer, MATRIX_BUFFER_BYTES, telemetry);
        }

        sleep_ms(33);
    }

    return 0;
}