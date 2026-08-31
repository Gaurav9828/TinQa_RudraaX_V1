#include "WeatherProvider.h"
#include <cmath>

WeatherProvider::WeatherProvider() {}

float WeatherProvider::getPseudoRandom(uint16_t day, uint8_t seed_offset) const {
    uint32_t hash = day * 2654435761u + seed_offset * 1013904223u;
    hash = (hash ^ (hash >> 16)) * 2246822507u;
    return static_cast<float>(hash & 0xFFFF) / 65535.0f;
}

ActiveWeatherState WeatherProvider::evaluateWeather(uint16_t day_of_year, double current_hour_24) {
    DailyWeatherProfile base_profile = KedarnathClimateData::getHistoricalProfile(day_of_year);
    
    bool is_daytime = (current_hour_24 >= 6.0 && current_hour_24 <= 18.5);
    CloudCoverage coverage = is_daytime ? base_profile.day_coverage : base_profile.night_coverage;

    // 1. Inject Natural Random Anomalies
    float anomaly_roll = getPseudoRandom(day_of_year, 1);
    
    // Monsoon Anomaly: 12% chance of clear sunny day in heavy monsoon season
    if (day_of_year >= 182 && day_of_year <= 273 && anomaly_roll < 0.12f) {
        coverage = CloudCoverage::CLEAR;
    }
    // Winter Anomaly: 15% chance of overcast snow clouds in clear winter
    else if ((day_of_year < 90 || day_of_year > 300) && anomaly_roll > 0.85f) {
        coverage = CloudCoverage::FULL_CLOUDY;
    }

    // 2. Map Coverage to Continuous Cloud Density (0.0 to 1.0)
    float density = 0.0f;
    switch (coverage) {
        case CloudCoverage::CLEAR:       density = 0.05f; break;
        case CloudCoverage::MID_CLOUDY:  density = 0.45f; break;
        case CloudCoverage::FULL_CLOUDY: density = 0.88f; break;
    }

    // Add subtle continuous floating fluctuation
    float hourly_drift = std::sin(static_cast<float>(current_hour_24) * 0.5f) * 0.08f;
    density = std::max(0.0f, std::min(1.0f, density + hourly_drift));

    // 3. Determine Aurora Night (20 random clear winter nights)
    bool is_winter = (day_of_year <= 75 || day_of_year >= 310);
    float aurora_roll = getPseudoRandom(day_of_year, 2);
    // ~20 out of ~130 winter days = ~15% chance on clear winter nights
    bool is_aurora_night = is_winter && !is_daytime && (coverage == CloudCoverage::CLEAR) && (aurora_roll < 0.15f);

    bool is_thunder = (coverage == CloudCoverage::FULL_CLOUDY) && (getPseudoRandom(day_of_year, 3) > 0.4f);

    return ActiveWeatherState{
        coverage,
        density,
        is_aurora_night,
        is_thunder
    };
}