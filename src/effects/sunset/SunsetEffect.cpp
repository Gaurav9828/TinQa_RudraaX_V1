#include "effects/sunset/SunsetEffect.h"
#include <cmath>
#include <algorithm>
#include <cstring>

SunsetEffect::SunsetEffect() {
    init();
}

void SunsetEffect::init() {
    m_progress = 1.0f; 
    m_smoothed_progress = -1.0f; // -1.0f flags uninitialized state for instant first-frame snap
    m_previous_frame_buffer.clear();
    parseConfig();
}

void SunsetEffect::parseConfig() {
    m_end_coordinates.clear();
    for (size_t idx : Config::SUNSET_END_PEAK_INDICES) {
        size_t x = idx % Config::MATRIX_WIDTH;
        size_t y = idx / Config::MATRIX_WIDTH;
        m_end_coordinates.push_back({x, y});
    }
    updateDirectionVector(Config::EAST_DIRECTION_DEGREES - 180.0f);
}

void SunsetEffect::updateDirectionVector(float direction_degrees) {
    float radians = direction_degrees * (Config::PI / 180.0f);
    m_sun_direction_x = std::cos(radians);
    m_sun_direction_y = -std::sin(radians); 
}

float SunsetEffect::clampf(float val, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, val));
}

SunsetEffect::ColorRGB SunsetEffect::getSunsetColor(float phase) const {
    phase = clampf(phase, 0.0f, 1.0f);

    const ColorRGB COLOR_OFF           = {  0.0f,   0.0f,   0.0f};
    const ColorRGB COLOR_DEEP_RED      = {220.0f,  15.0f,   0.0f}; 
    const ColorRGB COLOR_GOLDEN_RED    = {255.0f,  65.0f,   0.0f}; 
    const ColorRGB COLOR_GOLDEN_YELLOW = {255.0f, 150.0f,  10.0f}; 
    const ColorRGB COLOR_SOFT_GOLD     = {255.0f, 200.0f,  80.0f}; 
    const ColorRGB COLOR_OFF_WHITE     = {245.0f, 235.0f, 125.0f}; 

    if (phase <= 0.001f) {
        return COLOR_OFF;
    } 
    else if (phase < 0.15f) {
        float t = phase / 0.15f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_DEEP_RED.r * t,
            COLOR_DEEP_RED.g * t,
            COLOR_DEEP_RED.b * t
        };
    }
    else if (phase < 0.30f) {
        float t = (phase - 0.15f) / 0.15f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_DEEP_RED.r + (COLOR_GOLDEN_RED.r - COLOR_DEEP_RED.r) * t,
            COLOR_DEEP_RED.g + (COLOR_GOLDEN_RED.g - COLOR_DEEP_RED.g) * t,
            COLOR_DEEP_RED.b + (COLOR_GOLDEN_RED.b - COLOR_DEEP_RED.b) * t
        };
    }
    else if (phase < 0.70f) {
        float t = (phase - 0.30f) / 0.40f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_GOLDEN_RED.r + (COLOR_GOLDEN_YELLOW.r - COLOR_GOLDEN_RED.r) * t,
            COLOR_GOLDEN_RED.g + (COLOR_GOLDEN_YELLOW.g - COLOR_GOLDEN_RED.g) * t,
            COLOR_GOLDEN_RED.b + (COLOR_GOLDEN_YELLOW.b - COLOR_GOLDEN_RED.b) * t
        };
    }
    else {
        float t = (phase - 0.70f) / 0.30f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_GOLDEN_YELLOW.r + (COLOR_OFF_WHITE.r - COLOR_GOLDEN_YELLOW.r) * t,
            COLOR_GOLDEN_YELLOW.g + (COLOR_OFF_WHITE.g - COLOR_GOLDEN_YELLOW.g) * t,
            COLOR_GOLDEN_YELLOW.b + (COLOR_OFF_WHITE.b - COLOR_GOLDEN_YELLOW.b) * t
        };
    }
}

void SunsetEffect::update(uint32_t delta_ms) {
    float total_duration_ms = (Config::SUNRISE_DURATION_MINUTES * 60.0f * 1000.0f) * 0.5f;
    m_progress -= static_cast<float>(delta_ms) / total_duration_ms;
    if (m_progress < 0.0f) m_progress = 0.0f;
}

void SunsetEffect::render(uint8_t* buffer, size_t width, size_t height) {
    renderWithPhase(buffer, width, height, m_progress, Config::EAST_DIRECTION_DEGREES - 180.0f);
}

void SunsetEffect::renderWithPhase(uint8_t* buffer, size_t width, size_t height, float phase, float direction_degrees) {
    if (!buffer || width == 0 || height == 0) return;

    size_t total_bytes = width * height * 3;
    if (m_previous_frame_buffer.size() != total_bytes) {
        m_previous_frame_buffer.resize(total_bytes, 0);
    }

    // Instantly snap smoothed progress on first frame to prevent animation/replay pop
    bool is_first_frame = (m_smoothed_progress < 0.0f);
    if (is_first_frame) {
        m_smoothed_progress = phase;
    } else {
        float smoothing_factor = 0.20f; 
        m_smoothed_progress += (phase - m_smoothed_progress) * smoothing_factor;
    }

    if (m_smoothed_progress <= 0.001f && phase <= 0.001f) {
        std::fill(buffer, buffer + total_bytes, 0);
        std::fill(m_previous_frame_buffer.begin(), m_previous_frame_buffer.end(), 0);
        m_smoothed_progress = 0.0f;
        return;
    }

    updateDirectionVector(direction_degrees);

    float max_possible_dist = std::hypot(static_cast<float>(width), static_cast<float>(height));
    float current_wave_radius = m_smoothed_progress * max_possible_dist * 1.6f;

    float p = clampf(m_smoothed_progress, 0.0f, 1.0f);
    float global_brightness_scale = p * p * (3.0f - 2.0f * p);

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            float min_effective_dist = 99999.0f;

            for (const auto& end_pt : m_end_coordinates) {
                float px = static_cast<float>(end_pt.first);
                float py = static_cast<float>(end_pt.second);

                float dx = fx - px;
                float dy = fy - py;
                float euclidean_dist = std::hypot(dx, dy);

                float directional_dot = (dx * m_sun_direction_x + dy * m_sun_direction_y);
                float directional_penalty = (directional_dot < 0.0f) ? 0.15f * std::abs(directional_dot) : 1.4f * directional_dot;

                float effective_dist = euclidean_dist + directional_penalty;
                if (effective_dist < min_effective_dist) {
                    min_effective_dist = effective_dist;
                }
            }

            float pixel_phase = 0.0f;
            if (current_wave_radius > 0.001f) {
                float wave_delta = current_wave_radius - min_effective_dist;
                pixel_phase = clampf(wave_delta / (max_possible_dist * 0.65f), 0.0f, 1.0f);
            }

            ColorRGB rgb = getSunsetColor(pixel_phase);

            float norm_r = rgb.r * global_brightness_scale / 255.0f;
            float norm_g = rgb.g * global_brightness_scale / 255.0f;
            float norm_b = rgb.b * global_brightness_scale / 255.0f;

            norm_r = std::pow(norm_r, 1.8f);
            norm_g = std::pow(norm_g, 1.8f);
            norm_b = std::pow(norm_b, 1.8f);

            uint8_t target_r = static_cast<uint8_t>(clampf(norm_r * 255.0f, 0.0f, 255.0f));
            uint8_t target_g = static_cast<uint8_t>(clampf(norm_g * 255.0f, 0.0f, 255.0f));
            uint8_t target_b = static_cast<uint8_t>(clampf(norm_b * 255.0f, 0.0f, 255.0f));

            size_t pixel_index = (y * width + x) * 3;

            uint8_t final_r, final_g, final_b;
            if (is_first_frame) {
                // Instantly set target values on first frame to eliminate fade-in/replay lag
                final_r = target_r;
                final_g = target_g;
                final_b = target_b;
            } else {
                uint8_t prev_r = m_previous_frame_buffer[pixel_index];
                uint8_t prev_g = m_previous_frame_buffer[pixel_index + 1];
                uint8_t prev_b = m_previous_frame_buffer[pixel_index + 2];

                final_r = static_cast<uint8_t>(prev_r + (static_cast<float>(target_r - prev_r) * 0.45f));
                final_g = static_cast<uint8_t>(prev_g + (static_cast<float>(target_g - prev_g) * 0.45f));
                final_b = static_cast<uint8_t>(prev_b + (static_cast<float>(target_b - prev_b) * 0.45f));
            }

            buffer[pixel_index]     = final_r;
            buffer[pixel_index + 1] = final_g;
            buffer[pixel_index + 2] = final_b;

            m_previous_frame_buffer[pixel_index]     = final_r;
            m_previous_frame_buffer[pixel_index + 1] = final_g;
            m_previous_frame_buffer[pixel_index + 2] = final_b;
        }
    }
}