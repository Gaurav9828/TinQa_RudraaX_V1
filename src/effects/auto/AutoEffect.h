#ifndef AUTO_EFFECT_H
#define AUTO_EFFECT_H

#include <cstdint>
#include <vector>
#include "effects/IEffect.h"
#include "effects/sunrise/SunriseEffect.h"
#include "effects/sunset/SunsetEffect.h"
#include "effects/aurora/AuroraEffect.h"
#include "effects/thunder/ThunderEffect.h"
#include "layers/SolarLightingLayer.h"
#include "layers/LunarLightingLayer.h"
#include "layers/StarfieldLayer.h"
#include "layers/MeteorShowerLayer.h"
#include "effects/auto/weather/WeatherProvider.h"
#include "effects/auto/weather/WeatherModifier.h"
#include "utils/WatchdogManager.h"

enum class AutoModeType {
    REAL_TIME,
    HYPERLAPSE
};

class AutoEffect : public IEffect {
public:
    AutoEffect();
    ~AutoEffect() = default;

    void init() override;
    void setMode(AutoModeType mode);
    void toggleHyperlapse();
    bool isHyperlapse() const { return m_mode == AutoModeType::HYPERLAPSE; }

    void setSimulatedTime(double total_seconds);
    void setDayOfYear(uint16_t day_of_year);

    void update(uint32_t delta_ms) override;
    void updateWithMasterTime(uint32_t delta_ms, double simulated_seconds, uint16_t day_of_year);
    void render(uint8_t* buffer, size_t width, size_t height) override;

    const char* getName() const override { return "AutoEffect"; }

    double getSimulatedTime() const { return m_simulated_seconds; }
    uint16_t getDayOfYear() const { return m_day_of_year; }

private:
    float calculateMoonPhaseFactor() const;

    AutoModeType m_mode;
    double m_simulated_seconds;
    double m_hyperlapse_seconds;
    double m_saved_real_time_seconds;
    uint16_t m_day_of_year;
    uint32_t m_watchdog_save_timer_ms;

    WeatherProvider m_weather_provider;

    SunriseEffect m_sunrise_effect;
    SunsetEffect m_sunset_effect;
    AuroraEffect m_aurora_effect;
    ThunderEffect m_thunder_effect;

    SolarLightingLayer m_solar_layer;
    LunarLightingLayer m_lunar_layer;
    StarfieldLayer m_starfield_layer;
    MeteorShowerLayer m_meteor_layer;

    std::vector<uint8_t> m_effect_temp_buffer;
};

#endif