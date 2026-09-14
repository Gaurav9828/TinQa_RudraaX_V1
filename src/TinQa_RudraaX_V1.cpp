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
#include "hardware/i2c.h"

// Project Header Files
#include "config/AppConfig.h"
#include "drivers/TouchDriver.h"
#include "drivers/BH1750Driver.h"
#include "drivers/LedMatrixDriver.h"
// #include "network/WifiWsServer.h" // Commented out: Wi-Fi stack disabled for instant boot optimization
#include "utils/TimePersistence.h"

// Effect Subsystem Headers
#include "effects/IEffect.h"
#include "effects/thunder/ThunderEffect.h"
#include "effects/aurora/AuroraEffect.h"
#include "effects/auto/AutoEffect.h"
#include "effects/sunrise/SunriseEffect.h"
#include "effects/sunset/SunsetEffect.h"
#include "effects/test_pattern/TestPatternEffect.h"
#include "effects/startup/StartupEffect.h"

// ============================================================================
// CONSTANTS & SYSTEM BUFFERS
// ============================================================================
bool isInStartupPhase = true;

constexpr size_t MATRIX_BUFFER_BYTES = Config::MATRIX_WIDTH * Config::MATRIX_HEIGHT * 3;
constexpr uint32_t FIFO_CMD_RENDER = 0xDEADBEEF;

static uint8_t renderBuffer[MATRIX_BUFFER_BYTES] = {0};
static uint8_t displayBuffer[MATRIX_BUFFER_BYTES] = {0};

static LedMatrixDriver hardwareMatrix;
// static WifiWsServer wsServer; // Commented out: Wi-Fi stack instance disabled

// Hardware System Status Indicators
static bool isCore1Ready = false;
// static bool isWsServerReady = false; // Commented out
static bool isTouchDriverReady = false;
static bool isAmbientSensorReady = false;

// App Control State Variables
static bool isPoweredOn = true;
static AppState currentState = Config::INITIAL_APP_STATE;
static AppState lastActiveState = Config::INITIAL_APP_STATE;
static uint32_t totalFrameCount = 0;

// Master Clock State Variables (Persistent across reboots/power cycles via Flash TimePersistence)
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
static StartupEffect startupEffect;

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
    if (TimePersistence::loadState(g_day_of_year, g_simulated_seconds)) {
        LOG_INFO("RTC", "System Clock restored from flash persistence state: Day %u, Seconds %.1f", g_day_of_year, g_simulated_seconds);
    } else {
        LOG_INFO("RTC", "System Clock initialized via build timestamp fallback: Day %u, Seconds %.1f", g_day_of_year, g_simulated_seconds);
    }
    // Pre-sync AutoEffect simulated time immediately upon boot
    autoEffect.setSimulatedTime(g_simulated_seconds);
    autoEffect.setDayOfYear(g_day_of_year);
}

void updateSystemClock(uint32_t delta_ms) {
    double speed_multiplier = 1.0;

    if (currentState == STATE_AUTO && autoEffect.isHyperlapse()) {
        const double target_cycle_mins = static_cast<double>(Config::HYPERLAPSE_DURATION_MINUTES);
        speed_multiplier = Config::SECONDS_IN_DAY / (target_cycle_mins > 0.0 ? (target_cycle_mins * 60.0) : 120.0);
    }

    g_simulated_seconds += (static_cast<double>(delta_ms) / 1000.0) * speed_multiplier;

    if (g_simulated_seconds >= Config::SECONDS_IN_DAY) {
        g_simulated_seconds = std::fmod(g_simulated_seconds, Config::SECONDS_IN_DAY);
        g_day_of_year = static_cast<uint16_t>((g_day_of_year % 365) + 1);
    }

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
        LOG_WARN("POWER_CTRL", "System is POWERED OFF. Ignoring state change to [%s].", getAppStateName(newState));
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
    hardwareMatrix.init();
    LOG_INFO("HW_MATRIX", "LedMatrixDriver hardware initialized on Core 1.");

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

    // 1. Launch Core 1 instantly so LED driver hardware starts up right away at power-on
    multicore_launch_core1(core1_entry);
    isCore1Ready = true;

    // 2. Initialize quick flash time tracking and pre-sync AutoEffect
    initSystemClock();

    // 3. Initialize fast I2C sensors
    TouchDriver touchDriver;
    touchDriver.init();
    isTouchDriverReady = true;

    BH1750Driver ambientSensor;
    ambientSensor.init();
    if (ambientSensor.isOperational()) {
        isAmbientSensorReady = true;
    }

    // 4. Immediately start the startup effect before anything else
    startupEffect.init();
    isInStartupPhase = true;

    LOG_INFO("BOOT", "=========================================");
    LOG_INFO("BOOT", "   TinQa RudraaX V1 Firmware Starting   ");
    LOG_INFO("BOOT", "=========================================");

    uint32_t lastTime = to_ms_since_boot(get_absolute_time());

    while (true) {
        uint32_t currentTime = to_ms_since_boot(get_absolute_time());
        uint32_t deltaMs = currentTime - lastTime;
        lastTime = currentTime;

        if (deltaMs > 500) {
            deltaMs = 16; 
        }

        updateSystemClock(deltaMs);

        // Touch Input Processing
        if (isTouchDriverReady) {
            touchDriver.update();

            if (touchDriver.wasPad1SingleClicked()) {
                if (currentState == STATE_AUTO) {
                    autoEffect.setMode(AutoModeType::REAL_TIME);
                } else {
                    switchToEffect(STATE_AUTO);
                }
            }
            if (touchDriver.wasPad1LongPressed()) {
                if (currentState != STATE_AUTO) {
                    switchToEffect(STATE_AUTO);
                }
                autoEffect.toggleHyperlapse();
            }
            if (touchDriver.wasPad2SingleClicked()) { switchToEffect(STATE_SUNRISE); }
            if (touchDriver.wasPad2LongPressed()) { switchToEffect(STATE_SUNSET); }
            if (touchDriver.wasPad3Pressed()) { switchToEffect(STATE_THUNDER); }
            if (touchDriver.wasPad4Pressed()) { switchToEffect(STATE_AURORA); }
            if (touchDriver.wasPad5Pressed()) { togglePower(); }
        }

        if (isAmbientSensorReady) {
            ambientSensor.update();
        }

        uint8_t rawScale = (isAmbientSensorReady && ambientSensor.isOperational()) 
                            ? ambientSensor.getCalculatedBrightness() 
                            : Config::DEFAULT_BRIGHTNESS;

        uint8_t targetScale = std::min(rawScale, Config::DEFAULT_BRIGHTNESS);
        float factor = targetScale / 255.0f;

        // Frame Rendering Logic
        if (isInStartupPhase) {
            // Render startup animation on screen
            startupEffect.update(deltaMs);
            startupEffect.render(renderBuffer, Config::MATRIX_WIDTH, Config::MATRIX_HEIGHT);

            // Continuously pre-compute AutoEffect layers and sun/moon positions in background
            // using the restored clock time so all states are fully settled when startup finishes.
            autoEffect.updateWithMasterTime(deltaMs, g_simulated_seconds, g_day_of_year);

            if (startupEffect.isComplete()) {
                isInStartupPhase = false;
                LOG_INFO("BOOT", "Startup sequence complete. Switching to Auto mode.");
                setEffectPointer(STATE_AUTO);
            }
        } else if (isPoweredOn && activeEffect != nullptr) {
            if (currentState == STATE_AUTO) {
                autoEffect.updateWithMasterTime(deltaMs, g_simulated_seconds, g_day_of_year);
            } else {
                activeEffect->update(deltaMs);
            }

            if (currentState == STATE_SUNRISE && sunriseEffect.getProgress() >= 1.0f) {
                currentState = STATE_SUNSET;
                lastActiveState = STATE_SUNSET;
                activeEffect = &sunsetEffect;
                activeEffect->init();
            } 
            else if (currentState == STATE_SUNSET && sunsetEffect.getProgress() <= 0.0f) {
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
            multicore_fifo_push_timeout_us(FIFO_CMD_RENDER, 5000);
        }

        totalFrameCount++;
        sleep_ms(Config::FRAME_INTERVAL_MS);
    }

    return 0;
}