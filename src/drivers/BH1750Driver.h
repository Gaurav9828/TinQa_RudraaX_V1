#pragma once

#include <cstdint>
#include "hardware/i2c.h"

class BH1750Driver {
public:
    BH1750Driver();

    // Initializes i2c0 peripheral and sensor hardware with auto-address scanning
    bool init();

    // Periodic updater to read light levels and calculate auto brightness
    void update();

    // Getter functions
    float getLux() const { return currentLux; }
    uint8_t getCalculatedBrightness() const { return calculatedBrightness; }
    bool isOperational() const { return isInitialized; }

private:
    // Commands for BH1750
    static constexpr uint8_t CMD_POWER_ON = 0x01;
    static constexpr uint8_t CMD_CONTINUOUS_HIGH_RES_MODE = 0x10;

    bool isInitialized;
    uint8_t activeI2cAddress; // Store scanned active I2C address (0x23 or 0x5C)
    uint32_t lastReadTimeMs;
    float currentLux;
    uint8_t calculatedBrightness;

    void calculateBrightness();
};