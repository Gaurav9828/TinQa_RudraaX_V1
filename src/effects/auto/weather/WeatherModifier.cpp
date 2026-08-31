#include "WeatherModifier.h"
#include <algorithm>

void WeatherModifier::applyCloudCover(uint8_t* buffer, size_t width, size_t height, const ActiveWeatherState& weather, bool is_daytime) {
    if (!buffer || weather.cloud_density <= 0.05f) return;

    // Cloud attenuation factor: Higher density reduces direct sunlight/stars/aurora intensity
    float dimming_factor = 1.0f - (weather.cloud_density * 0.55f); 
    
    // Desaturation factor: Overcast skies diffuse color into cold cool grays
    float gray_mix = weather.cloud_density * 0.35f;

    size_t total_pixels = width * height;
    for (size_t i = 0; i < total_pixels; ++i) {
        size_t idx = i * 3;
        float r = buffer[idx];
        float g = buffer[idx + 1];
        float b = buffer[idx + 2];

        // Compute pixel brightness / grayscale value
        float gray = (r * 0.299f + g * 0.587f + b * 0.114f) * (is_daytime ? 0.85f : 0.40f);

        // Blend raw color with cloud-scattered gray tone
        r = r * (1.0f - gray_mix) + gray * gray_mix;
        g = g * (1.0f - gray_mix) + gray * gray_mix;
        b = b * (1.0f - gray_mix) + gray * gray_mix;

        // Apply overcast dimming
        buffer[idx]     = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r * dimming_factor)));
        buffer[idx + 1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g * dimming_factor)));
        buffer[idx + 2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b * dimming_factor)));
    }
}