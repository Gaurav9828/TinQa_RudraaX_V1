#include "effects/test_pattern/TestPatternEffect.h"
#include "config/AppConfig.h"
#include <cstring>

static const uint8_t DIGIT_FONT[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
    {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
    {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
    {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
    {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
    {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
    {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
    {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
    {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

void TestPatternEffect::init() {
    animationTick = 0;
}

void TestPatternEffect::update(uint32_t delta_ms) {
    animationTick += delta_ms;
}

void TestPatternEffect::drawDigit(uint8_t* buffer, size_t width, size_t height, int startX, int startY, int number, uint8_t r, uint8_t g, uint8_t b) {
    if (!buffer || number < 0 || number > 9) return;

    for (int row = 0; row < 5; ++row) {
        const uint8_t rowBits = DIGIT_FONT[number][row];

        for (int col = 0; col < 3; ++col) {
            if (!(rowBits & (1 << (2 - col)))) continue;

            const int px = startX + col;
            const int py = startY + row;

            if (px < 0 || px >= static_cast<int>(width) || py < 0 || py >= static_cast<int>(height)) continue;

            const size_t idx = (py * width + px) * 3;
            buffer[idx] = r;
            buffer[idx + 1] = g;
            buffer[idx + 2] = b;
        }
    }
}

void TestPatternEffect::render(uint8_t* buffer, size_t width, size_t height) {
    if (!buffer || width == 0 || height == 0) return;

    std::memset(buffer, 0, width * height * 3);

    const size_t panelsX = width / Config::PANEL_SIZE;
    const size_t panelsY = height / Config::PANEL_SIZE;
    const uint8_t borderR = 40, borderG = 40, borderB = 180;
    const uint8_t numR = 255, numG = 220, numB = 0;
    const bool pulse = (animationTick / 500) % 2 == 0;

    for (size_t panelY = 0; panelY < panelsY; ++panelY) {
        for (size_t panelX = 0; panelX < panelsX; ++panelX) {
            const size_t panelIndex = panelY * panelsX + panelX + 1;
            const size_t offsetX = panelX * Config::PANEL_SIZE;
            const size_t offsetY = panelY * Config::PANEL_SIZE;
            const PanelRotation rotation = Config::PANEL_ROTATIONS[panelIndex - 1];

            for (size_t i = 0; i < Config::PANEL_SIZE; ++i) {
                const size_t topIdx = (offsetY * width + offsetX + i) * 3;
                buffer[topIdx] = borderR;
                buffer[topIdx + 1] = borderG;
                buffer[topIdx + 2] = borderB;

                const size_t bottomIdx = ((offsetY + Config::PANEL_SIZE - 1) * width + offsetX + i) * 3;
                buffer[bottomIdx] = borderR;
                buffer[bottomIdx + 1] = borderG;
                buffer[bottomIdx + 2] = borderB;

                const size_t leftIdx = ((offsetY + i) * width + offsetX) * 3;
                buffer[leftIdx] = borderR;
                buffer[leftIdx + 1] = borderG;
                buffer[leftIdx + 2] = borderB;

                const size_t rightIdx = ((offsetY + i) * width + offsetX + Config::PANEL_SIZE - 1) * 3;
                buffer[rightIdx] = borderR;
                buffer[rightIdx + 1] = borderG;
                buffer[rightIdx + 2] = borderB;
            }

            const size_t dotIdx = ((offsetY + 2) * width + offsetX + 2) * 3;
            buffer[dotIdx] = pulse ? 0 : 255;
            buffer[dotIdx + 1] = pulse ? 255 : 0;
            buffer[dotIdx + 2] = 0;

            int digitX = static_cast<int>(offsetX) + 6;
            int digitY = static_cast<int>(offsetY) + 6;

            if (rotation == DEG_90) {
                digitX = static_cast<int>(offsetX) + 6;
                digitY = static_cast<int>(offsetY) + 6;
            } else if (rotation == DEG_180) {
                digitX = static_cast<int>(offsetX) + 7;
                digitY = static_cast<int>(offsetY) + 5;
            } else if (rotation == DEG_270) {
                digitX = static_cast<int>(offsetX) + 7;
                digitY = static_cast<int>(offsetY) + 5;
            }

            if (panelIndex < 10) {
                drawDigit(buffer, width, height, digitX, digitY, static_cast<int>(panelIndex), numR, numG, numB);
            } else if (panelIndex < 100) {
                const int tens = static_cast<int>(panelIndex / 10);
                const int units = static_cast<int>(panelIndex % 10);
                drawDigit(buffer, width, height, digitX, static_cast<int>(offsetY) + 3, tens, numR, numG, numB);
                drawDigit(buffer, width, height, digitX, static_cast<int>(offsetY) + 8, units, numR, numG, numB);
            }
        }
    }
}