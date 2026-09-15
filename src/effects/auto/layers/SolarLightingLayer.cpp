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

    // Synchronize dawn/dusk boundaries perfectly with Sunrise (ends ~7.0h) and Sunset (starts ~17.0h)
    if (ctx.current_hour >= 5.0 && ctx.current_hour < 7.0) {
        daylight = smoothstep(5.0f, 7.0f, static_cast<float>(ctx.current_hour));
    } else if (ctx.current_hour >= 7.0 && ctx.current_hour <= 17.0) {
        daylight = 1.0f;
    } else if (ctx.current_hour > 17.0 && ctx.current_hour <= 19.0) {
        daylight = 1.0f - smoothstep(17.0f, 19.0f, static_cast<float>(ctx.current_hour));
    }

    // Base background colors matching sunrise/sunset endpoints by default
    float day_r = 245.0f;
    float day_g = 235.0f;
    float day_b = 125.0f;

    // Dynamic daytime color progression (Morning -> Vibrant Afternoon Yellow -> Evening)
    if (ctx.current_hour >= 6.0 && ctx.current_hour <= 18.0) {
        float day_progress = static_cast<float>((ctx.current_hour - 6.0) / 12.0);
        day_progress = std::clamp(day_progress, 0.0f, 1.0f);

        // Milestone 1: Sunrise End / Morning start ({245.0f, 235.0f, 125.0f})
        float r1 = 245.0f, g1 = 235.0f, b1 = 125.0f;
        // Milestone 2: Midday / Mid-Afternoon vibrant golden yellow peak
        float r2 = 255.0f, g2 = 215.0f, b2 =  45.0f;
        // Milestone 3: Sunset Start / Evening ({245.0f, 235.0f, 125.0f})
        float r3 = 245.0f, g3 = 235.0f, b3 = 125.0f;

        if (day_progress <= 0.5f) {
            // Morning to mid-afternoon transition (smoothly warming into bright yellow)
            float t = day_progress / 0.5f;
            t = t * t * (3.0f - 2.0f * t); // Smoothstep curve
            day_r = r1 + (r2 - r1) * t;
            day_g = g1 + (g2 - g1) * t;
            day_b = b1 + (b2 - b1) * t;
        } else {
            // Afternoon to evening transition (dropping back to sunset entry color)
            float t = (day_progress - 0.5f) / 0.5f;
            t = t * t * (3.0f - 2.0f * t); // Smoothstep curve
            day_r = r2 + (r3 - r2) * t;
            day_g = g2 + (g3 - g2) * t;
            day_b = b2 + (b3 - b2) * t;
        }
    }

    const float cloud_day_factor = 1.0f - (ctx.weather.cloud_density * 0.40f);

    const uint8_t r = static_cast<uint8_t>(std::clamp((day_r * cloud_day_factor) * daylight, 0.0f, 255.0f));
    const uint8_t g = static_cast<uint8_t>(std::clamp((day_g * cloud_day_factor) * daylight, 0.0f, 255.0f));
    const uint8_t b = static_cast<uint8_t>(std::clamp((day_b * cloud_day_factor) * daylight, 0.0f, 255.0f));

    const size_t total_pixels = width * height;
    for (size_t i = 0; i < total_pixels; ++i) {
        buffer[i * 3 + 0] = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }

    // --- Sun Cluster Entity Rendering (10 LEDs) ---
    if (ctx.current_hour >= 6.0 && ctx.current_hour <= 18.0) {
        float sun_visibility = 1.0f;
        if (ctx.current_hour >= 6.0 && ctx.current_hour <= 7.0) {
            sun_visibility = smoothstep(6.0f, 7.0f, static_cast<float>(ctx.current_hour));
        } else if (ctx.current_hour >= 17.0 && ctx.current_hour <= 18.0) {
            sun_visibility = 1.0f - smoothstep(17.0f, 18.0f, static_cast<float>(ctx.current_hour));
        }

        sun_visibility *= (1.0f - (ctx.weather.cloud_density * 0.50f));

        if (sun_visibility > 0.0f) {
            float day_progress = static_cast<float>((ctx.current_hour - 6.0) / 12.0);
            day_progress = std::clamp(day_progress, 0.0f, 1.0f);

            int matrix_width = static_cast<int>(Config::MATRIX_WIDTH);
            int matrix_height = static_cast<int>(Config::MATRIX_HEIGHT);

            int sun_x = static_cast<int>((1.0f - day_progress) * static_cast<float>(matrix_width - 1));
            sun_x = std::clamp(sun_x, 0, matrix_width - 1);

            float bottom_y = static_cast<float>(matrix_height - 1);
            float center_y = bottom_y / 2.0f;
            float arc_offset = std::sin(day_progress * Config::PI) * (bottom_y - center_y);
            int sun_y = static_cast<int>(bottom_y - arc_offset);
            sun_y = std::clamp(sun_y, 0, matrix_height - 1);

            struct SunPixel {
                int dx;
                int dy;
                float weight;
            };

            const SunPixel sun_cluster[10] = {
                { 0,  0, 1.0f },  
                {-1,  0, 0.85f}, { 1,  0, 0.85f}, 
                { 0, -1, 0.85f}, { 0,  1, 0.85f}, 
                {-1, -1, 0.65f}, { 1, -1, 0.65f}, 
                {-1,  1, 0.65f}, { 1,  1, 0.65f},
                { 0, -2, 0.50f}   
            };

            for (const auto& p : sun_cluster) {
                int px = sun_x + p.dx;
                int py = sun_y + p.dy;

                if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height) {
                    const size_t pixel_idx = (py * matrix_width + px) * 3;

                    float brightness = 230.0f * sun_visibility * p.weight;
                    uint8_t sun_r = static_cast<uint8_t>(std::clamp(brightness, 0.0f, 255.0f));
                    uint8_t sun_g = static_cast<uint8_t>(std::clamp(brightness * 0.75f, 0.0f, 255.0f)); 
                    uint8_t sun_b = static_cast<uint8_t>(std::clamp(brightness * 0.02f, 0.0f, 255.0f)); 

                    buffer[pixel_idx + 0] = std::min<uint16_t>(255, buffer[pixel_idx + 0] + sun_r);
                    buffer[pixel_idx + 1] = std::min<uint16_t>(255, buffer[pixel_idx + 1] + sun_g);
                    buffer[pixel_idx + 2] = std::min<uint16_t>(255, buffer[pixel_idx + 2] + sun_b);
                }
            }
        }
    }
}