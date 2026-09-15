#pragma once

#include <cstdint>

enum class CloudCoverage : uint8_t {
    CLEAR = 0,
    FEW = 1,
    SCATTERED = 2,
    BROKEN = 3,
    OVERCAST = 4,
    HEAVY_STORM = 5
};

struct DailyWeatherProfile {
    CloudCoverage day_coverage;
    CloudCoverage night_coverage;
};

class KedarnathClimateData {
public:
    static DailyWeatherProfile getHistoricalProfile(uint16_t day_of_year) {
        if (day_of_year < 1) day_of_year = 1;
        if (day_of_year > 365) day_of_year = 365;

        // Introduce rare severe storm days (~3-5 days per year)
        if (day_of_year == 55 || day_of_year == 140 || day_of_year == 215 || day_of_year == 260) {
            return {CloudCoverage::HEAVY_STORM, CloudCoverage::HEAVY_STORM};
        }

        // Jan - Apr (Days 1 - 120): High Clarity, Occasional Cold Fronts
        if (day_of_year <= 120) {
            if (day_of_year % 7 == 0) return {CloudCoverage::OVERCAST, CloudCoverage::OVERCAST};
            if (day_of_year % 4 == 0) return {CloudCoverage::SCATTERED, CloudCoverage::BROKEN};
            return {CloudCoverage::CLEAR, CloudCoverage::FEW};
        }
        // May - June (Days 121 - 181): Pre-Monsoon Build-up
        else if (day_of_year <= 181) {
            if (day_of_year % 3 == 0) return {CloudCoverage::BROKEN, CloudCoverage::OVERCAST};
            if (day_of_year % 5 == 0) return {CloudCoverage::SCATTERED, CloudCoverage::SCATTERED};
            return {CloudCoverage::CLEAR, CloudCoverage::CLEAR};
        }
        // July - Sept (Days 182 - 273): Heavy Monsoon Season
        else if (day_of_year <= 273) {
            if (day_of_year % 10 == 0) return {CloudCoverage::CLEAR, CloudCoverage::SCATTERED};
            if (day_of_year % 4 == 0)  return {CloudCoverage::BROKEN, CloudCoverage::OVERCAST};
            return {CloudCoverage::HEAVY_STORM, CloudCoverage::OVERCAST};
        }
        // Oct - Dec (Days 274 - 365): Autumn Post-Monsoon & Early Winter
        else {
            if (day_of_year % 12 == 0) return {CloudCoverage::OVERCAST, CloudCoverage::BROKEN};
            if (day_of_year % 5 == 0)  return {CloudCoverage::SCATTERED, CloudCoverage::CLEAR};
            return {CloudCoverage::CLEAR, CloudCoverage::FEW};
        }
    }
};