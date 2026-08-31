#ifndef THUNDER_EFFECT_H
#define THUNDER_EFFECT_H

#include "effects/IEffect.h"
#include <vector>
#include <cstdint>
#include <cstddef>

enum class LightningColorType {
    PURE_WHITE,         // Pure 255, 255, 255
    ELECTRIC_BLUE_WHITE,// Electric ionized white with high blue saturation
    WARM_GOLDEN_WHITE   // Deep ground stroke white
};

struct Point2D {
    float x; // Normalised 0.0 -> 1.0
    float y; // Normalised 0.0 -> 1.0
};

class ThunderEffect : public IEffect {
public:
    ThunderEffect();
    ~ThunderEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Thunder"; }

private:
    uint32_t stageTimerMs;
    uint32_t nextEventDelayMs;
    bool isStrikeActive;

    // Rapid Whole-Panel Strobe Special Mode
    bool isRapidBlinking;
    int rapidBlinkCount;
    uint32_t rapidBlinkTimerMs;
    bool rapidBlinkState;

    // Active Strike Attributes
    int activePatternId;
    LightningColorType currentColorType;
    float currentBrightness; 
    float decayRate;         
    float flashIntensity;    
    
    // Strobe / Multi-hit handling
    int reFlickerCount;

    // Vector Path Points (Normalised 0.0 -> 1.0)
    std::vector<Point2D> boltPoints;

    // Helper functions
    uint32_t getRandomRange(uint32_t min_val, uint32_t max_val);
    float getRandomFloat();
    void generateNextStrike();
    void executePattern(int patternId);
    void generateZigZagPath(bool vertical, float startPos);
    void generateForkedPath();
    void getLightningRGB(LightningColorType colorType, float intensity, uint8_t &r, uint8_t &g, uint8_t &b);
};

#endif // THUNDER_EFFECT_H