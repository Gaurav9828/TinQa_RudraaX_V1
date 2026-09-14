#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <cstdarg>
#include <sys/time.h>

#include "pico/stdlib.h"
#include "pico/types.h"
#include "pico/util/datetime.h"
#include "pico/multicore.h"
#include "pico/sync.h"

// Project Header Files
#include "config/AppConfig.h"
#include "drivers/TouchDriver.h"
#include "drivers/BH1750Driver.h"
#include "drivers/LedMatrixDriver.h"
#include "network/WifiWsServer.h"
#include "utils/TimePersistence.h"

// Effect Subsystem Headers
#include "effects/IEffect.h"
#include "effects/thunder/ThunderEffect.h"
#include "effects/aurora/AuroraEffect.h"
#include "effects/auto/AutoEffect.h"
#include "effects/sunrise/SunriseEffect.h"
#include "effects/sunset/SunsetEffect.h"
#include "effects/test_pattern/TestPatternEffect.h"

// ============================================================================
// CONSTANTS & SYSTEM BUFFERS
// ============================================================================
constexpr size_t MATRIX_BUFFER_BYTES = Config::MATRIX_WIDTH * Config::MATRIX_HEIGHT * 3;
constexpr uint32_t FIFO_CMD_RENDER = 0xDEADBEEF;

static uint8_t renderBuffer[MATRIX_BUFFER_BYTES] = {0};
static uint8_t displayBuffer[MATRIX_BUFFER_BYTES] = {0};

static LedMatrixDriver hardwareMatrix;

// Hardware System Status Indicators
static bool isCore1Ready = false;
static bool isWsServerReady = false;
static bool isTouchDriverReady = false;
static bool isAmbientSensorReady = false;

// App Control State Variables
static bool isPoweredOn = true;
static AppState currentState = Config::INITIAL_APP_STATE;
static AppState lastActiveState = Config::INITIAL_APP_STATE;
static uint32_t totalFrameCount = 0;

// Master Clock State Variables (Persistent across reboots/power cycles)
static uint16_t g_day_of_year = 150;
static double g_simulated_seconds = 0.0;
static uint32_t g_time_save_timer_ms = 0;

// Effect Subsystem Instances
static IEffect *activeEffect = nullptr;
static ThunderEffect thunderEffect;
static AuroraEffect auroraEffect;
static AutoEffect autoEffect;
static SunriseEffect sunriseEffect;
static SunsetEffect sunsetEffect;
static TestPatternEffect testPatternEffect;

// ============================================================================
// SIMPLE SERIAL MONITOR LOGGING MACROS
// ============================================================================
#define LOG_INFO(tag, fmt, ...) do { \
    uint32_t ms = to_ms_since_boot(get_absolute_time()); \
    printf("[%08lu ms] [INFO] [%-14s] " fmt "\n", ms, tag, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

#define LOG_WARN(tag, fmt, ...) do { \
    uint32_t ms = to_ms_since_boot(get_absolute_time()); \
    printf("[%08lu ms] [WARN] [%-14s] " fmt "\n", ms, tag, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

#define LOG_ERR(tag, fmt, ...) do { \
    uint32_t ms = to_ms_since_boot(get_absolute_time()); \
    printf("[%08lu ms] [ERR!] [%-14s] " fmt "\n", ms, tag, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

#define LOG_DBG(tag, fmt, ...) do { \
    uint32_t ms = to_ms_since_boot(get_absolute_time()); \
    printf("[%08lu ms] [DBG ] [%-14s] " fmt "\n", ms, tag, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

const char* getAppStateName(AppState state) {
    switch (state) {
        case STATE_IDLE:    return "IDLE (POWER OFF)";
        case STATE_THUNDER: return "THUNDER_EFFECT";
        case STATE_AURORA:  return "AURORA_EFFECT";
        case STATE_SUNRISE: return "SUNRISE_EFFECT";
        case STATE_SUNSET:  return "SUNSET_EFFECT";
        case STATE_AUTO:    return "AUTO_EFFECT";
        case STATE_TEST:    return "TEST_PATTERN";
        default:            return "UNKNOWN_STATE";
    }
}

// ============================================================================
// SYSTEM TIME INITIALIZATION & CLOCK ENGINE
// ============================================================================
void initSystemClock() {
    // Load last saved state from flash storage, falling back to compile-time macro if unavailable
    if (TimePersistence::loadState(g_day_of_year, g_simulated_seconds)) {
        LOG_INFO("RTC", "System Clock restored from flash state: Day %u, Seconds %.1f", g_day_of_year, g_simulated_seconds);
    } else {
        LOG_INFO("RTC", "System Clock initialized via build timestamp fallback: Day %u, Seconds %.1f", g_day_of_year, g_simulated_seconds);
    }
}

void updateSystemClock(uint32_t delta_ms) {
    double speed_multiplier = 1.0;

    // Check if AutoEffect is active and running in hyperlapse mode to scale time appropriately
    if (currentState == STATE_AUTO && autoEffect.isHyperlapse()) {
        const double target_cycle_mins = static_cast<double>(Config::HYPERLAPSE_DAY_DURATION_MINUTES);
        speed_multiplier = Config::SECONDS_IN_DAY / (target_cycle_mins > 0.0 ? (target_cycle_mins * 60.0) : 120.0);
    }

    g_simulated_seconds += (static_cast<double>(delta_ms) / 1000.0) * speed_multiplier;

    if (g_simulated_seconds >= Config::SECONDS_IN_DAY) {
        g_simulated_seconds = std::fmod(g_simulated_seconds, Config::SECONDS_IN_DAY);
        g_day_of_year = static_cast<uint16_t>((g_day_of_year % 365) + 1);
    }

    // Periodically checkpoint master clock to flash every 30 seconds
    g_time_save_timer_ms += delta_ms;
    if (g_time_save_timer_ms >= 30000) {
        g_time_save_timer_ms = 0;
        TimePersistence::saveState(g_day_of_year, g_simulated_seconds);
    }
}

// ============================================================================
// STATE CONTROLLER & EFFECT SWITCHING ENGINE
// ============================================================================
void setEffectPointer(AppState state) {
    LOG_INFO("STATE_CTRL", "Switching Active State: [%s]", getAppStateName(state));

    switch (state) {
        case STATE_TEST:
            activeEffect = &testPatternEffect;
            break;
        case STATE_THUNDER:
            activeEffect = &thunderEffect;
            break;
        case STATE_AURORA:
            activeEffect = &auroraEffect;
            break;
        case STATE_AUTO:
            activeEffect = &autoEffect;
            break;
        case STATE_SUNRISE:
            activeEffect = &sunriseEffect;
            break;
        case STATE_SUNSET:
            activeEffect = &sunsetEffect;
            break;
        default:
            activeEffect = nullptr;
            LOG_INFO("STATE_CTRL", "State machine set to IDLE. Dynamic effects disabled.");
            break;
    }

    if (activeEffect != nullptr) {
        activeEffect->init();
        LOG_INFO("STATE_CTRL", "Effect [%s] initialized successfully.", activeEffect->getName());
    }
}

void switchToEffect(AppState newState) {
    if (!isPoweredOn) {
        LOG_WARN("POWER_CTRL", "System is POWERED OFF. Ignoring state change to [%s]. Press Pad 5 to power on.", getAppStateName(newState));
        return;
    }

    if (currentState == newState) {
        LOG_INFO("STATE_CTRL", "Re-triggering current active effect [%s]...", getAppStateName(newState));
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
        
        std::memset(renderBuffer, 0, sizeof(renderBuffer));
        std::memset(displayBuffer, 0, sizeof(displayBuffer));
        
        LOG_INFO("POWER_CTRL", "System Powered OFF. Matrix buffer zeroed out.");
    } else {
        currentState = (lastActiveState != STATE_IDLE) ? lastActiveState : STATE_TEST;
        setEffectPointer(currentState);
        LOG_INFO("POWER_CTRL", "System Powered ON. Restoring state: [%s]", getAppStateName(currentState));
    }
}

// ============================================================================
// CORE 1 DISPLAY DRIVER ENGINE
// ============================================================================
void core1_entry() {
    LOG_INFO("CORE_1", "Core 1 processing thread online.");
    
    hardwareMatrix.init();
    LOG_INFO("HW_MATRIX", "LedMatrixDriver hardware initialized.");

    while (true) {
        uint32_t msg = multicore_fifo_pop_blocking();
        if (msg == FIFO_CMD_RENDER) {
            hardwareMatrix.show(displayBuffer, MATRIX_BUFFER_BYTES);
        }
    }
}

// ============================================================================
// MAIN SYSTEM LOOP (CORE 0)
// ============================================================================
int main() {
    stdio_init_all();
    sleep_ms(2000);

    LOG_INFO("BOOT", "=========================================");
    LOG_INFO("BOOT", "   TinQa RudraaX V1 Firmware Starting   ");
    LOG_INFO("BOOT", "=========================================");

    initSystemClock();

    multicore_launch_core1(core1_entry);
    isCore1Ready = true;
    LOG_INFO("SYSTEM", "Core 1 LED Engine active.");

    WifiWsServer wsServer;
    if (Config::TEST_MODE) {
        if (wsServer.init()) {
            isWsServerReady = true;
            LOG_INFO("NETWORK", "Wi-Fi / WebSockets active.");
        } else {
            LOG_WARN("NETWORK", "Wi-Fi / WS Server failed. Continuing offline.");
        }
    }

    TouchDriver touchDriver;
    touchDriver.init();
    isTouchDriverReady = true;
    LOG_INFO("HARDWARE", "Touch Driver online.");

    BH1750Driver ambientSensor;
    ambientSensor.init();
    if (ambientSensor.isOperational()) {
        isAmbientSensorReady = true;
        LOG_INFO("HARDWARE", "BH1750 Light Sensor online.");
    } else {
        LOG_WARN("HARDWARE", "BH1750 Sensor offline. Using static brightness: %d", Config::DEFAULT_BRIGHTNESS);
    }

    setEffectPointer(currentState);

    uint32_t lastTime = to_ms_since_boot(get_absolute_time());

    while (true) {
        uint32_t currentTime = to_ms_since_boot(get_absolute_time());
        uint32_t deltaMs = currentTime - lastTime;
        lastTime = currentTime;

        // Advance global master clock continuously regardless of active effect
        updateSystemClock(deltaMs);

        // WebSocket Processing
        if (Config::TEST_MODE && isWsServerReady) {
            wsServer.update();
        }

        // Touch Input Diagnostics & State Handling
        if (isTouchDriverReady) {
            touchDriver.update();

            if (touchDriver.wasPad1SingleClicked()) {
                LOG_INFO("TOUCH_EVT", "Pad 1 Single-Tap Detected!");
                if (currentState == STATE_AUTO) {
                    autoEffect.setMode(AutoModeType::REAL_TIME);
                    LOG_INFO("AUTO", "Explicit Real-Time Mode Set");
                } else {
                    switchToEffect(STATE_AUTO);
                }
            }
            if (touchDriver.wasPad1LongPressed()) {
                LOG_INFO("TOUCH_EVT", "Pad 1 Long-Press Detected!");
                if (currentState != STATE_AUTO) {
                    switchToEffect(STATE_AUTO);
                }
                autoEffect.toggleHyperlapse();
                LOG_INFO("AUTO", "Hyperlapse Mode Toggled -> Active: %s", autoEffect.getName());
            }

            if (touchDriver.wasPad2SingleClicked()) {
                LOG_INFO("TOUCH_EVT", "Pad 2 Single-Tap Detected!");
                switchToEffect(STATE_SUNRISE);
            }
            if (touchDriver.wasPad2LongPressed()) {
                LOG_INFO("TOUCH_EVT", "Pad 2 Long-Press Detected!");
                switchToEffect(STATE_SUNSET);
            }

            if (touchDriver.wasPad3Pressed()) {
                LOG_INFO("TOUCH_EVT", "Pad 3 Tap Detected!");
                switchToEffect(STATE_THUNDER);
            }
            if (touchDriver.wasPad4Pressed()) {
                LOG_INFO("TOUCH_EVT", "Pad 4 Tap Detected!");
                switchToEffect(STATE_AURORA);
            }
            if (touchDriver.wasPad5Pressed()) {
                LOG_INFO("TOUCH_EVT", "Pad 5 Power Tap Detected!");
                togglePower();
            }
        }

        // Ambient Brightness Management
        if (isAmbientSensorReady) {
            ambientSensor.update();
        }

        uint8_t rawScale = (isAmbientSensorReady && ambientSensor.isOperational()) 
                            ? ambientSensor.getCalculatedBrightness() 
                            : Config::DEFAULT_BRIGHTNESS;

        uint8_t targetScale = std::min(rawScale, Config::DEFAULT_BRIGHTNESS);
        float factor = targetScale / 255.0f;

        // Frame Rendering Logic
        if (isPoweredOn && activeEffect != nullptr) {
            // Pass global master clock variables into AutoEffect if active
            if (currentState == STATE_AUTO) {
                autoEffect.updateWithMasterTime(deltaMs, g_simulated_seconds, g_day_of_year);
            } else {
                activeEffect->update(deltaMs);
            }

            if (currentState == STATE_SUNRISE && sunriseEffect.getProgress() >= 1.0f) {
                LOG_INFO("AUTO_CYCLE", "Sunrise complete. Transitioning to Sunset...");
                currentState = STATE_SUNSET;
                lastActiveState = STATE_SUNSET;
                activeEffect = &sunsetEffect;
                activeEffect->init();
            } 
            else if (currentState == STATE_SUNSET && sunsetEffect.getProgress() <= 0.0f) {
                LOG_INFO("AUTO_CYCLE", "Sunset complete. Transitioning to Sunrise...");
                currentState = STATE_SUNRISE;
                lastActiveState = STATE_SUNRISE;
                activeEffect = &sunriseEffect;
                activeEffect->init();
            }

            activeEffect->render(renderBuffer, Config::MATRIX_WIDTH, Config::MATRIX_HEIGHT);

            for (size_t i = 0; i < MATRIX_BUFFER_BYTES; i++) {
                renderBuffer[i] = static_cast<uint8_t>(renderBuffer[i] * factor);
            }
        } else {
            std::memset(renderBuffer, 0, MATRIX_BUFFER_BYTES);
        }

        if (isCore1Ready) {
            std::memcpy(displayBuffer, renderBuffer, MATRIX_BUFFER_BYTES);
            
            if (!multicore_fifo_push_timeout_us(FIFO_CMD_RENDER, 5000)) {
                LOG_WARN("CORE_0", "FIFO Full! Skipped frame push.");
            }
        }

        totalFrameCount++;
        if (totalFrameCount % 100 == 0) {
            LOG_INFO("FRAME_STATUS", "Frame: %lu | Active State: %s | Pwr: %s | Lux Scale: %d/255", 
                     totalFrameCount, getAppStateName(currentState), isPoweredOn ? "ON" : "OFF", targetScale);
        }

        if (Config::TEST_MODE && isWsServerReady) {
            MatrixTelemetryHeader telemetry;
            telemetry.brightness = static_cast<uint8_t>((targetScale / 255.0f) * 100.0f);
            telemetry.actual_fps = Config::TARGET_FPS;
            telemetry.effect_id = static_cast<uint8_t>(currentState);

            wsServer.broadcastFrame(displayBuffer, MATRIX_BUFFER_BYTES, telemetry);
        }

        sleep_ms(Config::FRAME_INTERVAL_MS);
    }

    return 0;
}