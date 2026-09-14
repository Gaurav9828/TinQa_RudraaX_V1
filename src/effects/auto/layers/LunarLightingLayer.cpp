#include "LunarLightingLayer.h"
#include <cmath>
#include <algorithm>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }
}

void LunarLightingLayer::render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) {
    if (!buffer || width == 0 || height == 0) return;

    if (ctx.current_hour >= 5.5 && ctx.current_hour <= 19.5) {
        return;
    }

    float moon_visibility = 1.0f;

    if (ctx.current_hour >= 19.5 && ctx.current_hour <= 20.5) {
        moon_visibility = smoothstep(0.0f, 1.0f, static_cast<float>(ctx.current_hour - 19.5));
    } else if (ctx.current_hour >= 4.5 && ctx.current_hour < 5.5) {
        moon_visibility = 1.0f - smoothstep(0.0f, 1.0f, static_cast<float>(ctx.current_hour - 4.5));
    }

    moon_visibility *= 1.0f - (ctx.weather.cloud_density * 0.70f);

    if (moon_visibility <= 0.0f || ctx.moon_phase_factor <= 0.01f) {
        return; // Totally dark on New Moon / No Moon
    }

    const int moon_x = static_cast<int>(width > 2 ? width - 2 : width - 1);
    const int moon_y = 1;

    // Brightness scaled directly by the astronomical moon phase factor (New Moon = 0, Full Moon = max single LED brightness)
    const uint8_t core_val = static_cast<uint8_t>(std::clamp((8.0f * ctx.moon_phase_factor) * moon_visibility, 0.0f, 255.0f));
    
    if (core_val == 0) return;

    const size_t core_idx = (moon_y * width + moon_x) * 3;

    // Single LED light rendering
    buffer[core_idx + 0] = std::min<uint16_t>(255, buffer[core_idx + 0] + core_val);
    buffer[core_idx + 1] = std::min<uint16_t>(255, buffer[core_idx + 1] + core_val);
    buffer[core_idx + 2] = std::min<uint16_t>(255, buffer[core_idx + 2] + static_cast<uint8_t>(core_val * 0.90f));
}