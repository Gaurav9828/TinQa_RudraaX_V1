#include "LunarLightingLayer.h"
#include "config/AppConfig.h"
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

    // Normalize current hour into a continuous timeline starting from sunset start (~16.5h) 
    // through midnight to sunrise end (~8.5h next day / 32.5h).
    double adjusted_hour = ctx.current_hour;
    if (adjusted_hour < 8.5) {
        adjusted_hour += 24.0;
    }

    // Moon visibility strictly spans from the start of sunset (16.5h) to the end of sunrise (32.5h)
    if (adjusted_hour < 16.5 || adjusted_hour > 32.5) {
        return;
    }

    // Smooth fade-in during sunset and fade-out during sunrise
    float visibility = 1.0f;
    if (adjusted_hour >= 16.5 && adjusted_hour <= 18.0) {
        visibility = smoothstep(16.5f, 18.0f, static_cast<float>(adjusted_hour));
    } else if (adjusted_hour >= 31.0 && adjusted_hour <= 32.5) {
        visibility = 1.0f - smoothstep(31.0f, 32.5f, static_cast<float>(adjusted_hour));
    }

    // Cloud density attenuation
    visibility *= 1.0f - (ctx.weather.cloud_density * 0.70f);
    if (visibility <= 0.0f) return;

    // Retrieve moon illumination/phase factor calculated by WeatherProvider
    float phase_factor = ctx.weather.moon_phase_factor;

    // Check for absolute New Moon (0% illumination). 
    // All other moon days remain visible with luminosity proportional to phase factor.
    if (phase_factor < 0.005f) {
        return; 
    }

    // Maximum full moon brightness scaled to 50 max.
    float target_brightness = phase_factor * 50.0f * visibility;
    uint8_t core_val = static_cast<uint8_t>(std::clamp(target_brightness, 1.0f, 255.0f));

    // Dynamic trajectory calculation using matrix dimensions from AppConfig:
    // Starts at bottom row (D), arches to center (O), and ends at bottom row opposite side (C).
    float visibility_progress = static_cast<float>((adjusted_hour - 16.5) / 16.0);
    visibility_progress = std::clamp(visibility_progress, 0.0f, 1.0f);

    int matrix_width = static_cast<int>(Config::MATRIX_WIDTH);
    int matrix_height = static_cast<int>(Config::MATRIX_HEIGHT);

    int moon_x = static_cast<int>((1.0f - visibility_progress) * static_cast<float>(matrix_width - 1));
    moon_x = std::clamp(moon_x, 0, matrix_width - 1);

    float bottom_y = static_cast<float>(matrix_height - 1);
    float center_y = bottom_y / 2.0f;
    float arc_offset = std::sin(visibility_progress * 3.14159265f) * (bottom_y - center_y);
    int moon_y = static_cast<int>(bottom_y - arc_offset);
    moon_y = std::clamp(moon_y, 0, matrix_height - 1);

    const size_t core_idx = (moon_y * matrix_width + moon_x) * 3;

    // Color logic: Strictly white for all phases except Full Moon
    if (phase_factor >= 0.85f) {
        // Full Moon: Atmospheric color variations (red/orange near horizon -> yellow -> white at zenith)
        float color_progress = std::sin(visibility_progress * 3.14159265f);
        float red_multiplier = 1.15f + (1.0f - color_progress) * 0.4f;
        float green_multiplier = 1.0f + (color_progress * 0.15f);
        float blue_multiplier = 0.8f + (color_progress * 0.35f);

        if (visibility_progress < 0.15f || visibility_progress > 0.85f) {
            // Horizon moon: deep warm red/orange tint
            buffer[core_idx + 0] = std::min<uint16_t>(255, buffer[core_idx + 0] + core_val);
            buffer[core_idx + 1] = std::min<uint16_t>(255, buffer[core_idx + 1] + static_cast<uint8_t>(core_val * 0.5f));
            buffer[core_idx + 2] = std::min<uint16_t>(255, buffer[core_idx + 2] + static_cast<uint8_t>(core_val * 0.2f));
        } else {
            buffer[core_idx + 0] = std::min<uint16_t>(255, buffer[core_idx + 0] + static_cast<uint8_t>(core_val * red_multiplier));
            buffer[core_idx + 1] = std::min<uint16_t>(255, buffer[core_idx + 1] + static_cast<uint8_t>(core_val * green_multiplier));
            buffer[core_idx + 2] = std::min<uint16_t>(255, buffer[core_idx + 2] + static_cast<uint8_t>(core_val * blue_multiplier));
        }
    } else {
        // Non-Full Moon phases: Strictly pure white
        buffer[core_idx + 0] = std::min<uint16_t>(255, buffer[core_idx + 0] + core_val);
        buffer[core_idx + 1] = std::min<uint16_t>(255, buffer[core_idx + 1] + core_val);
        buffer[core_idx + 2] = std::min<uint16_t>(255, buffer[core_idx + 2] + static_cast<uint8_t>(core_val * 1.15f));
    }
}