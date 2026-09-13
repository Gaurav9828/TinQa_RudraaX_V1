#include "AutoEffect.h"
#include "../../config/AppConfig.h"
#include "pico/stdlib.h"
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstdio>

namespace {
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp(
            (x - edge0) / (edge1 - edge0),
            0.0f,
            1.0f
        );

        return t * t * t *
               (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    inline float smoothBlend(float a, float b, float amount) {
        amount = smoothstep(0.0f, 1.0f, amount);
        return a + (b - a) * amount;
    }
}

AutoEffect::AutoEffect()
    : m_mode(AutoModeType::HYPERLAPSE),
      m_simulated_seconds(28800.0),
      m_day_of_year(150),
      m_cached_width(0),
      m_cached_height(0)
{
    init();
}

void AutoEffect::init() {
    m_sunrise_effect.init();
    m_sunset_effect.init();
    m_aurora_effect.init();
    m_thunder_effect.init();

    m_effect_temp_buffer.clear();

    m_stars.clear();
    m_cached_width = 0;
    m_cached_height = 0;

    if (m_mode == AutoModeType::REAL_TIME) {
        syncWithSystemTime();
    }
}

void AutoEffect::setMode(AutoModeType mode) {
    m_mode = mode;

    if (m_mode == AutoModeType::REAL_TIME) {
        syncWithSystemTime();
    }
}

void AutoEffect::toggleHyperlapse() {
    setMode(
        isHyperlapse()
            ? AutoModeType::REAL_TIME
            : AutoModeType::HYPERLAPSE
    );
}

void AutoEffect::syncWithSystemTime() {
    time_t now = ::time(nullptr);
    struct tm* local_time = ::localtime(&now);

    if (local_time && local_time->tm_year > 70) {
        m_day_of_year =
            static_cast<uint16_t>(local_time->tm_yday + 1);

        m_simulated_seconds =
            static_cast<double>(
                (local_time->tm_hour * 3600) +
                (local_time->tm_min * 60) +
                local_time->tm_sec
            );
    } else {
        m_day_of_year = 150;
        m_simulated_seconds = 28800.0;
    }
}

void AutoEffect::setSimulatedTime(double total_seconds) {
    m_simulated_seconds =
        std::fmod(
            total_seconds,
            Config::SECONDS_IN_DAY
        );

    if (m_simulated_seconds < 0.0) {
        m_simulated_seconds +=
            Config::SECONDS_IN_DAY;
    }
}

void AutoEffect::setDayOfYear(uint16_t day_of_year) {
    m_day_of_year =
        std::clamp(
            day_of_year,
            static_cast<uint16_t>(1),
            static_cast<uint16_t>(365)
        );
}

float AutoEffect::calculateMoonPhaseFactor() const {
    const double SYNODIC_MONTH = 29.530588;

    double total_days =
        static_cast<double>(m_day_of_year) +
        (m_simulated_seconds / Config::SECONDS_IN_DAY);

    double lunar_age =
        std::fmod(
            total_days,
            SYNODIC_MONTH
        );

    if (lunar_age < 0.0) {
        lunar_age += SYNODIC_MONTH;
    }

    float phase_factor =
        static_cast<float>(
            (1.0 -
             std::cos(
                 (lunar_age / SYNODIC_MONTH) *
                 2.0 *
                 Config::PI
             )) *
            0.5
        );

    return std::clamp(
        phase_factor,
        0.0f,
        1.0f
    );
}

void AutoEffect::updateTimeProgress(uint32_t delta_ms) {
    if (m_mode == AutoModeType::HYPERLAPSE) {
        const double hyperlapse_duration =
            static_cast<double>(
                Config::HYPERLAPSE_DAY_DURATION_MINUTES *
                60.0
            );

        const double speed_multiplier =
            Config::SECONDS_IN_DAY /
            (
                hyperlapse_duration > 0.0
                    ? hyperlapse_duration
                    : 120.0
            );

        m_simulated_seconds +=
            (
                static_cast<double>(delta_ms) /
                1000.0
            ) *
            speed_multiplier;

        while (m_simulated_seconds >=
               Config::SECONDS_IN_DAY) {

            m_simulated_seconds -=
                Config::SECONDS_IN_DAY;

            m_day_of_year =
                static_cast<uint16_t>(
                    (m_day_of_year % 365) + 1
                );
        }
    } else {
        m_simulated_seconds +=
            static_cast<double>(delta_ms) /
            1000.0;

        while (m_simulated_seconds >=
               Config::SECONDS_IN_DAY) {

            m_simulated_seconds -=
                Config::SECONDS_IN_DAY;

            m_day_of_year =
                static_cast<uint16_t>(
                    (m_day_of_year % 365) + 1
                );
        }
    }
}

void AutoEffect::generateFixedStars(
    size_t width,
    size_t height)
{
    if (m_cached_width == width &&
        m_cached_height == height &&
        !m_stars.empty()) {
        return;
    }

    m_cached_width = width;
    m_cached_height = height;
    m_stars.clear();

    if (width == 0 || height == 0) {
        return;
    }

    const size_t MIN_STAR_COUNT = 3;
    const size_t MAX_STAR_COUNT = 10;

    uint32_t base_seed =
        static_cast<uint32_t>(
            (width * 73856093u) ^
            (height * 19349663u)
        );

    const size_t star_count =
        MIN_STAR_COUNT +
        (
            base_seed %
            (MAX_STAR_COUNT -
             MIN_STAR_COUNT +
             1)
        );

    for (size_t i = 0;
         i < star_count;
         ++i) {

        uint32_t seed =
            base_seed ^
            static_cast<uint32_t>(
                (i + 1) * 2654435761u
            );

        size_t x =
            static_cast<size_t>(
                (
                    seed * 1664525u +
                    1013904223u
                ) %
                width
            );

        size_t y =
            static_cast<size_t>(
                (
                    (
                        seed ^
                        0x9E3779B9u
                    ) *
                    1664525u +
                    1013904223u
                ) %
                std::max<size_t>(
                    1,
                    height / 2
                )
            );

        FixedStar star;
        star.x = x;
        star.y = y;
        star.seed = seed;

        bool duplicate = false;

        for (const auto& existing : m_stars) {
            if (existing.x == star.x &&
                existing.y == star.y) {

                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            m_stars.push_back(star);
        }
    }

    for (size_t i = 0;
         m_stars.size() < MIN_STAR_COUNT &&
         m_stars.size() < MAX_STAR_COUNT;
         ++i) {

        uint32_t seed =
            base_seed ^
            static_cast<uint32_t>(
                (i + 37) * 1103515245u
            );

        FixedStar star;

        star.x =
            static_cast<size_t>(
                seed % width
            );

        star.y =
            static_cast<size_t>(
                (seed >> 8) %
                std::max<size_t>(
                    1,
                    height / 2
                )
            );

        star.seed = seed;

        bool duplicate = false;

        for (const auto& existing : m_stars) {
            if (existing.x == star.x &&
                existing.y == star.y) {

                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            m_stars.push_back(star);
        }
    }
}

void AutoEffect::update(uint32_t delta_ms) {
    updateTimeProgress(delta_ms);

    m_sunrise_effect.update(delta_ms);
    m_sunset_effect.update(delta_ms);
    m_aurora_effect.update(delta_ms);
    m_thunder_effect.update(delta_ms);
}

void AutoEffect::renderMountainLighting(
    uint8_t* buffer,
    size_t width,
    size_t height,
    const ActiveWeatherState& weather)
{
    const double current_hour =
        m_simulated_seconds / 3600.0;

    float daylight = 0.0f;

    /*
     * Full 24-hour smooth cycle:
     *
     * 05:00  -> sunrise begins
     * 06:30  -> full daylight
     * 17:30  -> sunset begins
     * 19:30  -> night
     */
    if (current_hour >= 5.0 &&
        current_hour < 6.5) {

        daylight =
            smoothstep(
                0.0f,
                1.0f,
                static_cast<float>(
                    (current_hour - 5.0) /
                    1.5
                )
            );

    } else if (current_hour >= 6.5 &&
               current_hour <= 17.5) {

        daylight = 1.0f;

    } else if (current_hour > 17.5 &&
               current_hour <= 19.5) {

        daylight =
            1.0f -
            smoothstep(
                0.0f,
                1.0f,
                static_cast<float>(
                    (current_hour - 17.5) /
                    2.0
                )
            );
    }

    float solar_peak = 0.0f;

    if (current_hour >= 6.0 &&
        current_hour <= 18.0) {

        solar_peak =
            std::sin(
                static_cast<float>(
                    (
                        (current_hour - 6.0) /
                        12.0
                    ) *
                    Config::PI
                )
            );

        solar_peak =
            std::clamp(
                solar_peak,
                0.0f,
                1.0f
            );

        solar_peak =
            smoothstep(
                0.0f,
                1.0f,
                solar_peak
            );
    }

    const float cloud_day_factor =
        1.0f -
        (weather.cloud_density * 0.40f);

    const float day_r =
        (
            220.0f +
            (240.0f - 220.0f) *
            solar_peak
        ) *
        cloud_day_factor;

    const float day_g =
        (
            165.0f +
            (205.0f - 165.0f) *
            solar_peak
        ) *
        cloud_day_factor;

    const float day_b =
        (
            85.0f +
            (130.0f - 85.0f) *
            solar_peak
        ) *
        cloud_day_factor;

    const uint8_t r =
        static_cast<uint8_t>(
            std::clamp(
                day_r * daylight,
                0.0f,
                255.0f
            )
        );

    const uint8_t g =
        static_cast<uint8_t>(
            std::clamp(
                day_g * daylight,
                0.0f,
                255.0f
            )
        );

    const uint8_t b =
        static_cast<uint8_t>(
            std::clamp(
                day_b * daylight,
                0.0f,
                255.0f
            )
        );

    const size_t total_pixels =
        width * height;

    for (size_t i = 0;
         i < total_pixels;
         ++i) {

        buffer[i * 3 + 0] = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }
}

void AutoEffect::renderStaticStars(
    uint8_t* buffer,
    size_t width,
    size_t height,
    float cloud_density)
{
    const double current_hour =
        m_simulated_seconds / 3600.0;

    float star_visibility = 0.0f;

    /*
     * Stars:
     *
     * 19:00 -> 20:30 : smoothly appear
     * 20:30 -> 04:30 : fully visible
     * 04:30 -> 06:00 : smoothly disappear
     */
    if (current_hour >= 19.0 &&
        current_hour <= 20.5) {

        star_visibility =
            smoothstep(
                0.0f,
                1.0f,
                static_cast<float>(
                    (current_hour - 19.0) /
                    1.5
                )
            );

    } else if (current_hour > 20.5 ||
               current_hour < 4.5) {

        star_visibility = 1.0f;

    } else if (current_hour >= 4.5 &&
               current_hour < 6.0) {

        star_visibility =
            1.0f -
            smoothstep(
                0.0f,
                1.0f,
                static_cast<float>(
                    (current_hour - 4.5) /
                    1.5
                )
            );
    }

    if (star_visibility <= 0.0f) {
        return;
    }

    star_visibility *=
        1.0f -
        (cloud_density * 0.45f);

    if (star_visibility <= 0.0f) {
        return;
    }

    generateFixedStars(width, height);

    /*
     * Extremely low static brightness.
     * No twinkle.
     * No random disappearance.
     */
    const float STAR_BRIGHTNESS = 2.0f;

    for (const auto& star : m_stars) {
        if (star.x >= width ||
            star.y >= height) {
            continue;
        }

        const uint8_t final_val =
            static_cast<uint8_t>(
                std::clamp(
                    STAR_BRIGHTNESS *
                    star_visibility,
                    1.0f,
                    2.0f
                )
            );

        const size_t idx =
            (star.y * width + star.x) * 3;

        buffer[idx + 0] =
            std::min<uint16_t>(
                255,
                buffer[idx + 0] + final_val
            );

        buffer[idx + 1] =
            std::min<uint16_t>(
                255,
                buffer[idx + 1] + final_val
            );

        buffer[idx + 2] =
            std::min<uint16_t>(
                255,
                buffer[idx + 2] + final_val
            );
    }
}

void AutoEffect::renderCornerMoon(
    uint8_t* buffer,
    size_t width,
    size_t height,
    float cloud_density)
{
    const double current_hour =
        m_simulated_seconds / 3600.0;

    /*
     * Moon is visible during the night.
     * Smoothly fades in/out around the
     * same night transition used by stars.
     */
    if (current_hour >= 5.5 &&
        current_hour <= 19.5) {
        return;
    }

    float moon_visibility = 1.0f;

    if (current_hour >= 19.5 &&
        current_hour <= 20.5) {

        moon_visibility =
            smoothstep(
                0.0f,
                1.0f,
                static_cast<float>(
                    current_hour - 19.5
                )
            );

    } else if (current_hour >= 4.5 &&
               current_hour < 5.5) {

        moon_visibility =
            1.0f -
            smoothstep(
                0.0f,
                1.0f,
                static_cast<float>(
                    current_hour - 4.5
                )
            );
    }

    moon_visibility *=
        1.0f -
        (cloud_density * 0.70f);

    if (moon_visibility <= 0.0f) {
        return;
    }

    const float phase =
        calculateMoonPhaseFactor();

    const int moon_x =
        static_cast<int>(
            width > 2
                ? width - 2
                : width - 1
        );

    const int moon_y = 1;

    const uint8_t core_val =
        static_cast<uint8_t>(
            (
                2.0f +
                6.0f * phase
            ) *
            moon_visibility
        );

    const size_t core_idx =
        (moon_y * width + moon_x) * 3;

    buffer[core_idx + 0] =
        std::min<uint16_t>(
            255,
            buffer[core_idx + 0] +
            core_val
        );

    buffer[core_idx + 1] =
        std::min<uint16_t>(
            255,
            buffer[core_idx + 1] +
            core_val
        );

    buffer[core_idx + 2] =
        std::min<uint16_t>(
            255,
            buffer[core_idx + 2] +
            static_cast<uint8_t>(
                core_val * 0.90f
            )
        );
}

void AutoEffect::render(
    uint8_t* buffer,
    size_t width,
    size_t height)
{
    if (!buffer ||
        width == 0 ||
        height == 0) {
        return;
    }

    const size_t total_bytes =
        width * height * 3;

    if (m_effect_temp_buffer.size() !=
        total_bytes) {

        m_effect_temp_buffer.resize(
            total_bytes
        );
    }

    const double current_hour =
        m_simulated_seconds / 3600.0;

    /*
     * Keep this definition consistent with
     * the mountain lighting cycle.
     *
     * Twilight is treated as daytime for
     * cloud attenuation purposes.
     */
    const bool is_daytime =
        (
            current_hour >= 5.0 &&
            current_hour <= 19.5
        );

    // 1. Evaluate Weather State
    ActiveWeatherState weather =
        m_weather_provider.evaluateWeather(
            m_day_of_year,
            current_hour
        );

    // 2. Base Mountain Ambient Day/Night Frame
    renderMountainLighting(
        buffer,
        width,
        height,
        weather
    );

    // 3. Render Corner Moon during Night hours
    renderCornerMoon(
        buffer,
        width,
        height,
        weather.cloud_density
    );

    // 4. Render 3-10 Fixed Static Stars
    renderStaticStars(
        buffer,
        width,
        height,
        weather.cloud_density
    );

    // 5. Sunrise Overlay Transition
    if (current_hour >= 4.5 &&
        current_hour <= 8.5) {

        const float raw_progress =
            static_cast<float>(
                (current_hour - 4.5) /
                4.0
            );

        const float sunrise_phase =
            smoothstep(
                0.0f,
                1.0f,
                raw_progress
            );

        const float weight =
            smoothstep(
                0.0f,
                1.0f,
                std::sin(
                    raw_progress *
                    Config::PI
                )
            );

        m_sunrise_effect.renderWithPhase(
            m_effect_temp_buffer.data(),
            width,
            height,
            sunrise_phase,
            Config::EAST_DIRECTION_DEGREES
        );

        for (size_t i = 0;
             i < total_bytes;
             ++i) {

            buffer[i] =
                static_cast<uint8_t>(
                    smoothBlend(
                        static_cast<float>(
                            buffer[i]
                        ),
                        static_cast<float>(
                            m_effect_temp_buffer[i]
                        ),
                        weight
                    )
                );
        }
    }

    // 6. Sunset Overlay Transition
    else if (current_hour >= 16.5 &&
             current_hour <= 20.0) {

        const float raw_progress =
            static_cast<float>(
                (current_hour - 16.5) /
                3.5
            );

        const float sunset_phase =
            1.0f -
            smoothstep(
                0.0f,
                1.0f,
                raw_progress
            );

        const float weight =
            smoothstep(
                0.0f,
                1.0f,
                std::sin(
                    raw_progress *
                    Config::PI
                )
            );

        const float west_direction =
            std::fmod(
                Config::EAST_DIRECTION_DEGREES +
                180.0f,
                360.0f
            );

        m_sunset_effect.renderWithPhase(
            m_effect_temp_buffer.data(),
            width,
            height,
            sunset_phase,
            west_direction
        );

        for (size_t i = 0;
             i < total_bytes;
             ++i) {

            buffer[i] =
                static_cast<uint8_t>(
                    smoothBlend(
                        static_cast<float>(
                            buffer[i]
                        ),
                        static_cast<float>(
                            m_effect_temp_buffer[i]
                        ),
                        weight
                    )
                );
        }
    }

    // 7. Thunder Strikes (Storms)
    if (weather.is_thunder_possible) {
        m_thunder_effect.render(
            m_effect_temp_buffer.data(),
            width,
            height
        );

        const float flash_intensity =
            0.60f *
            (
                1.0f -
                (weather.cloud_density * 0.30f)
            );

        for (size_t i = 0;
             i < total_bytes;
             ++i) {

            if (m_effect_temp_buffer[i] > 20) {
                buffer[i] =
                    static_cast<uint8_t>(
                        std::min(
                            255.0f,
                            buffer[i] +
                            m_effect_temp_buffer[i] *
                            flash_intensity
                        )
                    );
            }
        }
    }

    // 8. Overcast Cloud Attenuation
    WeatherModifier::applyCloudCover(
        buffer,
        width,
        height,
        weather,
        is_daytime
    );
}