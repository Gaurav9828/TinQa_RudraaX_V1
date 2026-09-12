#ifndef SUNSET_EFFECT_H
#define SUNSET_EFFECT_H

#include "effects/IEffect.h"
#include "config/AppConfig.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

class SunsetEffect : public IEffect {
public:
    SunsetEffect();
    ~SunsetEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Mountain Sunset Glow"; }

    void renderWithPhase(uint8_t* buffer, size_t width, size_t height, float phase, float direction_degrees);

    // Added getter for autonomous state machine cycling
    float getProgress() const { return m_progress; }

private:
    struct ColorRGB {
        float r, g, b;
    };

    void parseConfig();
    void updateDirectionVector(float direction_degrees);
    ColorRGB getSunsetColor(float phase) const;
    static float clampf(float val, float min_val, float max_val);

private:
    std::vector<std::pair<size_t, size_t>> m_end_coordinates;
    float m_sun_direction_x = -1.0f;
    float m_sun_direction_y = 0.0f;
    float m_progress = 1.0f; // Starts at 1.0f (Warm Daylight) and fades down to 0.0f (Night)
};

#endif // SUNSET_EFFECT_H