#pragma once

#include "config/KedarnathClimateData.h"
#include <cstdint>

#ifndef ACTIVE_WEATHER_STATE_H
#define ACTIVE_WEATHER_STATE_H

struct ActiveWeatherState {
    float cloud_density;
    bool is_thunder_possible;
    float moon_phase_factor;
    double sunrise_hour; // Dynamically calculated sunrise for the current day
    double sunset_hour;  // Dynamically calculated sunset for the current day
};
#endif // ACTIVE_WEATHER_STATE_H

class WeatherProvider {
public:
    WeatherProvider() = default;

    // Marked const for clean state querying
    ActiveWeatherState evaluateWeather(uint16_t day_of_year, double current_hour_24) const;

private:
    float getPseudoRandom(uint16_t day, uint8_t seed_offset) const;
    void calculateSunriseSunset(uint16_t day_of_year, double& out_sunrise, double& out_sunset) const;
};