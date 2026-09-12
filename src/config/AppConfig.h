#pragma once

#include <cstdint>
#include <vector>
#include <array>

#define TEST_MODE_ENABLED 1

enum AppState : uint8_t
{
    STATE_IDLE = 0,
    STATE_THUNDER = 1,
    STATE_AURORA = 2,
    STATE_SUNRISE = 3,
    STATE_SUNSET = 4,
    STATE_AUTO = 5,
    STATE_TEST = 6
};

enum PanelRotation : uint16_t {
    DEG_0   = 0,
    DEG_90  = 90,
    DEG_180 = 180,
    DEG_270 = 270   // Equivalent to -90 degrees
};

namespace Config
{
    // Master Test Mode Toggle
    inline constexpr bool TEST_MODE = (TEST_MODE_ENABLED == 1);
    
    // Default boot state set to STATE_TEST for initial panel orientation checking
    inline constexpr AppState INITIAL_APP_STATE = STATE_AURORA;

    // Display Matrix Configuration (32x32 = 4 Panels, 64x64 = 16 Panels, etc.)
    inline constexpr uint16_t MATRIX_WIDTH  = 32;
    inline constexpr uint16_t MATRIX_HEIGHT = 32;

    inline constexpr uint16_t PANEL_SIZE = 16;
    inline constexpr uint16_t LEDS_PER_PANEL = PANEL_SIZE * PANEL_SIZE;
    inline constexpr uint16_t PANELS_X = MATRIX_WIDTH / PANEL_SIZE;
    inline constexpr uint16_t PANELS_Y = MATRIX_HEIGHT / PANEL_SIZE;
    inline constexpr uint16_t TOTAL_PANELS = PANELS_X * PANELS_Y;

    // Adjusted rotation array for uniform panel orientation across 2x2 grid
    inline constexpr std::array<PanelRotation, TOTAL_PANELS> PANEL_ROTATIONS = {
        DEG_0,    // Panel 1 (Top-Left)
        DEG_0,    // Panel 2 (Top-Right)
        DEG_0,    // Panel 3 (Bottom-Left)
        DEG_0     // Panel 4 (Bottom-Right)
    };

    inline constexpr PanelRotation GLOBAL_PANEL_ROTATION = DEG_90; // DEG_0, DEG_90, DEG_180, DEG_270

    inline constexpr uint8_t DEFAULT_BRIGHTNESS = 155; 
    inline constexpr uint8_t BRIGHTNESS = DEFAULT_BRIGHTNESS;    

    inline constexpr uint32_t TARGET_FPS = 10; 
    inline constexpr uint32_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS;

    inline constexpr double SECONDS_IN_DAY = 86400.0;
    inline constexpr float PI = 3.14159265358979323846f;

    constexpr double HYPERLAPSE_DAY_DURATION_MINUTES = 1.0;
    inline constexpr bool HYPERLAPSE = true;

    inline constexpr float DEFAULT_START_HOUR_24 = 5.0f;

    inline constexpr float SUNRISE_DURATION_MINUTES = 5.0f;
    inline constexpr float EAST_DIRECTION_DEGREES = 180.0f;

    inline const std::vector<size_t> SUNRISE_PEAK_INDICES = {1023};
    inline const std::vector<size_t> SUNSET_END_PEAK_INDICES = {992};

    inline constexpr bool IS_BRIGHTNESS_SENSOR_AVAILABLE = true;
    inline constexpr bool IS_EXHAUST_FAN_AVAILABLE = true;

    namespace Touch
    {
        inline constexpr uint32_t DEBOUNCE_MS = 15;
    }

    namespace AmbientSensor
    {
        inline constexpr uint32_t I2C_BAUDRATE_HZ = 100000;
        inline constexpr uint8_t I2C_ADDR_LOW = 0x23;
        inline constexpr uint8_t I2C_ADDR_HIGH = 0x5C;
        inline constexpr uint8_t ACTIVE_I2C_ADDR = I2C_ADDR_LOW;

        inline constexpr uint32_t READ_INTERVAL_MS = 250;
        inline constexpr float FILTER_ALPHA = 0.35f;

        inline constexpr float MIN_LUX = 1.0f;
        inline constexpr float MAX_LUX = 500.0f;

        inline constexpr uint8_t MIN_BRIGHTNESS_FLOOR = 15;
        inline constexpr uint8_t MAX_BRIGHTNESS_CAP   = 153;

        inline constexpr float MIN_BRIGHTNESS_FLOOR_PCT = 6.0f;
        inline constexpr float MAX_BRIGHTNESS_CAP_PCT   = 60.0f;

        inline constexpr bool ENABLE_LOGGING = true;
    }

    inline constexpr char WIFI_SSID[] = "sharav4G";
    inline constexpr char WIFI_PASSWORD[] = "12341234";

    inline constexpr uint16_t WS_PORT = 21324;

    namespace Pins
    {
        inline constexpr uint32_t TOUCH_PAD_1 = 9;  // GP9
        inline constexpr uint32_t TOUCH_PAD_2 = 12; // GP12
        inline constexpr uint32_t TOUCH_PAD_3 = 13; // GP13
        inline constexpr uint32_t TOUCH_PAD_4 = 14; // GP14
        inline constexpr uint32_t TOUCH_PAD_5 = 15; // GP15

        inline constexpr uint32_t I2C_SDA = 4; // GP4
        inline constexpr uint32_t I2C_SCL = 5; // GP5

        inline constexpr uint32_t LED_DATA_PIN = 16; // GP16
    }
}