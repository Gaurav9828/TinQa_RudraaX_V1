#include "SolarLightingLayer.h"
#include "../../../config/AppConfig.h"
#include <cmath>
#include <algorithm>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

SolarLightingLayer::SolarLightingLayer() {
    init();
}

void SolarLightingLayer::init() {
    parseConfig();
}

void SolarLightingLayer::parseConfig() {
    m_peak_coordinates.clear();
    for (size_t idx : Config::SUNRISE_PEAK_INDICES) {
        size_t x = idx % Config::MATRIX_WIDTH;
        size_t y = idx / Config::MATRIX_WIDTH;
        m_peak_coordinates.push_back({x, y});
    }
    float radians = Config::EAST_DIRECTION_DEGREES * (Config::PI / 180.0f);
    m_sun_direction_x = std::cos(radians);
    m_sun_direction_y = -std::sin(radians);
}

float SolarLightingLayer::clampf(float val, float min_val, float max_val) const {
    return std::max(min_val, std::min(max_val, val));
}

void SolarLightingLayer::render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) {
    if (!buffer || width == 0 || height == 0) return;

    double sunrise = ctx.weather.sunrise_hour;
    double sunset = ctx.weather.sunset_hour;
    double twilight_dur = ctx.weather.redness_duration_hours;

    double dawn_start = sunrise - twilight_dur;
    double dawn_end = sunrise + 3.0; 
    double dusk_start = sunset - 3.0;
    double dusk_end = sunset + twilight_dur;

    float global_daylight = 0.0f;
    float spatial_expand_progress = 1.0f;
    bool is_dusk = false;

    if (ctx.current_hour >= dawn_start && ctx.current_hour < dawn_end) {
        float progress = static_cast<float>((ctx.current_hour - dawn_start) / (dawn_end - dawn_start));
        global_daylight = smoothstep(0.0f, 1.0f, progress);
        spatial_expand_progress = smoothstep(0.0f, 1.0f, progress);
    } 
    else if (ctx.current_hour >= dawn_end && ctx.current_hour <= dusk_start) {
        global_daylight = 1.0f;
        spatial_expand_progress = 1.0f;
    } 
    else if (ctx.current_hour > dusk_start && ctx.current_hour <= dusk_end) {
        float progress = static_cast<float>((ctx.current_hour - dusk_start) / (dusk_end - dusk_start));
        global_daylight = 1.0f - smoothstep(0.0f, 1.0f, progress);
        spatial_expand_progress = 1.0f - smoothstep(0.0f, 1.0f, progress);
        is_dusk = true; // Mark as ending phase to mirror direction
    } else {
        return; 
    }

    // Adjust direction vector: mirror it during dusk to copy sunset direction flow
    float active_dir_deg = Config::EAST_DIRECTION_DEGREES;
    if (is_dusk) {
        active_dir_deg -= 180.0f; // Mirror direction for sunset ending
    }
    float rads = active_dir_deg * (Config::PI / 180.0f);
    float dir_x = std::cos(rads);
    float dir_y = -std::sin(rads);

    float day_r = 255.0f;
    float day_g = 140.0f;
    float day_b = 40.0f;

    if (ctx.current_hour >= sunrise && ctx.current_hour <= sunset) {
        float day_progress = static_cast<float>((ctx.current_hour - sunrise) / (sunset - sunrise));
        day_progress = std::clamp(day_progress, 0.0f, 1.0f);

        float r1 = 255.0f, g1 = 140.0f, b1 =  40.0f;
        float r2 = 255.0f, g2 = 195.0f, b2 =  90.0f; 
        float r3 = 255.0f, g3 = 140.0f, b3 =  40.0f;

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

    float cloud_day_factor = 1.0f - (ctx.weather.cloud_density * 0.30f);
    const uint8_t base_r = static_cast<uint8_t>(clampf(day_r * cloud_day_factor * global_daylight, 0.0f, 255.0f));
    const uint8_t base_g = static_cast<uint8_t>(clampf(day_g * cloud_day_factor * global_daylight, 0.0f, 255.0f));
    const uint8_t base_b = static_cast<uint8_t>(clampf(day_b * cloud_day_factor * global_daylight, 0.0f, 255.0f));

    // --- GRADUAL SPATIAL WAVE MASK (Fixed Starting Jump) ---
    float max_possible_dist = std::hypot(static_cast<float>(width), static_cast<float>(height));
    // Smoother scaling curve for gradual LED illumination
    float current_wave_radius = spatial_expand_progress * max_possible_dist * 2.2f;

    int matrix_width = static_cast<int>(width);
    int matrix_height = static_cast<int>(height);

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            float min_effective_dist = 99999.0f;
            for (const auto& peak : m_peak_coordinates) {
                float px = static_cast<float>(peak.first);
                float py = static_cast<float>(peak.second);

                float dx = fx - px;
                float dy = fy - py;
                float euclidean_dist = std::hypot(dx, dy);

                float directional_dot = (dx * dir_x + dy * dir_y);
                float directional_penalty = (directional_dot < 0.0f) ? 0.2f * std::abs(directional_dot) : 1.2f * directional_dot;

                float effective_dist = euclidean_dist + directional_penalty;
                if (effective_dist < min_effective_dist) {
                    min_effective_dist = effective_dist;
                }
            }

            float spatial_mask = 1.0f;
            if (spatial_expand_progress < 1.0f) {
                float wave_delta = current_wave_radius - min_effective_dist;
                // Wider divisor band ensures gradual, slow individual LED turn-on transitions
                spatial_mask = clampf(wave_delta / (max_possible_dist * 0.75f), 0.0f, 1.0f);
                spatial_mask = spatial_mask * spatial_mask * (3.0f - 2.0f * spatial_mask);
            }

            if (spatial_mask > 0.0f) {
                size_t pixel_idx = (y * width + x) * 3;
                uint8_t final_r = static_cast<uint8_t>(base_r * spatial_mask);
                uint8_t final_g = static_cast<uint8_t>(base_g * spatial_mask);
                uint8_t final_b = static_cast<uint8_t>(base_b * spatial_mask);

                buffer[pixel_idx + 0] = std::min<uint16_t>(255, buffer[pixel_idx + 0] + final_r);
                buffer[pixel_idx + 1] = std::min<uint16_t>(255, buffer[pixel_idx + 1] + final_g);
                buffer[pixel_idx + 2] = std::min<uint16_t>(255, buffer[pixel_idx + 2] + final_b);
            }
        }
    }

    // --- Sun Entity Cluster ---
    if (ctx.current_hour >= sunrise && ctx.current_hour <= sunset) {
        float sun_visibility = 1.0f;
        double sun_fade_duration = 3.0; 
        if (ctx.current_hour >= sunrise && ctx.current_hour <= (sunrise + sun_fade_duration)) {
            sun_visibility = smoothstep(static_cast<float>(sunrise), static_cast<float>(sunrise + sun_fade_duration), static_cast<float>(ctx.current_hour));
        } else if (ctx.current_hour >= (sunset - sun_fade_duration) && ctx.current_hour <= sunset) {
            sun_visibility = 1.0f - smoothstep(static_cast<float>(sunset - sun_fade_duration), static_cast<float>(sunset), static_cast<float>(ctx.current_hour));
        }

        sun_visibility *= (1.0f - (ctx.weather.cloud_density * 0.40f));

        if (sun_visibility > 0.0f) {
            float base_sun_intensity = 230.0f * global_daylight; 
            float day_progress = static_cast<float>((ctx.current_hour - sunrise) / (sunset - sunrise));
            day_progress = std::clamp(day_progress, 0.0f, 1.0f);

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
                    uint8_t sun_r = static_cast<uint8_t>(clampf(brightness, 0.0f, 255.0f));
                    uint8_t sun_g = static_cast<uint8_t>(clampf(brightness * 0.78f, 0.0f, 255.0f)); 
                    uint8_t sun_b = static_cast<uint8_t>(clampf(brightness * 0.08f, 0.0f, 255.0f)); 

                    buffer[pixel_idx + 0] = std::min<uint16_t>(255, buffer[pixel_idx + 0] + sun_r);
                    buffer[pixel_idx + 1] = std::min<uint16_t>(255, buffer[pixel_idx + 1] + sun_g);
                    buffer[pixel_idx + 2] = std::min<uint16_t>(255, buffer[pixel_idx + 2] + sun_b);
                }
            }
        }
    }
}