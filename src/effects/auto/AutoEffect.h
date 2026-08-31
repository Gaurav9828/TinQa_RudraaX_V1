#pragma once

#include "../IEffect.h"
#include "../sunrise/SunriseEffect.h"
#include "../aurora/AuroraEffect.h"
#include "../thunder/ThunderEffect.h"
#include "weather/WeatherModifier.h"
#include "weather/WeatherModifier.h"
#include <vector>
#include <cstddef>
#include <cstdint>

class AutoEffect : public IEffect {
public:
    AutoEffect();
    ~AutoEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    
    // Pure Virtual Implementation from IEffect interface
    const char* getName() const override { return "Auto"; }

    // Time & Calendar Control
    void syncWithSystemTime();
    void setSimulatedTime(double total_seconds);
    void setDayOfYear(uint16_t day_of_year);
    uint16_t getDayOfYear() const { return m_day_of_year; }

private:
    void renderSkyBackground(uint8_t* buffer, size_t width, size_t height);
    void renderBaseStars(uint8_t* buffer, size_t width, size_t height);
    void renderMoonOnly(uint8_t* buffer, size_t width, size_t height);
    float calculateMoonPhaseFactor() const;
    
    // Standalone Effect Delegates
    SunriseEffect m_sunrise_delegate;
    AuroraEffect  m_aurora_delegate;
    ThunderEffect m_thunder_delegate;

    WeatherProvider m_weather_provider;

    // Simulation State
    double   m_simulated_seconds;
    uint16_t m_day_of_year;

    std::vector<uint8_t> m_effect_temp_buffer;
};