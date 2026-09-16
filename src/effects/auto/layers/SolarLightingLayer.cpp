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

    // Use dynamic sunrise and sunset times from WeatherProvider
    double sunrise = ctx.weather.sunrise_hour;
    double sunset = ctx.weather.sunset_hour;
    double twilight_dur = ctx.weather.redness_duration_hours;

    // --- GRADUAL DAWN / DUSK RAMP ---
    // Stretched morning ramp so it builds up gradually across 3 hours post-sunrise instead of popping instantly
    double morning_ramp_end = sunrise + 3.0; 
    double evening_ramp_start = sunset - 3.0;

    if (ctx.current_hour >= (sunrise - twilight_dur) && ctx.current_hour < morning_ramp_end) {
        daylight = smoothstep(static_cast<float>(sunrise - twilight_dur), static_cast<float>(morning_ramp_end), static_cast<float>(ctx.current_hour));
    } else if (ctx.current_hour >= morning_ramp_end && ctx.current_hour <= evening_ramp_start) {
        daylight = 1.0f;
    } else if (ctx.current_hour > evening_ramp_start && ctx.current_hour <= (sunset + twilight_dur)) {
        daylight = 1.0f - smoothstep(static_cast<float>(evening_ramp_start), static_cast<float>(sunset + twilight_dur), static_cast<float>(ctx.current_hour));
    }

    // Base background colors matching sunrise/sunset endpoints
    float day_r = 245.0f;
    float day_g = 235.0f;
    float day_b = 125.0f;

    // Dynamic daytime color progression mapped dynamically between sunrise and sunset
    if (ctx.current_hour >= sunrise && ctx.current_hour <= sunset) {
        float day_progress = static_cast<float>((ctx.current_hour - sunrise) / (sunset - sunrise));
        day_progress = std::clamp(day_progress, 0.0f, 1.0f);

        float r1 = 245.0f, g1 = 235.0f, b1 = 125.0f;
        float r2 = 255.0f, g2 = 215.0f, b2 =  45.0f;
        float r3 = 245.0f, g3 = 235.0f, b3 = 125.0f;

        if (day_progress <= 0.5f) {
            float t = day_progress / 0.5f;
            t = t * t * (3.0f - 2.0f * t);
            day_r = r1 + (r2 - r1) * t;
            day_g = g1 + (g2 - g1) * t;
            day_b = b1 + (b2 - b1) * t;
        } else {
            float t = (day_progress - 0.5f) / 0.5f;
            t = t * t * (3.0f - 2.0f * t);
            day_r = r2 + (r3 - r2) * t;
            day_g = g2 + (g3 - g2) * t;
            day_b = b2 + (b3 - b2) * t;
        }
    }

    // --- SMOOTH FADE-IN / FADE-OUT STORM TRANSITION ---
    float cloud_day_factor = 1.0f - (ctx.weather.cloud_density * 0.30f);
    float storm_transition_weight = 0.0f;

    // Scale storm window dynamically relative to daylight bounds
    float storm_start = static_cast<float>(sunrise + 3.0);
    float storm_end = static_cast<float>(sunset - 1.0);
    if (ctx.weather.cloud_density >= 0.95f && ctx.current_hour >= storm_start && ctx.current_hour <= sunset) {
        if (ctx.current_hour < (sunrise + 5.0)) {
            storm_transition_weight = smoothstep(static_cast<float>(storm_start), static_cast<float>(sunrise + 5.0), static_cast<float>(ctx.current_hour));
        } else if (ctx.current_hour <= storm_end) {
            storm_transition_weight = 1.0f;
        } else {
            storm_transition_weight = 1.0f - smoothstep(static_cast<float>(storm_end), static_cast<float>(sunset), static_cast<float>(ctx.current_hour));
        }
    }

    if (storm_transition_weight > 0.0f) {
        float target_cloud_factor = 0.08f;
        cloud_day_factor = cloud_day_factor * (1.0f - storm_transition_weight) + target_cloud_factor * storm_transition_weight;

        float storm_r = 120.0f;
        float storm_g = 140.0f;
        float storm_b = 175.0f;

        day_r = day_r * (1.0f - storm_transition_weight) + storm_r * storm_transition_weight;
        day_g = day_g * (1.0f - storm_transition_weight) + storm_g * storm_transition_weight;
        day_b = day_b * (1.0f - storm_transition_weight) + storm_b * storm_transition_weight;
    }

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
    if (ctx.current_hour >= sunrise && ctx.current_hour <= sunset) {
        float sun_visibility = 0.0f;
        
        // Match sun fade duration to the expanded morning ramp window
        double sun_fade_duration = 3.0; 
        if (ctx.current_hour >= sunrise && ctx.current_hour <= (sunrise + sun_fade_duration)) {
            sun_visibility = smoothstep(static_cast<float>(sunrise), static_cast<float>(sunrise + sun_fade_duration), static_cast<float>(ctx.current_hour));
        } else if (ctx.current_hour >= (sunset - sun_fade_duration) && ctx.current_hour <= sunset) {
            sun_visibility = 1.0f - smoothstep(static_cast<float>(sunset - sun_fade_duration), static_cast<float>(sunset), static_cast<float>(ctx.current_hour));
        } else {
            sun_visibility = 1.0f; 
        }

        float sun_storm_dimming = 1.0f - (storm_transition_weight * 0.85f);
        sun_visibility *= sun_storm_dimming * (1.0f - (ctx.weather.cloud_density * 0.40f));

        if (sun_visibility > 0.0f) {
            // Scale base sun intensity directly with current daylight factor so it dims nicely in early morning
            float base_sun_intensity = 230.0f * daylight; 
            
            float day_progress = static_cast<float>((ctx.current_hour - sunrise) / (sunset - sunrise));
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

            struct SunPixel { int dx; int dy; float weight; };
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

                    float brightness = base_sun_intensity * sun_visibility * p.weight;
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