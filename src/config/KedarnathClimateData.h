#pragma once

#include <cstdint>

enum class CloudCoverage : uint8_t {
    CLEAR = 0,
    MID_CLOUDY = 1,
    FULL_CLOUDY = 2
};

struct DailyWeatherProfile {
    CloudCoverage day_coverage;
    CloudCoverage night_coverage;
};

class KedarnathClimateData {
public:
    // Lookup daily historical weather profile for day of year (1 - 365)
    static DailyWeatherProfile getHistoricalProfile(uint16_t day_of_year) {
        if (day_of_year < 1) day_of_year = 1;
        if (day_of_year > 365) day_of_year = 365;

        // Jan - Apr (Days 1 - 120): High Clarity, Occasional Cold Fronts
        if (day_of_year <= 120) {
            if (day_of_year % 7 == 0) return {CloudCoverage::FULL_CLOUDY, CloudCoverage::FULL_CLOUDY};
            if (day_of_year % 4 == 0) return {CloudCoverage::MID_CLOUDY, CloudCoverage::MID_CLOUDY};
            return {CloudCoverage::CLEAR, CloudCoverage::CLEAR};
        }
        // May - June (Days 121 - 181): Pre-Monsoon Build-up
        else if (day_of_year <= 181) {
            if (day_of_year % 3 == 0) return {CloudCoverage::MID_CLOUDY, CloudCoverage::MID_CLOUDY};
            if (day_of_year % 5 == 0) return {CloudCoverage::FULL_CLOUDY, CloudCoverage::FULL_CLOUDY};
            return {CloudCoverage::CLEAR, CloudCoverage::CLEAR};
        }
        // July - Sept (Days 182 - 273): Heavy Monsoon Season (July 31st = Day 212)
        else if (day_of_year <= 273) {
            if (day_of_year % 10 == 0) return {CloudCoverage::CLEAR, CloudCoverage::CLEAR}; // Monsoon clear anomaly day
            if (day_of_year % 4 == 0)  return {CloudCoverage::MID_CLOUDY, CloudCoverage::MID_CLOUDY};
            return {CloudCoverage::FULL_CLOUDY, CloudCoverage::FULL_CLOUDY};
        }
        // Oct - Dec (Days 274 - 365): Autumn Post-Monsoon & Early Winter Clear Skies
        else {
            if (day_of_year % 12 == 0) return {CloudCoverage::FULL_CLOUDY, CloudCoverage::MID_CLOUDY};
            if (day_of_year % 5 == 0)  return {CloudCoverage::MID_CLOUDY, CloudCoverage::CLEAR};
            return {CloudCoverage::CLEAR, CloudCoverage::CLEAR};
        }
    }
};