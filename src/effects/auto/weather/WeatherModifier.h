#pragma once

#include <cstddef>
#include <cstdint>
#include "WeatherProvider.h"

class WeatherModifier {
public:
    // Applies cloud mask, desaturation, and directional skylight dynamic shading
    static void applyCloudCover(
        uint8_t* buffer,
        size_t width,
        size_t height,
        const ActiveWeatherState& weather,
        bool is_daytime
    );

private:
    static float getPseudoNoise(size_t x, size_t y);
};