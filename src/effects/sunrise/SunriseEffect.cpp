#include "effects/sunrise/SunriseEffect.h"
#include <cmath>
#include <algorithm>
#include <cstring>

SunriseEffect::SunriseEffect() {
    init();
}

void SunriseEffect::init() {
    m_progress = 0.0f;
    m_smoothed_progress = 0.0f;
    m_previous_frame_buffer.clear();
    parseConfig();
}

void SunriseEffect::parseConfig() {
    m_peak_coordinates.clear();
    for (size_t idx : Config::SUNRISE_PEAK_INDICES) {
        size_t x = idx % Config::MATRIX_WIDTH;
        size_t y = idx / Config::MATRIX_WIDTH;
        m_peak_coordinates.push_back({x, y});
    }
    updateDirectionVector(Config::EAST_DIRECTION_DEGREES);
}

void SunriseEffect::updateDirectionVector(float direction_degrees) {
    float radians = direction_degrees * (Config::PI / 180.0f);
    m_sun_direction_x = std::cos(radians);
    m_sun_direction_y = -std::sin(radians); 
}

float SunriseEffect::clampf(float val, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, val));
}

SunriseEffect::ColorRGB SunriseEffect::getAlpenglowColor(float phase) const {
    phase = clampf(phase, 0.0f, 1.0f);

    const ColorRGB COLOR_OFF           = {  0.0f,   0.0f,   0.0f};
    const ColorRGB COLOR_DEEP_RED      = {220.0f,  15.0f,   0.0f}; 
    const ColorRGB COLOR_GOLDEN_RED    = {255.0f,  65.0f,   0.0f}; 
    const ColorRGB COLOR_GOLDEN_YELLOW = {255.0f, 150.0f,  10.0f}; 
    const ColorRGB COLOR_SOLAR_START   = {255.0f, 140.0f,  40.0f}; 

    if (phase <= 0.01f) {
        return COLOR_OFF;
    } 
    else if (phase < 0.25f) {
        float t = phase / 0.25f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_DEEP_RED.r * t,
            COLOR_DEEP_RED.g * t,
            COLOR_DEEP_RED.b * t
        };
    } 
    else if (phase < 0.50f) {
        float t = (phase - 0.25f) / 0.25f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_DEEP_RED.r + (COLOR_GOLDEN_RED.r - COLOR_DEEP_RED.r) * t,
            COLOR_DEEP_RED.g + (COLOR_GOLDEN_RED.g - COLOR_DEEP_RED.g) * t,
            COLOR_DEEP_RED.b + (COLOR_GOLDEN_RED.b - COLOR_DEEP_RED.b) * t
        };
    } 
    else if (phase < 0.80f) {
        float t = (phase - 0.50f) / 0.30f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_GOLDEN_RED.r + (COLOR_GOLDEN_YELLOW.r - COLOR_GOLDEN_RED.r) * t,
            COLOR_GOLDEN_RED.g + (COLOR_GOLDEN_YELLOW.g - COLOR_GOLDEN_RED.g) * t,
            COLOR_GOLDEN_RED.b + (COLOR_GOLDEN_YELLOW.b - COLOR_GOLDEN_RED.b) * t
        };
    }
    else {
        float t = (phase - 0.80f) / 0.20f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_GOLDEN_YELLOW.r + (COLOR_SOLAR_START.r - COLOR_GOLDEN_YELLOW.r) * t,
            COLOR_GOLDEN_YELLOW.g + (COLOR_SOLAR_START.g - COLOR_GOLDEN_YELLOW.g) * t,
            COLOR_GOLDEN_YELLOW.b + (COLOR_SOLAR_START.b - COLOR_GOLDEN_YELLOW.b) * t
        };
    }
}

void SunriseEffect::update(uint32_t delta_ms) {
    float total_duration_ms = Config::SUNRISE_DURATION_MINUTES * 60.0f * 1000.0f;
    m_progress += static_cast<float>(delta_ms) / total_duration_ms;
    if (m_progress > 1.0f) m_progress = 1.0f;
}

void SunriseEffect::render(uint8_t* buffer, size_t width, size_t height) {
    renderWithPhase(buffer, width, height, m_progress, Config::EAST_DIRECTION_DEGREES, 0.0f);
}

void SunriseEffect::renderWithPhase(uint8_t* buffer, size_t width, size_t height, float phase, float direction_degrees, float cloud_density) {
    if (!buffer || width == 0 || height == 0) return;

    size_t total_bytes = width * height * 3;
    if (m_previous_frame_buffer.size() != total_bytes) {
        m_previous_frame_buffer.resize(total_bytes, 0);
    }

    float smoothing_factor = 0.25f; 
    m_smoothed_progress += (phase - m_smoothed_progress) * smoothing_factor;

    if (m_smoothed_progress <= 0.01f && phase <= 0.01f) {
        std::fill(buffer, buffer + total_bytes, 0);
        std::fill(m_previous_frame_buffer.begin(), m_previous_frame_buffer.end(), 0);
        m_smoothed_progress = 0.0f;
        return;
    }

    updateDirectionVector(direction_degrees);

    float max_possible_dist = std::hypot(static_cast<float>(width), static_cast<float>(height));
    float current_wave_radius = m_smoothed_progress * max_possible_dist * 2.0f;

    float cloud_dimming = 1.0f - (clampf(cloud_density, 0.0f, 1.0f) * 0.30f);
    float global_brightness_scale = (0.20f + (m_smoothed_progress * 0.80f)) * cloud_dimming;

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

                float directional_dot = (dx * m_sun_direction_x + dy * m_sun_direction_y);
                float directional_penalty = (directional_dot < 0.0f) ? 0.2f * std::abs(directional_dot) : 1.2f * directional_dot;

                float effective_dist = euclidean_dist + directional_penalty;
                if (effective_dist < min_effective_dist) {
                    min_effective_dist = effective_dist;
                }
            }

            float pixel_phase = 0.0f;
            // Force all LEDs fully turned on when sunrise completion is reached
            if (m_smoothed_progress >= 0.999f) {
                pixel_phase = 1.0f;
            } else if (current_wave_radius > 0.001f) {
                float wave_delta = current_wave_radius - min_effective_dist;
                pixel_phase = clampf(wave_delta / (max_possible_dist * 0.5f), 0.0f, 1.0f);
                pixel_phase = pixel_phase * pixel_phase * (3.0f - 2.0f * pixel_phase);
            }

            ColorRGB rgb = getAlpenglowColor(pixel_phase);

            rgb.r *= global_brightness_scale;
            rgb.g *= global_brightness_scale;
            rgb.b *= global_brightness_scale;

            size_t pixel_index = (y * width + x) * 3;
            
            uint8_t target_r = static_cast<uint8_t>(clampf(rgb.r, 0.0f, 255.0f));
            uint8_t target_g = static_cast<uint8_t>(clampf(rgb.g, 0.0f, 255.0f));
            uint8_t target_b = static_cast<uint8_t>(clampf(rgb.b, 0.0f, 255.0f));

            uint8_t prev_r = m_previous_frame_buffer[pixel_index];
            uint8_t prev_g = m_previous_frame_buffer[pixel_index + 1];
            uint8_t prev_b = m_previous_frame_buffer[pixel_index + 2];

            uint8_t final_r = static_cast<uint8_t>(prev_r + (static_cast<float>(target_r - prev_r) * 0.5f));
            uint8_t final_g = static_cast<uint8_t>(prev_g + (static_cast<float>(target_g - prev_g) * 0.5f));
            uint8_t final_b = static_cast<uint8_t>(prev_b + (static_cast<float>(target_b - prev_b) * 0.5f));

            buffer[pixel_index]     = final_r;
            buffer[pixel_index + 1] = final_g;
            buffer[pixel_index + 2] = final_b;

            m_previous_frame_buffer[pixel_index]     = final_r;
            m_previous_frame_buffer[pixel_index + 1] = final_g;
            m_previous_frame_buffer[pixel_index + 2] = final_b;
        }
    }
}