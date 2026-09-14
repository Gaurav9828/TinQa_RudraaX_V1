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
    const char* getName() const override { return "Mountain Sunrise Glow"; }

    void renderWithPhase(uint8_t* buffer, size_t width, size_t height, float phase, float direction_degrees);

    float getProgress() const { return m_progress; }

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
    
    // Smooth dampening state variables to eliminate hyperlapse stepping/flicker
    float m_smoothed_progress = 0.0f;
    std::vector<uint8_t> m_previous_frame_buffer;
};

#endif // SUNRISE_EFFECT_H