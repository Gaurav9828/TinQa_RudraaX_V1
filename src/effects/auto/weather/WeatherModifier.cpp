#include "WeatherModifier.h"
#include <algorithm>

float WeatherModifier::getPseudoNoise(size_t x, size_t y) {
    uint32_t hash = static_cast<uint32_t>(x * 73856093u ^ y * 19349663u);
    hash = (hash ^ (hash >> 13)) * 1274126177u;
    return static_cast<float>(hash & 0x0FFF) / 4095.0f;
}

void WeatherModifier::applyCloudCover(
    uint8_t* buffer,
    size_t width,
    size_t height,
    const ActiveWeatherState& weather,
    bool is_daytime)
{
    if (!buffer || width == 0 || height == 0 || weather.cloud_density <= 0.04f) {
        return;
    }

    // Pure luminance dimming factor: Clouds reduce light intensity without altering hues (no color overlap/desaturation)
    // For normal days, max cloud dimming is gentle. For the rare storm days (>0.90 density), it drops dramatically.
    float storm_dimming_floor = (weather.cloud_density > 0.90f && is_daytime) ? 0.05f : 0.25f;
    float base_dimming = 1.0f - (weather.cloud_density * (is_daytime ? 0.65f : 0.85f));
    base_dimming = std::clamp(base_dimming, storm_dimming_floor, 1.0f);

    for (size_t y = 0; y < height; ++y) {
        const float altitude_factor = 1.0f - (static_cast<float>(y) / static_cast<float>(height)) * 0.2f;

        for (size_t x = 0; x < width; ++x) {
            const size_t idx = (y * width + x) * 3;

            // Spatial noise to create natural cloud shadow patches
            const float noise = getPseudoNoise(x, y);
            const float local_density_modifier = noise * 0.10f - 0.05f;
            
            // Final pixel-level multiplier preserving original RGB ratios (pure dimming)
            float pixel_dimming = std::clamp(base_dimming - (local_density_modifier * altitude_factor), 0.03f, 1.0f);

            buffer[idx + 0] = static_cast<uint8_t>(std::clamp(static_cast<float>(buffer[idx + 0]) * pixel_dimming, 0.0f, 255.0f));
            buffer[idx + 1] = static_cast<uint8_t>(std::clamp(static_cast<float>(buffer[idx + 1]) * pixel_dimming, 0.0f, 255.0f));
            buffer[idx + 2] = static_cast<uint8_t>(std::clamp(static_cast<float>(buffer[idx + 2]) * pixel_dimming, 0.0f, 255.0f));
        }
    }
}