#pragma once

#include "config/KedarnathClimateData.h"
#include <cstdint>

#ifndef ACTIVE_WEATHER_STATE_H
#define ACTIVE_WEATHER_STATE_H

struct ActiveWeatherState {
    float cloud_density;
    bool is_thunder_possible;
    float moon_phase_factor; // Added to pass illumination/phase factor to RenderContext
};
#endif // ACTIVE_WEATHER_STATE_H

class WeatherProvider {
public:
    WeatherProvider() = default;

    // Evaluates weather state with smooth day/night temporal transitions
    ActiveWeatherState evaluateWeather(uint16_t day_of_year, double current_hour_24);

private:
    float getPseudoRandom(uint16_t day, uint8_t seed_offset) const;
};