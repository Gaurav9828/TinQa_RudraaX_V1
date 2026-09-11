#include "AutoEffect.h"
#include "../../config/AppConfig.h"
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstdio>

AutoEffect::AutoEffect()
    : m_simulated_seconds(0.0)
    , m_day_of_year(1)
{
    init();
}

void AutoEffect::init() {
    m_sunrise_delegate.init();
    m_aurora_delegate.init();
    m_thunder_delegate.init();
    m_effect_temp_buffer.clear();
}

void AutoEffect::syncWithSystemTime() {
    time_t now = ::time(nullptr);
    struct tm* local_time = ::localtime(&now);

    if (local_time && local_time->tm_year > 70) {
        m_day_of_year = static_cast<uint16_t>(local_time->tm_yday + 1);
        m_simulated_seconds = static_cast<double>(
            (local_time->tm_hour * 3600) + 
            (local_time->tm_min * 60) + 
            local_time->tm_sec
        );

        int hours = static_cast<int>(m_simulated_seconds) / 3600;
        int mins = (static_cast<int>(m_simulated_seconds) % 3600) / 60;
        int secs = static_cast<int>(m_simulated_seconds) % 60;

        printf("[AUTO EFFECT STARTED] Synced System Clock -> Day: %u | Time: %02d:%02d:%02d\n",
               m_day_of_year, hours, mins, secs);
    } else {
        m_day_of_year = 243;
        m_simulated_seconds = 54000.0;

        printf("[AUTO EFFECT STARTED] RTC Unset. Fallback -> Day: %u | Time: 15:00:00\n",
               m_day_of_year);
    }
}

void AutoEffect::setSimulatedTime(double total_seconds) {
    m_simulated_seconds = std::fmod(total_seconds, 86400.0);
    if (m_simulated_seconds < 0.0) m_simulated_seconds += 86400.0;
}

void AutoEffect::setDayOfYear(uint16_t day_of_year) {
    if (day_of_year < 1) day_of_year = 1;
    if (day_of_year > 365) day_of_year = 365;
    m_day_of_year = day_of_year;
}

float AutoEffect::calculateMoonPhaseFactor() const {
    const double SYNODIC_MONTH = 29.530588;
    
    double total_days = static_cast<double>(m_day_of_year) + (m_simulated_seconds / 86400.0);
    double lunar_age = std::fmod(total_days, SYNODIC_MONTH);
    if (lunar_age < 0.0) lunar_age += SYNODIC_MONTH;

    float phase_factor = static_cast<float>((1.0 - std::cos((lunar_age / SYNODIC_MONTH) * 2.0 * Config::PI)) * 0.5);
    return std::clamp(phase_factor, 0.0f, 1.0f);
}

void AutoEffect::update(uint32_t delta_ms) {

    // Time is controlled centrally by VirtualClock in main.cpp.
    // AutoEffect must never advance its own clock.

    m_sunrise_delegate.update(delta_ms);
    m_aurora_delegate.update(delta_ms);
    m_thunder_delegate.update(delta_ms);
}

void AutoEffect::renderSkyBackground(uint8_t* buffer, size_t width, size_t height) {
    double current_hour = m_simulated_seconds / 3600.0;
    
    // Smooth 24-hour solar cycle curve (0.0 at midnight, 1.0 at noon)
    float raw_cos = std::cos((current_hour - 12.0) * (Config::PI / 12.0));
    float solar_factor = (raw_cos + 1.0f) * 0.5f;

    // Graceful dawn/dusk transition scaling
    float day_weight = 0.0f;
    if (current_hour >= 4.5 && current_hour <= 20.5) {
        // Continuous smooth transition between dawn, noon, and twilight
        day_weight = std::sin(((current_hour - 4.5) / 16.0) * Config::PI);
        day_weight = std::clamp(day_weight, 0.0f, 1.0f);
    }

    uint8_t r = 10;
    uint8_t g = 15;
    uint8_t b = 35;

    if (day_weight > 0.001f) {
        // Daytime: Graceful progression from warm dawn (215, 225, 235) to dense alpine white (250, 252, 255)
        float white_density = std::clamp(solar_factor, 0.0f, 1.0f);

        uint8_t day_r = static_cast<uint8_t>(215 + (250 - 215) * white_density);
        uint8_t day_g = static_cast<uint8_t>(225 + (252 - 225) * white_density);
        uint8_t day_b = static_cast<uint8_t>(235 + (255 - 235) * white_density);

        // Night base incorporating moon illumination
        float moon_phase = calculateMoonPhaseFactor();
        uint8_t night_r = static_cast<uint8_t>(10 + (35 * moon_phase));
        uint8_t night_g = static_cast<uint8_t>(15 + (45 * moon_phase));
        uint8_t night_b = static_cast<uint8_t>(35 + (60 * moon_phase));

        // Smooth cross-fade between night and day
        r = static_cast<uint8_t>(night_r * (1.0f - day_weight) + day_r * day_weight);
        g = static_cast<uint8_t>(night_g * (1.0f - day_weight) + day_g * day_weight);
        b = static_cast<uint8_t>(night_b * (1.0f - day_weight) + day_b * day_weight);
    } else {
        // Night Skyglow Dynamics
        float moon_phase = calculateMoonPhaseFactor();
        r = static_cast<uint8_t>(10 + (35 * moon_phase));
        g = static_cast<uint8_t>(15 + (45 * moon_phase));
        b = static_cast<uint8_t>(35 + (60 * moon_phase));
    }

    size_t total_pixels = width * height;
    for (size_t i = 0; i < total_pixels; ++i) {
        buffer[i * 3]     = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }
}

void AutoEffect::renderBaseStars(uint8_t* buffer, size_t width, size_t height) {
    double current_hour = m_simulated_seconds / 3600.0;
    
    float star_alpha = 0.0f;

    if (current_hour >= 18.5 && current_hour <= 21.0) {
        star_alpha = static_cast<float>((current_hour - 18.5) / 2.5);
    } else if (current_hour > 21.0 || current_hour < 4.0) {
        star_alpha = 1.0f;
    } else if (current_hour >= 4.0 && current_hour <= 6.5) {
        star_alpha = 1.0f - static_cast<float>((current_hour - 4.0) / 2.5);
    }

    if (star_alpha <= 0.01f) return;

    // Moon Phase Extinction
    float moon_phase = calculateMoonPhaseFactor();
    float moon_washout_multiplier = 1.0f - (0.55f * moon_phase);
    star_alpha *= moon_washout_multiplier;

    // Continuous celestial drift
    float pixel_shift = static_cast<float>((m_simulated_seconds / 86400.0) * static_cast<double>(width) * 4.0);

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t shifted_x = (x + static_cast<size_t>(pixel_shift)) % width;

            uint32_t seed = static_cast<uint32_t>(y * width + shifted_x);
            seed = (seed ^ 61) ^ (seed >> 16);
            seed += (seed << 3);
            seed ^= (seed >> 4);
            seed *= 0x27d4eb2d;
            seed ^= (seed >> 15);

            if ((seed % 100) < 6) {
                float raw_brightness = 140.0f + static_cast<float>(seed % 115);
                uint8_t final_brightness = static_cast<uint8_t>(raw_brightness * star_alpha);

                size_t idx = (y * width + x) * 3;
                buffer[idx]     = std::min<uint16_t>(255, buffer[idx] + final_brightness);
                buffer[idx + 1] = std::min<uint16_t>(255, buffer[idx + 1] + final_brightness);
                buffer[idx + 2] = std::min<uint16_t>(255, buffer[idx + 2] + static_cast<uint8_t>(final_brightness * 1.1f));
            }
        }
    }
}

void AutoEffect::renderMoonOnly(uint8_t* buffer, size_t width, size_t height) {
    double current_hour = m_simulated_seconds / 3600.0;
    if (current_hour > 5.5 && current_hour < 19.5) return;

    float moon_phase = calculateMoonPhaseFactor();
    if (moon_phase < 0.05f) return; 

    size_t moon_x = width / 6;
    size_t moon_y = height / 6;
    size_t idx = (moon_y * width + moon_x) * 3;

    if (idx + 2 < width * height * 3) {
        uint8_t intensity = static_cast<uint8_t>(120 + (135 * moon_phase));
        buffer[idx]     = intensity;
        buffer[idx + 1] = intensity;
        buffer[idx + 2] = std::min(255, intensity + 20);
    }
}

void AutoEffect::render(uint8_t* buffer, size_t width, size_t height) {
    if (!buffer || width == 0 || height == 0) return;

    size_t total_bytes = width * height * 3;
    if (m_effect_temp_buffer.size() != total_bytes) {
        m_effect_temp_buffer.resize(total_bytes);
    }

    double current_hour = m_simulated_seconds / 3600.0;
    bool is_daytime = (current_hour >= 6.0 && current_hour <= 18.5);

    // 1. Fetch Weather Profile
    ActiveWeatherState weather = m_weather_provider.evaluateWeather(m_day_of_year, current_hour);

    // 2. Render Sky Background & Celestials
    renderSkyBackground(buffer, width, height);

    if (weather.cloud_density < 0.60f) {
        renderBaseStars(buffer, width, height);
        renderMoonOnly(buffer, width, height);
    }

    // 3. Winter Aurora Night Layer
    if (weather.is_aurora_night && !is_daytime) {
        m_aurora_delegate.render(m_effect_temp_buffer.data(), width, height);
        
        float alpha = 0.65f * (1.0f - weather.cloud_density);
        for (size_t i = 0; i < total_bytes; ++i) {
            buffer[i] = static_cast<uint8_t>(buffer[i] * (1.0f - alpha) + m_effect_temp_buffer[i] * alpha);
        }
    }

    // 4. Sunrise (04:30 - 08:30) & Sunset (16:30 - 20:30)
    if (current_hour >= 4.5 && current_hour <= 8.5) {
        float phase = static_cast<float>((current_hour - 4.5) / 4.0);
        float weight = std::sin(phase * Config::PI);

        m_sunrise_delegate.renderWithPhase(
            m_effect_temp_buffer.data(), width, height, 
            phase, Config::EAST_DIRECTION_DEGREES
        );

        for (size_t i = 0; i < total_bytes; ++i) {
            buffer[i] = static_cast<uint8_t>(buffer[i] * (1.0f - weight) + m_effect_temp_buffer[i] * weight);
        }
    }
    else if (current_hour >= 16.5 && current_hour <= 20.5) {
        float raw_phase = static_cast<float>((current_hour - 16.5) / 4.0);
        float sunset_phase = 1.0f - raw_phase;
        float weight = std::sin(raw_phase * Config::PI);

        float west_direction = std::fmod(Config::EAST_DIRECTION_DEGREES + 180.0f, 360.0f);

        // Fixed width parameter passing for sunset delegate
        m_sunrise_delegate.renderWithPhase(
            m_effect_temp_buffer.data(), width, height, 
            sunset_phase, west_direction
        );

        for (size_t i = 0; i < total_bytes; ++i) {
            buffer[i] = static_cast<uint8_t>(buffer[i] * (1.0f - weight) + m_effect_temp_buffer[i] * weight);
        }
    }

    // 5. Thunder Burst Intermittent Windows
    if (weather.is_thunder_possible) {
        double cycle_minutes = std::fmod(m_simulated_seconds / 60.0, 110.0);
        bool is_thunder_burst_active = (cycle_minutes < 20.0);

        if (is_thunder_burst_active) {
            m_thunder_delegate.render(m_effect_temp_buffer.data(), width, height);

            float flash_intensity = 0.65f;
            for (size_t i = 0; i < total_bytes; ++i) {
                if (m_effect_temp_buffer[i] > 40) {
                    buffer[i] = static_cast<uint8_t>(
                        std::min(255.0f, buffer[i] + m_effect_temp_buffer[i] * flash_intensity)
                    );
                }
            }
        }
    }

    // 6. Overcast Cloud Filter
    WeatherModifier::applyCloudCover(buffer, width, height, weather, is_daytime);
}