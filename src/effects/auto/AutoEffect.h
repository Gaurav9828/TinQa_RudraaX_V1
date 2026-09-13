#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "../IEffect.h"
#include "../sunrise/SunriseEffect.h"
#include "../sunset/SunsetEffect.h"
#include "../aurora/AuroraEffect.h"
#include "../thunder/ThunderEffect.h"
#include "weather/WeatherProvider.h"
#include "weather/WeatherModifier.h"

enum class AutoModeType {
    REAL_TIME,
    HYPERLAPSE
};

struct FixedStar {
    size_t x;
    size_t y;
    uint32_t seed;
};

class AutoEffect : public IEffect {
public:
    AutoEffect();
    ~AutoEffect() override = default;

    // IEffect Interface Implementation
    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Auto Climate Mode"; }

    // AutoEffect Specific APIs
    void setMode(AutoModeType mode);
    AutoModeType getMode() const { return m_mode; }
    void toggleHyperlapse();
    bool isHyperlapse() const { return m_mode == AutoModeType::HYPERLAPSE; }

    void syncWithSystemTime();
    void setSimulatedTime(double total_seconds);
    double getSimulatedTime() const { return m_simulated_seconds; }

    void setDayOfYear(uint16_t day_of_year);
    uint16_t getDayOfYear() const { return m_day_of_year; }

    float calculateMoonPhaseFactor() const;

private:
    void updateTimeProgress(uint32_t delta_ms);
    void generateFixedStars(size_t width, size_t height);
    void renderMountainLighting(uint8_t* buffer, size_t width, size_t height, const ActiveWeatherState& weather);
    void renderStaticStars(uint8_t* buffer, size_t width, size_t height, float cloud_density);
    void renderCornerMoon(uint8_t* buffer, size_t width, size_t height, float cloud_density);

    AutoModeType m_mode;
    double m_simulated_seconds; // 0.0 to 86400.0
    uint16_t m_day_of_year;     // 1 to 365

    // Fixed 3-10 Stars Cache
    std::vector<FixedStar> m_stars;
    size_t m_cached_width;
    size_t m_cached_height;

    // Weather Data Provider
    WeatherProvider m_weather_provider;

    // Delegated Specialized Effects
    SunriseEffect m_sunrise_effect;
    SunsetEffect  m_sunset_effect;
    AuroraEffect  m_aurora_effect;
    ThunderEffect m_thunder_effect;

    // Off-screen scratch buffer for multi-layer compositing
    std::vector<uint8_t> m_effect_temp_buffer;
};