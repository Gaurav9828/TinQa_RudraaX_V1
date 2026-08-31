#pragma once

#include "config/KedarnathClimateData.h"
#include <random>

struct ActiveWeatherState {
    CloudCoverage current_coverage;
    float cloud_density;      // 0.0f (Clear) to 1.0f (Full Heavy Cloud)
    bool is_aurora_night;     // True for ~20 random clear winter nights
    bool is_thunder_possible; // True during high cloud density
};

class WeatherProvider {
public:
    WeatherProvider();
    
    // Evaluate weather state for a specific time and day of year
    ActiveWeatherState evaluateWeather(uint16_t day_of_year, double current_hour_24);

private:
    float getPseudoRandom(uint16_t day, uint8_t seed_offset) const;
};