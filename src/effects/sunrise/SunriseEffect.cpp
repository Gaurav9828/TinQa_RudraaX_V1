#include "SunriseEffect.h"

SunriseEffect::SunriseEffect() {
    init();
}

void SunriseEffect::init() {
    m_progress = 0.0f;
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

    if (phase < 0.20f) {
        float t = phase / 0.20f;
        return {
            COLOR_NIGHT.r + (COLOR_CRIMSON.r - COLOR_NIGHT.r) * t,
            COLOR_NIGHT.g + (COLOR_CRIMSON.g - COLOR_NIGHT.g) * t,
            COLOR_NIGHT.b + (COLOR_CRIMSON.b - COLOR_NIGHT.b) * t
        };
    } else if (phase < 0.50f) {
        float t = (phase - 0.20f) / 0.30f;
        return {
            COLOR_CRIMSON.r + (COLOR_GOLDEN.r - COLOR_CRIMSON.r) * t,
            COLOR_CRIMSON.g + (COLOR_GOLDEN.g - COLOR_CRIMSON.g) * t,
            COLOR_CRIMSON.b + (COLOR_GOLDEN.b - COLOR_CRIMSON.b) * t
        };
    } else if (phase < 0.75f) {
        float t = (phase - 0.50f) / 0.25f;
        return {
            COLOR_GOLDEN.r + (COLOR_SOFT_SUN.r - COLOR_GOLDEN.r) * t,
            COLOR_GOLDEN.g + (COLOR_SOFT_SUN.g - COLOR_GOLDEN.g) * t,
            COLOR_GOLDEN.b + (COLOR_SOFT_SUN.b - COLOR_GOLDEN.b) * t
        };
    } else {
        // Smoothly settle down into the dim morning daylight base (140, 140, 135)
        float t = (phase - 0.75f) / 0.25f;
        return {
            COLOR_SOFT_SUN.r + (COLOR_DAYLIGHT.r - COLOR_SOFT_SUN.r) * t,
            COLOR_SOFT_SUN.g + (COLOR_DAYLIGHT.g - COLOR_SOFT_SUN.g) * t,
            COLOR_SOFT_SUN.b + (COLOR_DAYLIGHT.b - COLOR_SOFT_SUN.b) * t
        };
    }
}

void SunriseEffect::update(uint32_t delta_ms) {
    float total_duration_ms = Config::SUNRISE_DURATION_MINUTES * 60.0f * 1000.0f;
    m_progress += static_cast<float>(delta_ms) / total_duration_ms;
    if (m_progress > 1.0f) m_progress = 1.0f;
}

void SunriseEffect::render(uint8_t* buffer, size_t width, size_t height) {
    renderWithPhase(buffer, width, height, m_progress, Config::EAST_DIRECTION_DEGREES);
}

void SunriseEffect::renderWithPhase(uint8_t* buffer, size_t width, size_t height, float phase, float direction_degrees) {
    if (!buffer || width == 0 || height == 0) return;

    updateDirectionVector(direction_degrees);

    float max_possible_dist = std::hypot(static_cast<float>(width), static_cast<float>(height));
    float current_wave_radius = phase * max_possible_dist * 1.4f;

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
            if (current_wave_radius > 0.001f) {
                float wave_delta = current_wave_radius - min_effective_dist;
                pixel_phase = clampf(wave_delta / (max_possible_dist * 0.4f), 0.0f, 1.0f);
            }

            ColorRGB rgb = getAlpenglowColor(pixel_phase);

            size_t pixel_index = (y * width + x) * 3;
            buffer[pixel_index]     = static_cast<uint8_t>(clampf(rgb.r, 0.0f, 255.0f));
            buffer[pixel_index + 1] = static_cast<uint8_t>(clampf(rgb.g, 0.0f, 255.0f));
            buffer[pixel_index + 2] = static_cast<uint8_t>(clampf(rgb.b, 0.0f, 255.0f));
        }
    }
}