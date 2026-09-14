#include "LedMatrixDriver.h"
#include "../config/AppConfig.h"
#include "ws2812.pio.h"

LedMatrixDriver::LedMatrixDriver() : pio(pio0), sm(0), offset(0) {}

LedMatrixDriver::~LedMatrixDriver() {}

bool LedMatrixDriver::init() {
    sm = pio_claim_unused_sm(pio, true);
    if (sm < 0) {
        return false;
    }

    offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, Config::Pins::LED_DATA_PIN, 800000, false);

    return true;
}

void LedMatrixDriver::clear() {
    uint8_t zero_buffer[Config::MATRIX_WIDTH * Config::MATRIX_HEIGHT * 3] = {0};
    show(zero_buffer, sizeof(zero_buffer));
}

inline uint16_t LedMatrixDriver::getPhysicalIndex(uint16_t x, uint16_t y) const {
    // 1. Apply Master / Global Matrix Rotation across the full 32x32 layout
    uint16_t gx = x;
    uint16_t gy = y;

    switch (Config::GLOBAL_PANEL_ROTATION) {
        case DEG_90:
            gx = Config::MATRIX_HEIGHT - 1 - y;
            gy = x;
            break;
        case DEG_180:
            gx = Config::MATRIX_WIDTH - 1 - x;
            gy = Config::MATRIX_HEIGHT - 1 - y;
            break;
        case DEG_270:
            gx = y;
            gy = Config::MATRIX_WIDTH - 1 - x;
            break;
        case DEG_0:
        default:
            break;
    }

    // 2. Identify panel coordinates in grid (2x2 grid for 32x32 matrix)
    const uint16_t panelX = gx / Config::PANEL_SIZE;
    const uint16_t panelY = gy / Config::PANEL_SIZE;
    const uint16_t panelIndex = panelY * Config::PANELS_X + panelX;

    const uint16_t localX = gx % Config::PANEL_SIZE;
    const uint16_t localY = gy % Config::PANEL_SIZE;

    // 3. Apply individual per-panel orientation
    const PanelRotation rotation = Config::PANEL_ROTATIONS[panelIndex];
    uint16_t rx = localX;
    uint16_t ry = localY;

    switch (rotation) {
        case DEG_90:
            rx = Config::PANEL_SIZE - 1 - localY;
            ry = localX;
            break;
        case DEG_180:
            rx = Config::PANEL_SIZE - 1 - localX;
            ry = Config::PANEL_SIZE - 1 - localY;
            break;
        case DEG_270:
            rx = localY;
            ry = Config::PANEL_SIZE - 1 - localX;
            break;
        case DEG_0:
        default:
            break;
    }

    // 4. Handle serpentine (zigzag) wiring inside individual 16x16 panels
    if (rx & 1) {
        ry = Config::PANEL_SIZE - 1 - ry;
    }

    const uint16_t localPhysicalIndex = (rx * Config::PANEL_SIZE) + ry;
    const uint16_t panelOffset = panelIndex * Config::LEDS_PER_PANEL;

    return panelOffset + localPhysicalIndex;
}

void LedMatrixDriver::show(const uint8_t* frameBuffer, size_t size) {
    constexpr size_t TOTAL_LEDS = Config::MATRIX_WIDTH * Config::MATRIX_HEIGHT;
    constexpr size_t BUFFER_SIZE = TOTAL_LEDS * 3;

    if (!frameBuffer || size < BUFFER_SIZE) {
        return;
    }

    static uint8_t reorderedBuffer[BUFFER_SIZE];

    for (uint16_t y = 0; y < Config::MATRIX_HEIGHT; ++y) {
        for (uint16_t x = 0; x < Config::MATRIX_WIDTH; ++x) {
            uint16_t srcIdx = (y * Config::MATRIX_WIDTH + x) * 3;
            uint16_t physIdx = getPhysicalIndex(x, y);
            uint16_t dstIdx = physIdx * 3;

            reorderedBuffer[dstIdx + 0] = frameBuffer[srcIdx + 0];
            reorderedBuffer[dstIdx + 1] = frameBuffer[srcIdx + 1];
            reorderedBuffer[dstIdx + 2] = frameBuffer[srcIdx + 2];
        }
    }

    // Fast inline PIO transmission loop
    for (size_t i = 0; i < BUFFER_SIZE; i += 3) {
        uint8_t r = reorderedBuffer[i + 0];
        uint8_t g = reorderedBuffer[i + 1];
        uint8_t b = reorderedBuffer[i + 2];

        uint32_t grb = ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
        pio_sm_put_blocking(pio, sm, grb << 8u);
    }
}