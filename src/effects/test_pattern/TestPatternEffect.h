#ifndef TEST_PATTERN_EFFECT_H
#define TEST_PATTERN_EFFECT_H

#include "effects/IEffect.h"
#include <cstdint>
#include <cstddef>

class TestPatternEffect : public IEffect {
public:
    TestPatternEffect() = default;
    ~TestPatternEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Panel Orientation Test Pattern"; }

private:
    uint32_t animationTick = 0;

    void drawDigit(uint8_t* buffer, size_t width, size_t height, 
                   int startX, int startY, int number, 
                   uint8_t r, uint8_t g, uint8_t b);
};

#endif // TEST_PATTERN_EFFECT_H