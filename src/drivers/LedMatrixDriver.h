#pragma once

#include <cstdint>
#include <cstddef>
#include "pico/stdlib.h"
#include "hardware/pio.h"

class LedMatrixDriver {
public:
    LedMatrixDriver();
    ~LedMatrixDriver();

    bool init();
    void show(const uint8_t* frameBuffer, size_t size);

    // Declared as const to match member function signature
    uint16_t getPhysicalIndex(uint16_t x, uint16_t y) const;

private:
    PIO pio;
    uint sm;
    uint offset;
};