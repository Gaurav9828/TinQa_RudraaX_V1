#include "CloudLayer.h"
#include <algorithm>
#include <cmath>

CloudLayer::CloudLayer() : m_scroll_x(0.0f), m_smoothed_cloud_intensity(0.0f) {}

void CloudLayer::init() {
    m_scroll_x = 0.0f;
    m_smoothed_cloud_intensity = 0.0f;
}

void CloudLayer::update(uint32_t delta_ms, bool is_hyperlapse) {
    // Scroll cloud chunks smoothly across the matrix
    float speed = is_hyperlapse ? 0.03f : 0.007f;
    m_scroll_x += static_cast<float>(delta_ms) * speed;
}

void CloudLayer::render(uint8_t* buffer, size_t width, size_t height, CloudDensityTier tier, bool is_daytime) {
    if (!buffer || width == 0 || height == 0) {
        return;
    }

    float target_intensity = 0.0f;
    uint8_t shadow_alpha = 255;
    float coverage_threshold = 0.70f; 

    switch (tier) {
        case CloudDensityTier::LEVEL_0_CLEAR:
            target_intensity = 0.0f;
            break;
        case CloudDensityTier::LEVEL_1_FEW:
            target_intensity = 1.0f;
            shadow_alpha = is_daytime ? 170 : 200;
            coverage_threshold = 0.80f;
            break;
        case CloudDensityTier::LEVEL_2_SCATTERED:
            target_intensity = 1.0f;
            shadow_alpha = is_daytime ? 130 : 160;
            coverage_threshold = 0.65f;
            break;
        case CloudDensityTier::LEVEL_3_BROKEN:
            target_intensity = 1.0f;
            shadow_alpha = is_daytime ? 90 : 120;
            coverage_threshold = 0.48f;
            break;
        case CloudDensityTier::LEVEL_4_MID_OVERCAST:
        case CloudDensityTier::LEVEL_5_HEAVY:
        case CloudDensityTier::LEVEL_6_VERY_HEAVY:
        case CloudDensityTier::LEVEL_7_THUNDER_STORM:
            target_intensity = 1.0f;
            shadow_alpha = is_daytime ? 45 : 15;
            coverage_threshold = 0.25f;
            break;
    }

    // Time-based smoothing (independent of frame rates, preventing hyperlapse pops)
    float smoothing_speed = 0.005f; // Adjust interpolation speed if needed
    float diff = target_intensity - m_smoothed_cloud_intensity;
    m_smoothed_cloud_intensity += diff * std::clamp(smoothing_speed * 16.0f, 0.0f, 1.0f);

    if (m_smoothed_cloud_intensity < 0.001f) {
        return;
    }

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t index = (y * width + x) * 3;

            float nx = (static_cast<float>(x) + m_scroll_x) * 0.18f;
            float ny = static_cast<float>(y) * 0.18f;

            float blob_1 = std::sin(nx) * std::cos(ny);
            float blob_2 = std::sin(nx * 0.5f + ny * 0.7f);
            float cloud_noise = (blob_1 * 0.65f + blob_2 * 0.35f + 1.0f) * 0.5f; 

            if (cloud_noise >= coverage_threshold) {
                float edge_distance = (cloud_noise - coverage_threshold) / (1.0f - coverage_threshold);
                float raw_factor = std::clamp(static_cast<float>(shadow_alpha) / 255.0f + (1.0f - edge_distance) * 0.2f, 0.05f, 1.0f);
                
                float factor = 1.0f - (m_smoothed_cloud_intensity * (1.0f - raw_factor));

                uint8_t r = buffer[index + 0];
                uint8_t g = buffer[index + 1];
                uint8_t b = buffer[index + 2];

                // Pure grayscale proportional dimming (Zero blue hue shift)
                buffer[index + 0] = static_cast<uint8_t>(r * factor);
                buffer[index + 1] = static_cast<uint8_t>(g * factor);
                buffer[index + 2] = static_cast<uint8_t>(b * factor);
            }
        }
    }
}