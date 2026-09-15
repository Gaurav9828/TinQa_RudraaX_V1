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
            case CloudCoverage::FEW:         return 0.15f;
            case CloudCoverage::SCATTERED:   return 0.30f;
            case CloudCoverage::BROKEN:      return 0.50f;
            case CloudCoverage::OVERCAST:    return 0.70f;
            case CloudCoverage::HEAVY_STORM: return 0.95f;
        }
        return 0.05f;
    };

    float day_density = mapCoverageToDensity(base_profile.day_coverage);
    float night_density = mapCoverageToDensity(base_profile.night_coverage);
    float target_density = day_density * day_weight + night_density * (1.0f - day_weight);

    // Mild, natural hourly variation for normal days (keeps hyperlapse mostly bright and clear)
    float hourly_drift = std::sin(static_cast<float>(current_hour_24) * 0.2618f) * 0.04f; 
    float final_density = std::clamp(target_density + hourly_drift, 0.05f, 1.0f);

    // Strict isolation: Only allow the rare daytime storm darkness spike on actual HEAVY_STORM profile days (~4-5 days a year)
    if (base_profile.day_coverage == CloudCoverage::HEAVY_STORM && (current_hour_24 >= 12.0 && current_hour_24 <= 16.0)) {
        final_density = 0.95f; // Pitch-black storm condition for these rare days only
    }

    // 2. Astronomical Lunar Phase Calculation
    const float synodic_month = 29.53059f;
    float fractional_day = static_cast<float>(day_of_year) + static_cast<float>(current_hour_24 / 24.0);
    float days_since_ref = fractional_day - 254.0f;
    float phase = std::fmod(days_since_ref, synodic_month);
    if (phase < 0.0f) phase += synodic_month;
    float cycle_fraction = phase / synodic_month;
    float moon_phase_factor = (1.0f - std::cos(cycle_fraction * 2.0f * 3.14159265f)) * 0.5f;

    // 3. Thunder Logic
    uint8_t hour_slot = static_cast<uint8_t>(current_hour_24);
    float hourly_storm_roll = getPseudoRandom(day_of_year * 24 + hour_slot, 3);
    bool is_thunder = (final_density >= 0.90f) && (hourly_storm_roll > 0.20f);
    bool is_thunder_possible = is_thunder && (current_hour_24 >= 13.0 && current_hour_24 <= 18.0);

    return ActiveWeatherState{
        final_density,
        is_thunder_possible,
        moon_phase_factor
    };
}