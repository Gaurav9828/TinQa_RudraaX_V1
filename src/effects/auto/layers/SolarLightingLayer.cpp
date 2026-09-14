#include "SolarLightingLayer.h"
#include "../../../config/AppConfig.h"
#include <cmath>
#include <algorithm>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t); // Smooth hermite curve to prevent linear pops
    }
}

void SolarLightingLayer::render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) {
    if (!buffer || width == 0 || height == 0) return;

    float daylight = 0.0f;

    // Smoother 24-hour cycle boundaries to completely eliminate frame blinking
    if (ctx.current_hour >= 5.0 && ctx.current_hour < 7.0) {
        daylight = smoothstep(5.0f, 7.0f, static_cast<float>(ctx.current_hour));
    } else if (ctx.current_hour >= 7.0 && ctx.current_hour <= 17.5) {
        daylight = 1.0f;
    } else if (ctx.current_hour > 17.5 && ctx.current_hour <= 19.5) {
        daylight = 1.0f - smoothstep(17.5f, 19.5f, static_cast<float>(ctx.current_hour));
    }

    float solar_peak = 0.0f;
    if (ctx.current_hour >= 6.0 && ctx.current_hour <= 18.0) {
        solar_peak = std::sin(static_cast<float>(((ctx.current_hour - 6.0) / 12.0) * Config::PI));
        solar_peak = std::clamp(solar_peak, 0.0f, 1.0f);
    }

    const float cloud_day_factor = 1.0f - (ctx.weather.cloud_density * 0.40f);

    // Warm white ambient background spectrum
    const float day_r = (245.0f + (255.0f - 245.0f) * solar_peak) * cloud_day_factor;
    const float day_g = (195.0f + (225.0f - 195.0f) * solar_peak) * cloud_day_factor;
    const float day_b = (90.0f +  (130.0f - 90.0f) * solar_peak) * cloud_day_factor;

    const uint8_t r = static_cast<uint8_t>(std::clamp(day_r * daylight, 0.0f, 255.0f));
    const uint8_t g = static_cast<uint8_t>(std::clamp(day_g * daylight, 0.0f, 255.0f));
    const uint8_t b = static_cast<uint8_t>(std::clamp(day_b * daylight, 0.0f, 255.0f));

    const size_t total_pixels = width * height;
    for (size_t i = 0; i < total_pixels; ++i) {
        buffer[i * 3 + 0] = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }

    // --- Sun Cluster Entity Rendering (10 LEDs) ---
    // Active during daylight hours, gracefully initializing as sunrise ends (6.0h - 7.0h) 
    // and fading out as sunset begins (17.0h - 18.0h).
    if (ctx.current_hour >= 6.0 && ctx.current_hour <= 18.0) {
        float sun_visibility = 1.0f;
        if (ctx.current_hour >= 6.0 && ctx.current_hour <= 7.0) {
            sun_visibility = smoothstep(6.0f, 7.0f, static_cast<float>(ctx.current_hour));
        } else if (ctx.current_hour >= 17.0 && ctx.current_hour <= 18.0) {
            sun_visibility = 1.0f - smoothstep(17.0f, 18.0f, static_cast<float>(ctx.current_hour));
        }

        // Cloud attenuation on the sun cluster entity
        sun_visibility *= (1.0f - (ctx.weather.cloud_density * 0.50f));

        if (sun_visibility > 0.0f) {
            float day_progress = static_cast<float>((ctx.current_hour - 6.0) / 12.0);
            day_progress = std::clamp(day_progress, 0.0f, 1.0f);

            int matrix_width = static_cast<int>(Config::MATRIX_WIDTH);
            int matrix_height = static_cast<int>(Config::MATRIX_HEIGHT);

            // Trajectory identical to the moon: bottom corner D -> center O -> opposite bottom corner C
            int sun_x = static_cast<int>((1.0f - day_progress) * static_cast<float>(matrix_width - 1));
            sun_x = std::clamp(sun_x, 0, matrix_width - 1);

            float bottom_y = static_cast<float>(matrix_height - 1);
            float center_y = bottom_y / 2.0f;
            float arc_offset = std::sin(day_progress * Config::PI) * (bottom_y - center_y);
            int sun_y = static_cast<int>(bottom_y - arc_offset);
            sun_y = std::clamp(sun_y, 0, matrix_height - 1);

            // 10 distinct LED cluster offsets relative to the center coordinate (dx, dy, intensity weight)
            struct SunPixel {
                int dx;
                int dy;
                float weight;
            };

            const SunPixel sun_cluster[10] = {
                { 0,  0, 1.0f },  // Core center (brightest)
                {-1,  0, 0.85f}, { 1,  0, 0.85f}, // Horizontal neighbors
                { 0, -1, 0.85f}, { 0,  1, 0.85f}, // Vertical neighbors
                {-1, -1, 0.65f}, { 1, -1, 0.65f}, // Diagonal corners
                {-1,  1, 0.65f}, { 1,  1, 0.65f},
                { 0, -2, 0.50f}   // Extension top LED for rich cluster shape
            };

            for (const auto& p : sun_cluster) {
                int px = sun_x + p.dx;
                int py = sun_y + p.dy;

                if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height) {
                    const size_t pixel_idx = (py * matrix_width + px) * 3;

                    // Rich, dark golden-yellow color distinct from warm white background
                    float brightness = 230.0f * sun_visibility * p.weight;
                    uint8_t sun_r = static_cast<uint8_t>(std::clamp(brightness, 0.0f, 255.0f));
                    uint8_t sun_g = static_cast<uint8_t>(std::clamp(brightness * 0.75f, 0.0f, 255.0f)); // Deep golden amber green channel
                    uint8_t sun_b = static_cast<uint8_t>(std::clamp(brightness * 0.02f, 0.0f, 255.0f)); // Minimized blue for vivid dark yellow

                    // Blend rich dark yellow sun cluster pixels over the warm white background
                    buffer[pixel_idx + 0] = std::min<uint16_t>(255, buffer[pixel_idx + 0] + sun_r);
                    buffer[pixel_idx + 1] = std::min<uint16_t>(255, buffer[pixel_idx + 1] + sun_g);
                    buffer[pixel_idx + 2] = std::min<uint16_t>(255, buffer[pixel_idx + 2] + sun_b);
                }
            }
        }
    }
}