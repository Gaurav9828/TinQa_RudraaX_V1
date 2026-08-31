#ifndef SUNRISE_EFFECT_H
#define SUNRISE_EFFECT_H

#include "effects/IEffect.h"
#include "config/AppConfig.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

class SunriseEffect : public IEffect {
public:
    SunriseEffect();
    ~SunriseEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Mountain Alpenglow Sunrise"; }

    // Direct interface for manual/smooth phase rendering in combined modes
    void renderWithPhase(uint8_t* buffer, size_t width, size_t height, float phase, float direction_degrees);

private:
    struct ColorRGB {
        float r, g, b;
    };

    void parseConfig();
    void updateDirectionVector(float direction_degrees);
    ColorRGB getAlpenglowColor(float phase) const;
    static float clampf(float val, float min_val, float max_val);

private:
    std::vector<std::pair<size_t, size_t>> m_peak_coordinates;
    float m_sun_direction_x = 1.0f;
    float m_sun_direction_y = 0.0f;
    float m_progress = 0.0f;

    const ColorRGB COLOR_NIGHT    = {3.0f,   5.0f,  15.0f};  
    const ColorRGB COLOR_CRIMSON  = {255.0f, 25.0f,  0.0f};  
    const ColorRGB COLOR_GOLDEN   = {255.0f, 160.0f, 0.0f};  
    const ColorRGB COLOR_SOFT_SUN = {255.0f, 220.0f, 140.0f};
    const ColorRGB COLOR_DAYLIGHT = {140.0f, 140.0f, 135.0f};};

#endif // SUNRISE_EFFECT_H