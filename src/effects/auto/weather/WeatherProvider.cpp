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

// Helper to determine if a day of the year falls into a summer month (April - October)
bool WeatherProvider::isSummerMonth(uint16_t day_of_year) const {
    // Approximate day ranges for Apr 1 to Oct 31 (Non-leap year baseline)
    // Jan(31), Feb(28), Mar(31) -> Day 91 is April 1st. Oct 31 is roughly Day 304.
    return (day_of_year >= 91 && day_of_year <= 304);
}

void WeatherProvider::calculateSunriseSunsetWithColors(uint16_t day_of_year, double& out_sunrise, double& out_sunset, double& out_redness_duration_hours) const {
    const double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
    const double RAD_TO_DEG = 180.0 / 3.14159265358979323846;

    const double latitude = 30.73;  
    const double longitude = 79.07; 

    double gamma = 2.0 * 3.14159265358979323846 * (static_cast<double>(day_of_year) - 1.0) / 365.0;

    double cos_gamma = std::cos(gamma);
    double sin_gamma = std::sin(gamma);
    double cos_2gamma = std::cos(2.0 * gamma);
    double sin_2gamma = std::sin(2.0 * gamma);

    double eot = 229.18 * (0.000075 + 0.001868 * cos_gamma - 0.032077 * sin_gamma 
                 - 0.014615 * cos_2gamma - 0.040849 * sin_2gamma);

    double declination = 0.006918 - 0.399912 * cos_gamma + 0.070257 * sin_gamma
                       - 0.006758 * cos_2gamma + 0.000907 * sin_2gamma
                       - 0.002697 * std::cos(3.0 * gamma) + 0.001480 * std::sin(3.0 * gamma);

    double lat_rad = latitude * DEG_TO_RAD;
    double sin_lat = std::sin(lat_rad);
    double cos_lat = std::cos(lat_rad);
    double cos_dec = std::cos(declination);
    double sin_dec = std::sin(declination);

    double h0_actual = -0.833 * DEG_TO_RAD;
    double cos_H_actual = (std::sin(h0_actual) - sin_lat * sin_dec) / (cos_lat * cos_dec);
    cos_H_actual = std::clamp(cos_H_actual, -1.0, 1.0);
    double H_actual_hours = (std::acos(cos_H_actual) * RAD_TO_DEG) / 15.0;

    double longitude_offset_hours = (82.5 - longitude) * 4.0 / 60.0; 
    double solar_noon = 12.0 + longitude_offset_hours - (eot / 60.0);

    out_sunrise = solar_noon - H_actual_hours;
    out_sunset = solar_noon + H_actual_hours;

    // Fixed durations: Summer = 40 mins (0.6666h), Winter = 30 mins (0.5h)
    if (isSummerMonth(day_of_year)) {
        out_redness_duration_hours = 40.0 / 60.0;
    } else {
        out_redness_duration_hours = 30.0 / 60.0;
    }
}

ActiveWeatherState WeatherProvider::evaluateWeather(uint16_t day_of_year, double current_hour_24) const {
    DailyWeatherProfile base_profile = m_kedarnath_climate_Data.getHistoricalProfile(day_of_year);
    
    double sunrise_hour = 6.0;
    double sunset_hour = 18.0;
    double redness_duration_hours = 0.5; 
    
    calculateSunriseSunsetWithColors(day_of_year, sunrise_hour, sunset_hour, redness_duration_hours);

    float current_h_f = static_cast<float>(current_hour_24);
    float sunrise_f = static_cast<float>(sunrise_hour);
    float sunset_f = static_cast<float>(sunset_hour);
    float redness_f = static_cast<float>(redness_duration_hours);

    // --- TIMELINE INTERPOLATION CORE ---
    float day_weight = 0.0f;
    float sunrise_sunset_phase = 0.0f;
    float sunrise_sunset_weight = 0.0f;

    if (current_h_f >= sunrise_f && current_h_f <= (sunrise_f + redness_f)) {
        float normalized_time = (current_h_f - sunrise_f) / redness_f;
        sunrise_sunset_phase = normalized_time; 
        sunrise_sunset_weight = std::sin(normalized_time * 3.14159265f); 
        day_weight = smoothstep(0.0f, 1.0f, normalized_time);            
    } 
    else if (current_h_f >= sunset_f && current_h_f <= (sunset_f + redness_f)) {
        float normalized_time = (current_h_f - sunset_f) / redness_f;
        sunrise_sunset_phase = 1.0f - normalized_time; 
        sunrise_sunset_weight = std::sin(normalized_time * 3.14159265f); 
        day_weight = 1.0f - smoothstep(0.0f, 1.0f, normalized_time);     
    }
    else if (current_h_f > (sunrise_f + redness_f) && current_h_f < sunset_f) {
        day_weight = 1.0f;
    }
    else {
        day_weight = 0.0f;
    }

    // --- CALCULATE STATIC/SMOOTH SOLAR BRIGHTNESS TRANSITION ---
    // Increases from morning to noon, decreases from noon to evening smoothly.
    float base_sun_brightness = 0.0f;
    double solar_noon = (sunrise_hour + sunset_hour) / 2.0;
    
    if (current_h_f >= sunrise_f && current_h_f <= sunset_f) {
        // Normalized progress from 0 (sunrise) to 1 (sunset) passing through 0.5 (noon)
        float day_progress = (current_h_f - sunrise_f) / (sunset_f - sunrise_f);
        // Sine curve peaking at noon (1.0) and hitting 0 at sunrise/sunset edges, scaled to clean daylight ceiling
        base_sun_brightness = std::sin(day_progress * 3.14159265f) * 0.5f; 
        base_sun_brightness = std::clamp(base_sun_brightness, 0.05f, 0.5f);
    } else {
        base_sun_brightness = 0.0f; // Night
    }

    // Cloud density mappings
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

    float hourly_drift = std::sin(current_h_f * 0.2618f) * 0.04f; 
    float final_density = std::clamp(target_density + hourly_drift, 0.05f, 1.0f);

    if (base_profile.day_coverage == CloudCoverage::HEAVY_STORM && (current_hour_24 >= 12.0 && current_hour_24 <= 16.0)) {
        final_density = 0.95f;
    }

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

    uint8_t hour_slot = static_cast<uint8_t>(current_hour_24);
    float hourly_storm_roll = getPseudoRandom(day_of_year * 24 + hour_slot, 3);
    bool is_thunder = (final_density >= 0.90f) && (hourly_storm_roll > 0.20f);
    bool is_thunder_possible = is_thunder && (current_hour_24 >= 13.0 && current_hour_24 <= 18.0);

    return ActiveWeatherState{
        final_density,
        is_thunder_possible,
        moon_phase_factor,
        sunrise_hour,
        sunset_hour,
        redness_duration_hours,
        sunrise_sunset_phase,
        sunrise_sunset_weight,
        base_sun_brightness
    };
}