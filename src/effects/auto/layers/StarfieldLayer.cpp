#include "StarfieldLayer.h"
#include <cmath>
#include <algorithm>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }
}

StarfieldLayer::StarfieldLayer()
    : m_cached_width(0), m_cached_height(0) {}

void StarfieldLayer::init() {
    m_stars.clear();
    m_cached_width = 0;
    m_cached_height = 0;
}

void StarfieldLayer::generateFixedStars(size_t width, size_t height) {
    if (m_cached_width == width && m_cached_height == height && !m_stars.empty()) {
        return;
    }

    m_cached_width = width;
    m_cached_height = height;
    m_stars.clear();

    if (width == 0 || height == 0) {
        return;
    }

    const size_t MIN_STAR_COUNT = 3;
    const size_t MAX_STAR_COUNT = 10;

    uint32_t base_seed = static_cast<uint32_t>((width * 73856093u) ^ (height * 19349663u));
    const size_t star_count = MIN_STAR_COUNT + (base_seed % (MAX_STAR_COUNT - MIN_STAR_COUNT + 1));

    size_t half_height = std::max<size_t>(1, height / 2);

    for (size_t i = 0; i < star_count; ++i) {
        uint32_t seed = base_seed ^ static_cast<uint32_t>((i + 1) * 2654435761u);
        size_t x = static_cast<size_t>((seed * 1664525u + 1013904223u) % width);
        
        // Shifted to the opposite (lower/back) side of the panel instead of the top half
        size_t y_offset = static_cast<size_t>(((seed ^ 0x9E3779B9u) * 1664525u + 1013904223u) % half_height);
        size_t y = height - 1 - y_offset; // Inverted/opposite vertical placement

        FixedStar star{x, y, seed};
        bool duplicate = false;

        for (const auto& existing : m_stars) {
            if (existing.x == star.x && existing.y == star.y) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            m_stars.push_back(star);
        }
    }

    for (size_t i = 0; m_stars.size() < MIN_STAR_COUNT && m_stars.size() < MAX_STAR_COUNT; ++i) {
        uint32_t seed = base_seed ^ static_cast<uint32_t>((i + 37) * 1103515245u);
        size_t x = static_cast<size_t>(seed % width);
        size_t y_offset = static_cast<size_t>((seed >> 8) % half_height);
        size_t y = height - 1 - y_offset;

        FixedStar star{x, y, seed};

        bool duplicate = false;
        for (const auto& existing : m_stars) {
            if (existing.x == star.x && existing.y == star.y) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            m_stars.push_back(star);
        }
    }
}

void StarfieldLayer::render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) {
    if (!buffer || width == 0 || height == 0) return;

    float star_visibility = 0.0f;

    if (ctx.current_hour >= 19.0 && ctx.current_hour <= 20.5) {
        star_visibility = smoothstep(0.0f, 1.0f, static_cast<float>((ctx.current_hour - 19.0) / 1.5));
    } else if (ctx.current_hour > 20.5 || ctx.current_hour < 4.5) {
        star_visibility = 1.0f;
    } else if (ctx.current_hour >= 4.5 && ctx.current_hour < 6.0) {
        star_visibility = 1.0f - smoothstep(0.0f, 1.0f, static_cast<float>((ctx.current_hour - 4.5) / 1.5));
    }

    if (star_visibility <= 0.0f) return;

    star_visibility *= 1.0f - (ctx.weather.cloud_density * 0.45f);
    if (star_visibility <= 0.0f) return;

    generateFixedStars(width, height);

    const float STAR_BRIGHTNESS = 2.0f;

    for (const auto& star : m_stars) {
        if (star.x >= width || star.y >= height) continue;

        const uint8_t final_val = static_cast<uint8_t>(std::clamp(STAR_BRIGHTNESS * star_visibility, 1.0f, 2.0f));
        const size_t idx = (star.y * width + star.x) * 3;

        buffer[idx + 0] = std::min<uint16_t>(255, buffer[idx + 0] + final_val);
        buffer[idx + 1] = std::min<uint16_t>(255, buffer[idx + 1] + final_val);
        buffer[idx + 2] = std::min<uint16_t>(255, buffer[idx + 2] + final_val);
    }
}