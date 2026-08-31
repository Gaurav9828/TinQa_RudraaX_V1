#pragma once

#include <cstddef>
#include <cstdint>
#include "WeatherProvider.h"

class WeatherModifier {
public:
    static void applyCloudCover(uint8_t* buffer, size_t width, size_t height, const ActiveWeatherState& weather, bool is_daytime);
};