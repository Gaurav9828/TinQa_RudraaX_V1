#include "SolarLightingLayer.h"
#include "../../../config/AppConfig.h"
#include <cmath>
#include <algorithm>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t); // Smooth hermite curve to prevent linear pops
    }
}

void SolarLightingLayer::render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) {
    if (!buffer || width == 0 || height == 0) return;

    float daylight = 0.0f;

    // Smoother 24-hour cycle boundaries to completely eliminate frame blinking
    if (ctx.current_hour >= 5.0 && ctx.current_hour < 7.0) {
        daylight = smoothstep(5.0f, 7.0f, static_cast<float>(ctx.current_hour));
    } else if (ctx.current_hour >= 7.0 && ctx.current_hour <= 17.5) {
        daylight = 1.0f;
    } else if (ctx.current_hour > 17.5 && ctx.current_hour <= 19.5) {
        daylight = 1.0f - smoothstep(17.5f, 19.5f, static_cast<float>(ctx.current_hour));
    }

    float solar_peak = 0.0f;
    if (ctx.current_hour >= 6.0 && ctx.current_hour <= 18.0) {
        solar_peak = std::sin(static_cast<float>(((ctx.current_hour - 6.0) / 12.0) * Config::PI));
        solar_peak = std::clamp(solar_peak, 0.0f, 1.0f);
    }

    const float cloud_day_factor = 1.0f - (ctx.weather.cloud_density * 0.40f);

    // Shifted from harsh white to a rich, warm golden-yellow daylight spectrum
    const float day_r = (245.0f + (255.0f - 245.0f) * solar_peak) * cloud_day_factor;
    const float day_g = (195.0f + (225.0f - 195.0f) * solar_peak) * cloud_day_factor;
    const float day_b = (90.0f +  (130.0f - 90.0f) * solar_peak) * cloud_day_factor;

    const uint8_t r = static_cast<uint8_t>(std::clamp(day_r * daylight, 0.0f, 255.0f));
    const uint8_t g = static_cast<uint8_t>(std::clamp(day_g * daylight, 0.0f, 255.0f));
    const uint8_t b = static_cast<uint8_t>(std::clamp(day_b * daylight, 0.0f, 255.0f));

    const size_t total_pixels = width * height;
    for (size_t i = 0; i < total_pixels; ++i) {
        buffer[i * 3 + 0] = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }
}