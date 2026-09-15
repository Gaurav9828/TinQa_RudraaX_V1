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
    // m_starfield_layer.init();
    m_meteor_layer.init();
    m_cloud_layer.init();

    m_effect_temp_buffer.clear();
}

void AutoEffect::setMode(AutoModeType mode) {
    if (m_mode != AutoModeType::HYPERLAPSE && mode == AutoModeType::HYPERLAPSE) {
        m_saved_real_time_seconds = m_simulated_seconds;
        // Force hyperlapse to start cleanly at 5:00 AM (18000 seconds) instead of reversing
        m_hyperlapse_seconds = 5.0 * 3600.0; 
        m_effect_temp_buffer.clear(); // Clear cached frame buffers
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
    // m_starfield_layer.update(delta_ms, ctx);
    m_meteor_layer.update(delta_ms, ctx.current_hour, ctx.is_hyperlapse);
}

void AutoEffect::update(uint32_t delta_ms) {
    updateWithMasterTime(delta_ms, m_simulated_seconds, m_day_of_year);
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
    const bool is_daytime = (current_hour >= 5.0 && current_hour <= 19.5);

    ActiveWeatherState weather = m_weather_provider.evaluateWeather(m_day_of_year, current_hour);

    RenderContext ctx{
        current_hour,
        m_day_of_year,
        calculateMoonPhaseFactor(),
        isHyperlapse(),
        weather
    };

    // 1. Render Base Layers & Effects
    m_solar_layer.render(buffer, width, height, ctx);
    m_lunar_layer.render(buffer, width, height, ctx);
    // m_starfield_layer.render(buffer, width, height, ctx);
    m_meteor_layer.render(buffer, width, height, ctx.weather.cloud_density);
       
    if (current_hour >= 4.5 && current_hour <= 8.5) {
        const float raw_progress = static_cast<float>((current_hour - 4.5) / 4.0);
        const float sunrise_phase = smoothstep(0.0f, 1.0f, raw_progress);
        const float weight = smoothstep(0.0f, 1.0f, std::sin(raw_progress * Config::PI));

        m_sunrise_effect.renderWithPhase(
            m_effect_temp_buffer.data(), width, height, sunrise_phase, Config::EAST_DIRECTION_DEGREES
        );

        for (size_t i = 0; i < total_bytes; ++i) {
            buffer[i] = static_cast<uint8_t>(smoothBlend(
                static_cast<float>(buffer[i]),
                static_cast<float>(m_effect_temp_buffer[i]),
                weight
            ));
        }
    }
    else if (current_hour >= 16.5 && current_hour <= 20.0) {
        const float raw_progress = static_cast<float>((current_hour - 16.5) / 3.5);
        const float sunset_phase = 1.0f - smoothstep(0.0f, 1.0f, raw_progress);
        const float weight = smoothstep(0.0f, 1.0f, std::sin(raw_progress * Config::PI));
        const float west_direction = std::fmod(Config::EAST_DIRECTION_DEGREES + 180.0f, 360.0f);

        m_sunset_effect.renderWithPhase(
            m_effect_temp_buffer.data(), width, height, sunset_phase, west_direction
        );

        for (size_t i = 0; i < total_bytes; ++i) {
            buffer[i] = static_cast<uint8_t>(smoothBlend(
                static_cast<float>(buffer[i]),
                static_cast<float>(m_effect_temp_buffer[i]),
                weight
            ));
        }
    }

    if (weather.is_thunder_possible) {
        m_thunder_effect.render(m_effect_temp_buffer.data(), width, height);
        const float flash_intensity = 0.60f * (1.0f - (weather.cloud_density * 0.30f));

        for (size_t i = 0; i < total_bytes; ++i) {
            if (m_effect_temp_buffer[i] > 20) {
                buffer[i] = static_cast<uint8_t>(std::min(
                    255.0f,
                    buffer[i] + m_effect_temp_buffer[i] * flash_intensity
                ));
            }
        }
    }

    WeatherModifier::applyCloudCover(buffer, width, height, weather, is_daytime);

    // ========================================================================
    // STEP 3: Render Moving Cloud Layer on top of EVERY effect in Auto Mode
    // ========================================================================
    CloudDensityTier cloud_tier = static_cast<CloudDensityTier>(weather.cloud_density);
    m_cloud_layer.render(buffer, width, height, cloud_tier, is_daytime);
}