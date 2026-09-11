#include "drivers/BH1750Driver.h"
#include "config/AppConfig.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

BH1750Driver::BH1750Driver()
    : isInitialized(false),
      activeI2cAddress(Config::AmbientSensor::I2C_ADDR_LOW),
      lastReadTimeMs(0),
      currentLux(0.0f),
      calculatedBrightness(Config::AmbientSensor::MAX_BRIGHTNESS_CAP) {}

bool BH1750Driver::init() {
    printf("[BH1750] Executing I2C Bus Reset & Initialization...\n");
    stdio_flush();

    gpio_init(Config::Pins::I2C_SDA);
    gpio_init(Config::Pins::I2C_SCL);
    gpio_set_dir(Config::Pins::I2C_SDA, GPIO_IN);
    gpio_set_dir(Config::Pins::I2C_SCL, GPIO_OUT);

    for (int i = 0; i < 9; ++i) {
        gpio_put(Config::Pins::I2C_SCL, 0);
        sleep_us(10);
        gpio_put(Config::Pins::I2C_SCL, 1);
        sleep_us(10);
    }

    i2c_init(i2c0, Config::AmbientSensor::I2C_BAUDRATE_HZ);
    gpio_set_function(Config::Pins::I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(Config::Pins::I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(Config::Pins::I2C_SDA);
    gpio_pull_up(Config::Pins::I2C_SCL);

    sleep_ms(150);

    uint8_t targetAddr = 0;
    uint8_t pwrCmd = CMD_POWER_ON;

    if (i2c_write_blocking(i2c0, Config::AmbientSensor::I2C_ADDR_LOW, &pwrCmd, 1, false) >= 0) {
        targetAddr = Config::AmbientSensor::I2C_ADDR_LOW;
    } else if (i2c_write_blocking(i2c0, Config::AmbientSensor::I2C_ADDR_HIGH, &pwrCmd, 1, false) >= 0) {
        targetAddr = Config::AmbientSensor::I2C_ADDR_HIGH;
    }

    if (targetAddr == 0) {
        printf("[BH1750 ERROR] No device found on GP%d/GP%d!\n", Config::Pins::I2C_SDA, Config::Pins::I2C_SCL);
        stdio_flush();
        isInitialized = false;
        return false;
    }

    uint8_t modeCmd = CMD_CONTINUOUS_HIGH_RES_MODE;
    if (i2c_write_blocking(i2c0, targetAddr, &modeCmd, 1, false) < 0) {
        printf("[BH1750 ERROR] Mode set failed on 0x%02X\n", targetAddr);
        stdio_flush();
        isInitialized = false;
        return false;
    }

    sleep_ms(180);

    activeI2cAddress = targetAddr;
    isInitialized = true;
    printf("[BH1750 SUCCESS] Connected to BH1750FVI at 0x%02X!\n", activeI2cAddress);
    stdio_flush();
    return true;
}

void BH1750Driver::update() {
    if (!isInitialized) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - lastReadTimeMs < Config::AmbientSensor::READ_INTERVAL_MS) {
        return;
    }
    lastReadTimeMs = now;

    uint8_t rxData[2] = {0, 0};
    int readLen = i2c_read_blocking(i2c0, activeI2cAddress, rxData, 2, false);

    if (readLen == 2) {
        uint16_t rawLux = (static_cast<uint16_t>(rxData[0]) << 8) | rxData[1];
        float rawLuxFloat = static_cast<float>(rawLux) / 1.2f;

        if (currentLux == 0.0f) {
            currentLux = rawLuxFloat;
        }

        currentLux = (Config::AmbientSensor::FILTER_ALPHA * rawLuxFloat) + 
                     ((1.0f - Config::AmbientSensor::FILTER_ALPHA) * currentLux);

        calculateBrightness();

        if (Config::AmbientSensor::ENABLE_LOGGING) {
            static uint32_t lastLogTime = 0;
            if (now - lastLogTime >= 2000) {
                printf("[BH1750] Raw: %.1f lx | Filtered: %.1f lx | Brightness: %u/255 (Max 60%%)\n",
                    rawLuxFloat, currentLux, calculatedBrightness);
                lastLogTime = now;
            }
        }
    } else {
        printf("[BH1750 ERROR] Read failed on 0x%02X!\n", activeI2cAddress);
        stdio_flush();
    }
}

void BH1750Driver::calculateBrightness() {
    float clampedLux = std::clamp(currentLux, Config::AmbientSensor::MIN_LUX, Config::AmbientSensor::MAX_LUX);
    float logLux = std::log10(clampedLux);
    float logMin = std::log10(Config::AmbientSensor::MIN_LUX);
    float logMax = std::log10(Config::AmbientSensor::MAX_LUX);

    float norm = (logLux - logMin) / (logMax - logMin);
    float targetBrightness = Config::AmbientSensor::MIN_BRIGHTNESS_FLOOR + 
                            (norm * (Config::AmbientSensor::MAX_BRIGHTNESS_CAP - Config::AmbientSensor::MIN_BRIGHTNESS_FLOOR));

    calculatedBrightness = static_cast<uint8_t>(
        std::clamp(targetBrightness, (float)Config::AmbientSensor::MIN_BRIGHTNESS_FLOOR, (float)Config::AmbientSensor::MAX_BRIGHTNESS_CAP)
    );
}