#ifndef AURORA_EFFECT_H
#define AURORA_EFFECT_H

#include "effects/IEffect.h"
#include <cstdint>
#include <cstddef>
#include <vector>

enum class AuroraTheme {
    EMERALD_CANOPY = 0, // Deep green swirls & high contrast rays
    LIME_HORIZON,       // Warm yellow-green gradient diffuse sky
    CRIMSON_CORONA,     // Soft green fading into vibrant pink/magenta tops
    VIOLET_TEMPEST,     // Cyan vertical rays with brilliant purple sky
    CLASSIC_ARC,        // Intense, sweep-directional green arc
    POLARIS_DUSK,       // Deep indigo, teal, and violet multi-layered curtain
    COUNT
};

struct ThemeColorStop {
    float pos;
    float r, g, b;
};

class AuroraEffect : public IEffect {
public:
    AuroraEffect();
    ~AuroraEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Aurora Dynamic Multi-Theme"; }

private:
    float time;
    
    // Theme Transition State
    AuroraTheme currentTheme;
    AuroraTheme targetTheme;
    float transitionProgress; // 0.0 to 1.0
    float themeTimer;         // Time spent in current state
    float themeDuration;      // Duration before triggering next switch (30s - 120s)

    // Dynamic wave directional state
    float dirX1, dirY1;
    float dirX2, dirY2;
    float waveSpeedMult;

    void selectRandomThemePair();
    void updateMotionParameters();
    void getThemeColor(AuroraTheme theme, float normY, float &r, float &g, float &b);
    void getBlendedColor(float normY, float intensity, uint8_t &r, uint8_t &g, uint8_t &b);
};

#endif // AURORA_EFFECT_H