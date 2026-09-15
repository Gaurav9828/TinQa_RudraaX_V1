#include "CloudLayer.h"
#include <algorithm>
#include <cmath>

CloudLayer::CloudLayer() : m_scroll_x(0.0f) {}

void CloudLayer::init() {
    m_scroll_x = 0.0f;
}

void CloudLayer::update(uint32_t delta_ms, bool is_hyperlapse) {
    // Scroll cloud chunks smoothly across the matrix
    float speed = is_hyperlapse ? 0.03f : 0.007f;
    m_scroll_x += static_cast<float>(delta_ms) * speed;
}

void CloudLayer::render(uint8_t* buffer, size_t width, size_t height, CloudDensityTier tier, bool is_daytime) {
    if (!buffer || width == 0 || height == 0 || tier == CloudDensityTier::LEVEL_0_CLEAR) {
        return;
    }

    // Configure shadow dimming alpha and chunk coverage thresholds per tier
    uint8_t shadow_alpha = 255;
    float coverage_threshold = 0.70f; 

    switch (tier) {
        case CloudDensityTier::LEVEL_1_FEW:
            shadow_alpha = is_daytime ? 170 : 200;
            coverage_threshold = 0.80f; // Sparse, isolated cloud chunks
            break;
        case CloudDensityTier::LEVEL_2_SCATTERED:
            shadow_alpha = is_daytime ? 130 : 160;
            coverage_threshold = 0.65f; // Moderate scattered cloud patches
            break;
        case CloudDensityTier::LEVEL_3_BROKEN:
            shadow_alpha = is_daytime ? 90 : 120;
            coverage_threshold = 0.48f; // Larger interconnected cloud masses with gaps
            break;
        case CloudDensityTier::LEVEL_4_MID_OVERCAST:
        case CloudDensityTier::LEVEL_5_HEAVY:
        case CloudDensityTier::LEVEL_6_VERY_HEAVY:
        case CloudDensityTier::LEVEL_7_THUNDER_STORM:
            shadow_alpha = is_daytime ? 45 : 15;
            coverage_threshold = 0.25f; // Dense blanket coverage
            break;
        default:
            break;
    }

    // Render organic cloud chunks and shadows across the LED grid
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t index = (y * width + x) * 3;

            // Multi-frequency coordinate scaling to form distinct cloud blobs/islands
            float nx = (static_cast<float>(x) + m_scroll_x) * 0.18f;
            float ny = static_cast<float>(y) * 0.18f;

            // Blend sine and cosine waves to create organic, rounded cluster shapes
            float blob_1 = std::sin(nx) * std::cos(ny);
            float blob_2 = std::sin(nx * 0.5f + ny * 0.7f);
            float cloud_noise = (blob_1 * 0.65f + blob_2 * 0.35f + 1.0f) * 0.5f; // Normalized to [0, 1]

            // If the noise value exceeds the tier threshold, dim the LEDs to cast a cloud shadow chunk
            if (cloud_noise >= coverage_threshold) {
                // Calculate softness gradient near the edges of the cloud chunk
                float edge_distance = (cloud_noise - coverage_threshold) / (1.0f - coverage_threshold);
                float factor = std::clamp(static_cast<float>(shadow_alpha) / 255.0f + (1.0f - edge_distance) * 0.2f, 0.05f, 1.0f);

                buffer[index + 0] = static_cast<uint8_t>(buffer[index + 0] * factor);
                buffer[index + 1] = static_cast<uint8_t>(buffer[index + 1] * factor);
                buffer[index + 2] = static_cast<uint8_t>(buffer[index + 2] * factor);
            }
        }
    }
}