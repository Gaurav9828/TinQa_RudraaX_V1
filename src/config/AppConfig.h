#pragma once

#include <cstdint>
#include <vector>

namespace Config {
    // Master Test Mode Toggle
    inline constexpr bool TEST_MODE = true;

    // Display Matrix Configuration
    inline constexpr uint16_t MATRIX_WIDTH = 32;
    inline constexpr uint16_t MATRIX_HEIGHT = 32;
    inline constexpr uint16_t BRIGHTNESS = 100;
    inline constexpr uint8_t DEFAULT_BRIGHTNESS = 128; // 0-255 scale

    // Target FPS Configuration
    inline constexpr uint32_t TARGET_FPS = 30;
    inline constexpr uint32_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS; // ~33 ms per frame

    // Math & Astronomical Constants
    inline constexpr double SECONDS_IN_DAY = 86400.0;
    inline constexpr float PI = 3.14159265358979323846f;

    // Effect Configurations
    constexpr double HYPERLAPSE_DAY_DURATION_MINUTES = 1.0;
    inline constexpr float DEFAULT_START_HOUR_24 = 6.0f;     // Default to 6:00 AM if system time is unavailable

    // Sunrise Effect Configuration
    inline constexpr float SUNRISE_DURATION_MINUTES = 1.0f; // Duration of touch-triggered sunrise
    inline constexpr float EAST_DIRECTION_DEGREES = 135.0f;

    // Peak LED Indices on the matrix (e.g., 40th, 105th, 300th LED)
    inline const std::vector<size_t> SUNRISE_PEAK_INDICES = { 150, 600, 800 };
    
    // Hardware Feature Availability Flags
    inline constexpr bool IS_BRIGHTNESS_SENSOR_AVAILABLE = true;
    inline constexpr bool IS_EXHAUST_FAN_AVAILABLE = true;

    // Wi-Fi Configuration (2.4 GHz Only)
    inline constexpr char WIFI_SSID[] = "sharav4G";
    inline constexpr char WIFI_PASSWORD[] = "12341234";

    // WebSocket / Server Port
    inline constexpr uint16_t WS_PORT = 21324;

    // GPIO Pin Assignments
    namespace Pins {
        // Touch Sensors (TTP223B)
        inline constexpr uint32_t TOUCH_PAD_1 = 9; // GP9 (Pin 12)
        inline constexpr uint32_t TOUCH_PAD_2 = 12; // GP12 (Pin 16)
        inline constexpr uint32_t TOUCH_PAD_3 = 13; // GP12 (Pin 17)
        inline constexpr uint32_t TOUCH_PAD_4 = 14; // GP14 (Pin 19)
        inline constexpr uint32_t TOUCH_PAD_5 = 15; // GP15 (Pin 20)

        // BH1750 Ambient Light Sensor (I2C0)
        inline constexpr uint32_t I2C_SDA = 4;      // GP4 (Pin 6)
        inline constexpr uint32_t I2C_SCL = 5;      // GP5 (Pin 7)
    }
}