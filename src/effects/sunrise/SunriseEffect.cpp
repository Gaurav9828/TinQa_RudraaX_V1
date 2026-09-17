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

SunriseEffect::ColorRGB SunriseEffect::getSunriseColor(float phase) const {
    phase = clampf(phase, 0.0f, 1.0f);

    const ColorRGB COLOR_DEEP_RED      = {220.0f,  15.0f,   0.0f}; 
    const ColorRGB COLOR_GOLDEN_RED    = {255.0f,  65.0f,   0.0f}; 
    const ColorRGB COLOR_GOLDEN_YELLOW = {255.0f, 150.0f,  10.0f}; 
    const ColorRGB COLOR_SOLAR_START   = {255.0f, 140.0f,  40.0f}; 

    if (phase < 0.25f) {
        float t = phase / 0.25f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_DEEP_RED.r + (COLOR_GOLDEN_RED.r - COLOR_DEEP_RED.r) * t,
            COLOR_DEEP_RED.g + (COLOR_GOLDEN_RED.g - COLOR_DEEP_RED.g) * t,
            COLOR_DEEP_RED.b + (COLOR_GOLDEN_RED.b - COLOR_DEEP_RED.b) * t
        };
    } 
    else if (phase < 0.65f) {
        float t = (phase - 0.25f) / 0.40f;
        t = t * t * (3.0f - 2.0f * t);
        return {
            COLOR_GOLDEN_RED.r + (COLOR_GOLDEN_YELLOW.r - COLOR_GOLDEN_RED.r) * t,
            COLOR_GOLDEN_RED.g + (COLOR_GOLDEN_YELLOW.g - COLOR_GOLDEN_RED.g) * t,
            COLOR_GOLDEN_RED.b + (COLOR_GOLDEN_YELLOW.b - COLOR_GOLDEN_RED.b) * t
        };
    }
    else {
        float t = (phase - 0.65f) / 0.35f;
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

    float smoothing_factor = 0.5f; 
    m_smoothed_progress += (phase - m_smoothed_progress) * smoothing_factor;

    if (m_smoothed_progress <= 0.0001f && phase <= 0.0001f) {
        std::fill(buffer, buffer + total_bytes, 0);
        std::fill(m_previous_frame_buffer.begin(), m_previous_frame_buffer.end(), 0);
        m_smoothed_progress = 0.0f;
        return;
    }

    updateDirectionVector(direction_degrees);

    float max_possible_dist = std::hypot(static_cast<float>(width), static_cast<float>(height));

    const ColorRGB COLOR_VIOLET_INDIGO = {35.0f, 22.0f, 55.0f}; // Dimmed soft shade

    // Sunrise wave radius calculations (starts expanding once progress > 0.2727)
    float sunrise_wave_progress = (m_smoothed_progress > 0.2727f) ? (m_smoothed_progress - 0.2727f) / (1.0f - 0.2727f) : 0.0f;
    float current_wave_radius = sunrise_wave_progress * max_possible_dist * 2.0f;

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

            ColorRGB rgb = {0.0f, 0.0f, 0.0f};

            // 1. Check if the sunrise red wave has reached this pixel
            if (m_smoothed_progress > 0.2727f && current_wave_radius > min_effective_dist) {
                float wave_delta = current_wave_radius - min_effective_dist;
                float pixel_sunrise_phase = clampf(wave_delta / (max_possible_dist * 0.4f), 0.0f, 1.0f);
                pixel_sunrise_phase = pixel_sunrise_phase * pixel_sunrise_phase * (3.0f - 2.0f * pixel_sunrise_phase);
                
                // Sunrise red/gold overrides the background
                rgb = getSunriseColor(pixel_sunrise_phase);
            } 
            else {
                // 2. Sparse Alpenglow Background (~3% of pixels distributed evenly, approx 30 LEDs on a 32x32 matrix)
                uint32_t pixel_hash = (static_cast<uint32_t>(x) * 73856093u) ^ (static_cast<uint32_t>(y) * 19349663u);
                if ((pixel_hash % 100u) < 3u) {
                    // Ramp intensity from 0 up to minimal brightness during the 15-min alpenglow window (0 to 0.2727),
                    // then hold steady at max dimness until the sunrise wave overrides it.
                    float ramp_progress = clampf(m_smoothed_progress / 0.2727f, 0.0f, 1.0f);
                    ramp_progress = ramp_progress * ramp_progress * (3.0f - 2.0f * ramp_progress);

                    rgb = {
                        COLOR_VIOLET_INDIGO.r * ramp_progress,
                        COLOR_VIOLET_INDIGO.g * ramp_progress,
                        COLOR_VIOLET_INDIGO.b * ramp_progress
                    };
                }
            }

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