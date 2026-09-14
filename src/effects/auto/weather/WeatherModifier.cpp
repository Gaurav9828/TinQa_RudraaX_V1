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

    // Global factors
    const float base_dimming = 1.0f - (weather.cloud_density * (is_daytime ? 0.45f : 0.65f));
    const float desat_mix = weather.cloud_density * 0.40f;

    for (size_t y = 0; y < height; ++y) {
        // Upper pixels accumulate slightly denser clouds
        const float altitude_factor = 1.0f - (static_cast<float>(y) / static_cast<float>(height)) * 0.3f;

        for (size_t x = 0; x < width; ++x) {
            const size_t idx = (y * width + x) * 3;

            // Low-cost spatial variation per pixel to break uniform flat gray
            const float noise = getPseudoNoise(x, y);
            const float local_density = std::clamp(weather.cloud_density * altitude_factor + (noise * 0.12f - 0.06f), 0.0f, 1.0f);

            float r = static_cast<float>(buffer[idx + 0]);
            float g = static_cast<float>(buffer[idx + 1]);
            float b = static_cast<float>(buffer[idx + 2]);

            // Grayscale target
            const float gray_val = (r * 0.299f + g * 0.587f + b * 0.114f) * (is_daytime ? 0.82f : 0.35f);

            // Desaturate colors towards overcast storm tone
            r = r * (1.0f - desat_mix) + gray_val * desat_mix;
            g = g * (1.0f - desat_mix) + gray_val * desat_mix;
            b = b * (1.0f - desat_mix) + gray_val * desat_mix;

            // Apply dimming factor with spatial noise modification
            const float pixel_dimming = std::clamp(base_dimming - (local_density * 0.15f), 0.10f, 1.0f);

            buffer[idx + 0] = static_cast<uint8_t>(std::clamp(r * pixel_dimming, 0.0f, 255.0f));
            buffer[idx + 1] = static_cast<uint8_t>(std::clamp(g * pixel_dimming, 0.0f, 255.0f));
            buffer[idx + 2] = static_cast<uint8_t>(std::clamp(b * pixel_dimming, 0.0f, 255.0f));
        }
    }
}