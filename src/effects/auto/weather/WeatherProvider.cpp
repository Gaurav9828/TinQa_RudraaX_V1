#include "WeatherProvider.h"
#include "config/AppConfig.h"
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

// Precise astronomical calculation of sunrise and sunset for Kedarnath
void WeatherProvider::calculateSunriseSunset(uint16_t day_of_year, double& out_sunrise, double& out_sunset) const {
    const double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
    const double RAD_TO_DEG = 180.0 / 3.14159265358979323846;

    const double latitude = 30.73;  
    const double longitude = 79.07; 

    // Solar Declination (delta) approximation
    double declination = 23.45 * std::sin((360.0 / 365.0) * (static_cast<double>(day_of_year) - 80.0) * DEG_TO_RAD);

    // Hour Angle (H) calculation using the Sunrise Equation
    double h0 = -0.833; // Accounts for atmospheric refraction and solar radius
    double lat_rad = latitude * DEG_TO_RAD;
    double dec_rad = declination * DEG_TO_RAD;
    double h0_rad = h0 * DEG_TO_RAD;

    double cos_H = (std::sin(h0_rad) - std::sin(lat_rad) * std::sin(dec_rad)) / (std::cos(lat_rad) * std::cos(dec_rad));
    cos_H = std::clamp(cos_H, -1.0, 1.0);
    double H = std::acos(cos_H) * RAD_TO_DEG;

    // Solar Noon correction based on longitude offset from IST meridian (82.5° E)
    double longitude_correction_hours = (82.5 - longitude) * 4.0 / 60.0;
    double solar_noon = 12.0 - longitude_correction_hours;

    double hours_from_noon = H / 15.0;
    out_sunrise = solar_noon - hours_from_noon;
    out_sunset = solar_noon + hours_from_noon;
}

ActiveWeatherState WeatherProvider::evaluateWeather(uint16_t day_of_year, double current_hour_24) const {
    DailyWeatherProfile base_profile = KedarnathClimateData::getHistoricalProfile(day_of_year);
    
    double sunrise_hour = 6.0;
    double sunset_hour = 18.0;
    calculateSunriseSunset(day_of_year, sunrise_hour, sunset_hour);

    // 1. Smooth Day vs Night Profile Interpolation using Dynamic Horizon Times
    float day_weight = 0.0f;
    const float twilight_offset = 1.0f;

    if (current_hour_24 >= (sunrise_hour + twilight_offset) && current_hour_24 <= (sunset_hour - twilight_offset)) {
        day_weight = 1.0f;
    } else if (current_hour_24 > sunrise_hour && current_hour_24 < (sunrise_hour + twilight_offset)) {
        day_weight = smoothstep(static_cast<float>(sunrise_hour), static_cast<float>(sunrise_hour + twilight_offset), static_cast<float>(current_hour_24));
    } else if (current_hour_24 > (sunset_hour - twilight_offset) && current_hour_24 < sunset_hour) {
        day_weight = 1.0f - smoothstep(static_cast<float>(sunset_hour - twilight_offset), static_cast<float>(sunset_hour), static_cast<float>(current_hour_24));
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

    float hourly_drift = std::sin(static_cast<float>(current_hour_24) * 0.2618f) * 0.04f; 
    float final_density = std::clamp(target_density + hourly_drift, 0.05f, 1.0f);

    if (base_profile.day_coverage == CloudCoverage::HEAVY_STORM && (current_hour_24 >= 12.0 && current_hour_24 <= 16.0)) {
        final_density = 0.95f;
    }

    // 2. Astronomical Lunar Phase Calculation
    uint16_t current_year = 2026; 
    long total_days_to_year_start = 0;
    for (uint16_t y = 2000; y < current_year; ++y) {
        bool is_leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        total_days_to_year_start += is_leap ? 366 : 365;
    }

    double fractional_day = static_cast<double>(day_of_year - 1) + (current_hour_24 / 24.0);
    double days_since_ref = static_cast<double>(total_days_to_year_start) - 5.0 + fractional_day;

    const double synodic_month = 29.53059;
    double cycle_angle_deg = (days_since_ref / synodic_month) * 360.0;
    
    double angle_mod = std::fmod(cycle_angle_deg, 360.0);
    if (angle_mod < 0.0) angle_mod += 360.0;

    double angle_rad = angle_mod * (3.14159265358979323846 / 180.0);
    float moon_phase_factor = static_cast<float>((1.0 - std::cos(angle_rad)) / 2.0);

    // 3. Thunder Logic
    uint8_t hour_slot = static_cast<uint8_t>(current_hour_24);
    float hourly_storm_roll = getPseudoRandom(day_of_year * 24 + hour_slot, 3);
    bool is_thunder = (final_density >= 0.90f) && (hourly_storm_roll > 0.20f);
    bool is_thunder_possible = is_thunder && (current_hour_24 >= 13.0 && current_hour_24 <= 18.0);

    return ActiveWeatherState{
        final_density,
        is_thunder_possible,
        moon_phase_factor,
        sunrise_hour,
        sunset_hour
    };
}