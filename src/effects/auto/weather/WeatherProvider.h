#pragma once

#include <cstdint>
#include "config/KedarnathClimateData.h"

struct ActiveWeatherState {
    float cloud_density;
    bool is_thunder_possible;
    float moon_phase_factor;
    double sunrise_hour;
    double sunset_hour;
    double redness_duration_hours;
    float sunrise_sunset_phase;
    float sunrise_sunset_weight;
    float base_sun_brightness;
};

class WeatherProvider {
public:
    WeatherProvider() = default;
    
    ActiveWeatherState evaluateWeather(uint16_t day_of_year, double current_hour_24) const;
    KedarnathClimateData m_kedarnath_climate_Data;

private:
    float getPseudoRandom(uint16_t day, uint8_t seed_offset) const;
    bool isSummerMonth(uint16_t day_of_year) const;
    void calculateSunriseSunsetWithColors(uint16_t day_of_year, double& out_sunrise, double& out_sunset, double& out_redness_duration_hours) const;
};