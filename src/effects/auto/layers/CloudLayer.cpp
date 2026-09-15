#include "CloudLayer.h"
#include <algorithm>
#include <cmath>

CloudLayer::CloudLayer() : m_scroll_x(0.0f) {}

void CloudLayer::init() {
    m_scroll_x = 0.0f;
}

void CloudLayer::update(uint32_t delta_ms, bool is_hyperlapse) {
    // Scroll faster in hyperlapse mode for visible motion
    float speed = is_hyperlapse ? 0.02f : 0.005f;
    m_scroll_x += static_cast<float>(delta_ms) * speed;
}

void CloudLayer::render(uint8_t* buffer, size_t width, size_t height, CloudDensityTier tier, bool is_daytime) {
    if (!buffer || width == 0 || height == 0 || tier == CloudDensityTier::LEVEL_0_CLEAR) {
        return;
    }

    // Determine global dimming/blackout factor based on density tier and day/night state
    uint8_t global_alpha = 255;
    bool total_blackout_night = false;

    switch (tier) {
        case CloudDensityTier::LEVEL_1_FEW:
            global_alpha = is_daytime ? 230 : 240;
            break;
        case CloudDensityTier::LEVEL_2_SCATTERED:
            global_alpha = is_daytime ? 200 : 210;
            break;
        case CloudDensityTier::LEVEL_3_BROKEN:
            global_alpha = is_daytime ? 160 : 170;
            break;
        case CloudDensityTier::LEVEL_4_MID_OVERCAST:
            global_alpha = is_daytime ? 120 : 100;
            break;
        case CloudDensityTier::LEVEL_5_HEAVY:
            global_alpha = is_daytime ? 80 : 50;
            break;
        case CloudDensityTier::LEVEL_6_VERY_HEAVY:
            global_alpha = is_daytime ? 40 : 15;
            break;
        case CloudDensityTier::LEVEL_7_THUNDER_STORM:
            global_alpha = is_daytime ? 20 : 0;
            if (!is_daytime) total_blackout_night = true;
            break;
        default:
            break;
    }

    // If severe storm at night, completely black out background
    if (total_blackout_night) {
        std::fill(buffer, buffer + (width * height * 3), 0);
        return;
    }

    // Threshold mapping based on tier level
    uint8_t density_threshold = static_cast<uint8_t>(static_cast<int>(tier) * 32);

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t index = (y * width + x) * 3;

            // Lightweight procedural noise substitute using sine/cosine blending
            float sample_x = (static_cast<float>(x) + m_scroll_x) * 0.1f;
            float sample_y = static_cast<float>(y) * 0.1f;
            float wave = std::sin(sample_x) + std::cos(sample_y);
            // Map wave from [-2, 2] to [0, 255]
            uint8_t noise_val = static_cast<uint8_t>((wave + 2.0f) * 63.75f);

            // Apply cloud shadow mask / dimming on top of existing pixels
            if (noise_val > (255 - density_threshold)) {
                buffer[index]     = static_cast<uint8_t>((buffer[index]     * global_alpha) / 255);
                buffer[index + 1] = static_cast<uint8_t>((buffer[index + 1] * global_alpha) / 255);
                buffer[index + 2] = static_cast<uint8_t>((buffer[index + 2] * global_alpha) / 255);
            }
        }
    }
}