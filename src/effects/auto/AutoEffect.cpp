#include "AutoEffect.h"
#include "../../config/AppConfig.h"
#include "pico/stdlib.h"
#include <cmath>
#include <algorithm>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    inline float smoothBlend(float a, float b, float amount) {
        amount = smoothstep(0.0f, 1.0f, amount);
        return a + (b - a) * amount;
    }
}

AutoEffect::AutoEffect()
    : m_mode(AutoModeType::REAL_TIME),
      m_simulated_seconds(0.0),
      m_hyperlapse_seconds(5.0 * 3600.0),
      m_saved_real_time_seconds(0.0),
      m_day_of_year(1),
      m_watchdog_save_timer_ms(0),
      m_time_initialized(false)
{
    double recovered_seconds = 0.0;
    uint16_t recovered_day = 1;

    if (WatchdogManager::getInstance().getRecoveryTime(recovered_seconds, recovered_day)) {
        if (!std::isnan(recovered_seconds) && !std::isinf(recovered_seconds) &&
            recovered_seconds >= 0.0 && recovered_seconds < Config::SECONDS_IN_DAY &&
            recovered_day >= 1 && recovered_day <= 365) {
            m_simulated_seconds = recovered_seconds;
            m_saved_real_time_seconds = recovered_seconds;
            m_day_of_year = recovered_day;
            m_time_initialized = true;
        } else {
            m_simulated_seconds = 0.0;
            m_saved_real_time_seconds = 0.0;
            m_day_of_year = 1;
            m_time_initialized = false;
        }
    } else {
        m_simulated_seconds = 0.0;
        m_saved_real_time_seconds = 0.0;
        m_day_of_year = 1;
        m_time_initialized = false;
    }
    init();
}

void AutoEffect::init() {
    m_sunrise_effect.init();
    m_sunset_effect.init();
    m_aurora_effect.init();
    m_thunder_effect.init();

    m_solar_layer.init();
    m_lunar_layer.init();
    m_meteor_layer.init();
    m_cloud_layer.init();

    m_effect_temp_buffer.clear();
}

void AutoEffect::setMode(AutoModeType mode) {
    if (m_mode != AutoModeType::HYPERLAPSE && mode == AutoModeType::HYPERLAPSE) {
        m_saved_real_time_seconds = m_simulated_seconds;
        m_hyperlapse_seconds = 5.0 * 3600.0; 
        m_effect_temp_buffer.clear();
    } 
    else if (m_mode == AutoModeType::HYPERLAPSE && mode == AutoModeType::REAL_TIME) {
        m_simulated_seconds = m_saved_real_time_seconds;
        m_effect_temp_buffer.clear();
    }
    m_mode = mode;
}

void AutoEffect::toggleHyperlapse() {
    if (!isHyperlapse()) {
        setMode(AutoModeType::HYPERLAPSE);
    } else {
        setMode(AutoModeType::REAL_TIME);
    }
}

void AutoEffect::setSimulatedTime(double total_seconds) {
    if (std::isnan(total_seconds) || std::isinf(total_seconds)) return;
    double clamped = std::fmod(total_seconds, Config::SECONDS_IN_DAY);
    if (clamped < 0.0) clamped += Config::SECONDS_IN_DAY;

    if (isHyperlapse()) {
        m_hyperlapse_seconds = clamped;
    } else {
        m_simulated_seconds = clamped;
        m_saved_real_time_seconds = clamped;
    }
    m_time_initialized = true;
}

void AutoEffect::setDayOfYear(uint16_t day_of_year) {
    m_day_of_year = std::clamp(day_of_year, static_cast<uint16_t>(1), static_cast<uint16_t>(365));
}

float AutoEffect::calculateMoonPhaseFactor() const {
    const double SYNODIC_MONTH = 29.530588;
    double active_seconds = isHyperlapse() ? m_hyperlapse_seconds : m_simulated_seconds;
    double total_days = static_cast<double>(m_day_of_year) + (active_seconds / Config::SECONDS_IN_DAY);
    double lunar_age = std::fmod(total_days, SYNODIC_MONTH);

    if (lunar_age < 0.0) lunar_age += SYNODIC_MONTH;

    float phase_factor = static_cast<float>((1.0 - std::cos((lunar_age / SYNODIC_MONTH) * 2.0 * Config::PI)) * 0.5);
    return std::clamp(phase_factor, 0.0f, 1.0f);
}

void AutoEffect::updateWithMasterTime(uint32_t delta_ms, double simulated_seconds, uint16_t day_of_year) {
    if (std::isnan(simulated_seconds) || std::isinf(simulated_seconds)) {
        simulated_seconds = 0.0;
    }

    double normalized_master = std::fmod(simulated_seconds, Config::SECONDS_IN_DAY);
    if (normalized_master < 0.0) normalized_master += Config::SECONDS_IN_DAY;

    m_day_of_year = std::clamp(day_of_year, static_cast<uint16_t>(1), static_cast<uint16_t>(365));

    if (isHyperlapse()) {
        m_simulated_seconds = m_saved_real_time_seconds;

        double cycle_duration_sec = static_cast<double>(Config::HYPERLAPSE_DURATION_MINUTES) * 60.0;
        if (cycle_duration_sec <= 0.0) cycle_duration_sec = 60.0;
        
        double speed_multiplier = Config::SECONDS_IN_DAY / cycle_duration_sec;
        double time_step_sec = (static_cast<double>(delta_ms) / 1000.0) * speed_multiplier;
        
        m_hyperlapse_seconds += time_step_sec;
        if (m_hyperlapse_seconds >= Config::SECONDS_IN_DAY) {
            m_hyperlapse_seconds -= Config::SECONDS_IN_DAY;
        }
    } else {
        m_simulated_seconds = normalized_master;
        m_saved_real_time_seconds = normalized_master;
    }

    m_time_initialized = true;

    m_watchdog_save_timer_ms += delta_ms;
    if (m_watchdog_save_timer_ms >= 1000) {
        m_watchdog_save_timer_ms = 0;
        WatchdogManager::getInstance().saveRecoveryState(m_saved_real_time_seconds, m_day_of_year);
    }

    WatchdogManager::getInstance().kick();

    m_sunrise_effect.update(delta_ms);
    m_sunset_effect.update(delta_ms);
    m_aurora_effect.update(delta_ms);
    m_thunder_effect.update(delta_ms);
    m_cloud_layer.update(delta_ms, isHyperlapse());

    // --- CONSUMING WEATHER PROVIDER IN UPDATE CYCLE ---
    double active_rendering_seconds = isHyperlapse() ? m_hyperlapse_seconds : m_simulated_seconds;
    const double current_hour = active_rendering_seconds / 3600.0;
    ActiveWeatherState weather = m_weather_provider.evaluateWeather(m_day_of_year, current_hour);

    RenderContext ctx{
        current_hour,
        m_day_of_year,
        calculateMoonPhaseFactor(),
        isHyperlapse(),
        weather
    };

    m_solar_layer.update(delta_ms, ctx);
    m_lunar_layer.update(delta_ms, ctx);
    m_meteor_layer.update(delta_ms, ctx.current_hour, ctx.day_of_year, ctx.is_hyperlapse, Config::MATRIX_WIDTH, Config::MATRIX_HEIGHT);
}

void AutoEffect::update(uint32_t delta_ms) {
    updateWithMasterTime(delta_ms, m_simulated_seconds, m_day_of_year);
}

bool AutoEffect::isThunderActive() const {
    double active_rendering_seconds = isHyperlapse() ? m_hyperlapse_seconds : m_simulated_seconds;
    const double current_hour = active_rendering_seconds / 3600.0;
    ActiveWeatherState weather = m_weather_provider.evaluateWeather(m_day_of_year, current_hour);
    
    return weather.is_thunder_possible && weather.cloud_density >= 0.70f;
}

void AutoEffect::render(uint8_t* buffer, size_t width, size_t height) {
    if (!buffer || width == 0 || height == 0) return;

    const size_t total_bytes = width * height * 3;
    if (m_effect_temp_buffer.size() != total_bytes) {
        m_effect_temp_buffer.resize(total_bytes);
    }

    std::fill(buffer, buffer + total_bytes, 0);

    if (!m_time_initialized) {
        return;
    }

    double active_rendering_seconds = isHyperlapse() ? m_hyperlapse_seconds : m_simulated_seconds;
    const double current_hour = active_rendering_seconds / 3600.0;

    // --- EVALUATING WEATHER STATE FOR RENDER PIPELINE ---
    ActiveWeatherState weather = m_weather_provider.evaluateWeather(m_day_of_year, current_hour);

    RenderContext ctx{
        current_hour,
        m_day_of_year,
        calculateMoonPhaseFactor(),
        isHyperlapse(),
        weather
    };

    // 1. Render Base Background Atmosphere Layers
    m_solar_layer.render(buffer, width, height, ctx);
    m_lunar_layer.render(buffer, width, height, ctx);
    m_meteor_layer.render(buffer, width, height, weather.cloud_density); 

    // 2. MATHEMATICAL SOLAR POSITION ENGINE
    const double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
    const double RAD_TO_DEG = 180.0 / 3.14159265358979323846;
    const double latitude = 30.73;  
    const double longitude = 79.07; 

    double gamma = 2.0 * 3.14159265358979323846 * (static_cast<double>(m_day_of_year) - 1.0) / 365.0;
    double eot = 229.18 * (0.000075 + 0.001868 * std::cos(gamma) - 0.032077 * std::sin(gamma) 
                 - 0.014615 * std::cos(2.0 * gamma) - 0.040849 * std::sin(2.0 * gamma));
    double declination = 0.006918 - 0.399912 * std::cos(gamma) + 0.070257 * std::sin(gamma)
                       - 0.006758 * std::cos(2.0 * gamma) + 0.000907 * std::sin(2.0 * gamma)
                       - 0.002697 * std::cos(3.0 * gamma) + 0.001480 * std::sin(3.0 * gamma);

    double solar_noon = (weather.sunrise_hour + weather.sunset_hour) / 2.0;
    double hour_angle_deg = (current_hour - solar_noon) * 15.0;

    double sin_alt = std::sin(latitude * DEG_TO_RAD) * std::sin(declination) + 
                     std::cos(latitude * DEG_TO_RAD) * std::cos(declination) * std::cos(hour_angle_deg * DEG_TO_RAD);
    float solar_altitude = static_cast<float>(std::asin(std::clamp(sin_alt, -1.0, 1.0)) * RAD_TO_DEG);

    // 3. SEAMLESS TWILIGHT AND GOLDEN HOUR LAYER RENDERERS
    const float EFF_START_ALT = -6.0f;   
    const float EFF_END_ALT   =  6.0f;   
    const float Horizon_ALT   = -0.833f; 

    if (solar_altitude >= EFF_START_ALT && solar_altitude <= EFF_END_ALT) {
        float progress = (solar_altitude - EFF_START_ALT) / (EFF_END_ALT - EFF_START_ALT);
        
        float weight = 0.0f;
        if (solar_altitude < Horizon_ALT) {
            weight = smoothstep(EFF_START_ALT, Horizon_ALT, solar_altitude);
        } else {
            weight = 1.0f - smoothstep(Horizon_ALT, EFF_END_ALT, solar_altitude);
        }

        if (current_hour < solar_noon) {
            m_sunrise_effect.renderWithPhase(
                m_effect_temp_buffer.data(), width, height, progress, Config::EAST_DIRECTION_DEGREES, weather.cloud_density
            );

            for (size_t i = 0; i < total_bytes; ++i) {
                buffer[i] = static_cast<uint8_t>(smoothBlend(
                    static_cast<float>(buffer[i]), static_cast<float>(m_effect_temp_buffer[i]), weight
                ));
            }
        }
        else {
            float sunset_progress = 1.0f - progress; 
            const float west_direction = std::fmod(Config::EAST_DIRECTION_DEGREES + 180.0f, 360.0f);

            m_sunset_effect.renderWithPhase(
                m_effect_temp_buffer.data(), width, height, sunset_progress, west_direction, weather.cloud_density
            );

            for (size_t i = 0; i < total_bytes; ++i) {
                buffer[i] = static_cast<uint8_t>(smoothBlend(
                    static_cast<float>(buffer[i]), static_cast<float>(m_effect_temp_buffer[i]), weight
                ));
            }
        }
    }

    // 4. Strict Thunder Mask Execution Guardrail
    if (weather.is_thunder_possible && weather.cloud_density >= 0.70f) {
        m_thunder_effect.render(m_effect_temp_buffer.data(), width, height);
        const float flash_intensity = 0.60f * (1.0f - (weather.cloud_density * 0.30f));
        const uint32_t FREE_LED_THRESHOLD = 15;

        size_t total_pixels = width * height;
        for (size_t i = 0; i < total_pixels; ++i) {
            size_t idx = i * 3;
            uint8_t r = buffer[idx];
            uint8_t g = buffer[idx + 1];
            uint8_t b = buffer[idx + 2];
            uint32_t current_intensity = static_cast<uint32_t>(r) + g + b;

            if (current_intensity <= FREE_LED_THRESHOLD) {
                uint8_t thun_val = m_effect_temp_buffer[idx]; 
                if (thun_val > 20) {
                    uint8_t flash_val = static_cast<uint8_t>(std::min(255.0f, static_cast<float>(thun_val) * flash_intensity));
                    buffer[idx]     = flash_val;
                    buffer[idx + 1] = flash_val;
                    buffer[idx + 2] = flash_val;
                }
            }
        }
    }

    WeatherModifier::applyCloudCover(buffer, width, height, weather, false);
}