#include "WeatherProvider.h"
#include <cmath>
#include <algorithm>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

float WeatherProvider::getPseudoRandom(uint16_t day, uint8_t seed_offset) const {
    uint32_t hash = day * 2654435761u + seed_offset * 1013904223u;
    hash = (hash ^ (hash >> 16)) * 2246822507u;
    return static_cast<float>(hash & 0xFFFF) / 65535.0f;
}

ActiveWeatherState WeatherProvider::evaluateWeather(uint16_t day_of_year, double current_hour_24) {
    DailyWeatherProfile base_profile = KedarnathClimateData::getHistoricalProfile(day_of_year);
    
    // 1. Smooth Day vs Night Profile Interpolation
    float day_weight = 0.0f;
    if (current_hour_24 >= 6.0 && current_hour_24 <= 18.0) {
        day_weight = 1.0f;
    } else if (current_hour_24 > 5.0 && current_hour_24 < 6.0) {
        day_weight = smoothstep(5.0f, 6.0f, static_cast<float>(current_hour_24));
    } else if (current_hour_24 > 18.0 && current_hour_24 < 19.0) {
        day_weight = 1.0f - smoothstep(18.0f, 19.0f, static_cast<float>(current_hour_24));
    }

    auto mapCoverageToDensity = [](CloudCoverage cov) -> float {
        switch (cov) {
            case CloudCoverage::CLEAR:       return 0.05f;
            case CloudCoverage::MID_CLOUDY:  return 0.45f;
            case CloudCoverage::FULL_CLOUDY: return 0.88f;
        }
        return 0.05f;
    };

    float day_density = mapCoverageToDensity(base_profile.day_coverage);
    float night_density = mapCoverageToDensity(base_profile.night_coverage);
    float target_density = day_density * day_weight + night_density * (1.0f - day_weight);

    // 2. Natural Anomaly Rolls
    float anomaly_roll = getPseudoRandom(day_of_year, 1);
    CloudCoverage effective_coverage = (day_weight > 0.5f) ? base_profile.day_coverage : base_profile.night_coverage;

    if (day_of_year >= 182 && day_of_year <= 273 && anomaly_roll < 0.12f) {
        effective_coverage = CloudCoverage::CLEAR;
        target_density = 0.05f;
    } else if ((day_of_year < 90 || day_of_year > 300) && anomaly_roll > 0.85f) {
        effective_coverage = CloudCoverage::FULL_CLOUDY;
        target_density = 0.88f;
    }

    // Continuous floating drift
    float hourly_drift = std::sin(static_cast<float>(current_hour_24) * 0.523598f) * 0.06f; 
    float final_density = std::clamp(target_density + hourly_drift, 0.0f, 1.0f);

    // 3. Precise Astronomical Lunar Phase Calculation (Dependent on Date and Hours)
    // Reference New Moon for 2026: Day 254 (September 11, 2026)
    const float synodic_month = 29.53059f;
    float fractional_day = static_cast<float>(day_of_year) + static_cast<float>(current_hour_24 / 24.0);
    float days_since_ref = fractional_day - 254.0f;
    float phase = std::fmod(days_since_ref, synodic_month);
    if (phase < 0.0f) phase += synodic_month;
    float cycle_fraction = phase / synodic_month;
    
    // Illumination factor: 0.0 (New Moon / No Moon) to 1.0 (Full Moon)
    float moon_phase_factor = (1.0f - std::cos(cycle_fraction * 2.0f * 3.14159265f)) * 0.5f;

    // 4. Special Features & Thunder Logic
    bool is_thunder = (final_density > 0.60f) && (getPseudoRandom(day_of_year, 3) > 0.40f);
    bool is_thunder_possible = is_thunder && (current_hour_24 >= 14.0 && current_hour_24 <= 18.0);

    return ActiveWeatherState{
        final_density,
        is_thunder_possible,
        moon_phase_factor
    };
}