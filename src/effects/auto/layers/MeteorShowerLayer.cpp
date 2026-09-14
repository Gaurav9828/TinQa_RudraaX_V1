#include "MeteorShowerLayer.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

MeteorShowerLayer::MeteorShowerLayer()
    : m_spawn_timer_ms(0), m_meteors_spawned_tonight(0), m_last_checked_hour(-1)
{
    m_meteors.resize(MAX_METEORS);
    init();
}

void MeteorShowerLayer::init() {
    m_spawn_timer_ms = 0;
    m_meteors_spawned_tonight = 0;
    for (auto& meteor : m_meteors) {
        meteor.active = false;
    }
}

void MeteorShowerLayer::spawnMeteor(size_t width, size_t height, bool is_mega_meteor) {
    for (auto& meteor : m_meteors) {
        if (!meteor.active) {
            meteor.active = true;

            int edge = std::rand() % 3;
            if (edge == 0) {
                meteor.x = static_cast<float>(std::rand() % std::max<size_t>(1, width));
                meteor.y = 0.0f;
            } else if (edge == 1) {
                meteor.x = 0.0f;
                meteor.y = static_cast<float>(std::rand() % std::max<size_t>(1, height / 2));
            } else {
                meteor.x = static_cast<float>(width - 1);
                meteor.y = static_cast<float>(std::rand() % std::max<size_t>(1, height / 2));
            }

            float angle = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f * 3.14159f;
            float speed = 3.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 7.0f;

            meteor.vx = std::cos(angle) * speed;
            meteor.vy = std::sin(angle) * speed;

            meteor.life = 0.0f;
            meteor.life_speed = 0.4f + (static_cast<float>(std::rand()) / RAND_MAX) * 1.2f;

            if (is_mega_meteor) {
                meteor.max_brightness = 2.8f; 
                meteor.brightening = true;
            } else {
                meteor.max_brightness = 0.6f + (static_cast<float>(std::rand()) / RAND_MAX) * 1.4f;
                meteor.brightening = ((std::rand() % 100) < 35);
            }

            meteor.brightness = meteor.brightening ? 0.05f : meteor.max_brightness;
            break;
        }
    }
}

void MeteorShowerLayer::update(uint32_t delta_ms, double current_hour, bool is_hyperlapse) {
    const bool is_deep_night = (current_hour >= 20.5 || current_hour < 4.5);
    if (!is_deep_night) {
        init();
        m_last_checked_hour = -1;
        return;
    }

    int current_hour_int = static_cast<int>(current_hour);
    if (m_last_checked_hour != current_hour_int && current_hour >= 20.5) {
        m_meteors_spawned_tonight = 0;
        m_last_checked_hour = current_hour_int;
    }

    const float delta_sec = static_cast<float>(delta_ms) / 1000.0f;
    m_spawn_timer_ms += delta_ms;

    bool is_weekly_mega_meteor = ((std::rand() % 300) == 0);

    const uint32_t spawn_interval = is_hyperlapse ? 400 : 3500;
    if (m_spawn_timer_ms >= spawn_interval && m_meteors_spawned_tonight < MAX_METEORS) {
        m_spawn_timer_ms = 0;
        if ((std::rand() % 100) < 40) {
            spawnMeteor(16, 16, is_weekly_mega_meteor);
            m_meteors_spawned_tonight++;
        }
    }

    for (auto& meteor : m_meteors) {
        if (!meteor.active) continue;

        meteor.x += meteor.vx * delta_sec * (is_hyperlapse ? 4.0f : 1.0f);
        meteor.y += meteor.vy * delta_sec * (is_hyperlapse ? 4.0f : 1.0f);
        meteor.life += delta_sec * meteor.life_speed;

        if (meteor.brightening) {
            meteor.brightness = meteor.max_brightness * (meteor.life * meteor.life);
        } else {
            meteor.brightness = meteor.max_brightness * (1.0f - meteor.life);
        }

        if (meteor.life >= 1.0f || meteor.x < 0.0f || meteor.x >= 16.0f || meteor.y < 0.0f || meteor.y >= 16.0f) {
            meteor.active = false;
        }
    }
}

void MeteorShowerLayer::render(uint8_t* buffer, size_t width, size_t height, float cloud_density) {
    if (!buffer || width == 0 || height == 0) return;

    const float cloud_mask = 1.0f - (cloud_density * 0.80f);
    if (cloud_mask <= 0.0f) return;

    float atmospheric_flash = 0.0f;

    for (const auto& meteor : m_meteors) {
        if (!meteor.active) continue;

        if (meteor.max_brightness > 2.0f && meteor.brightness > 1.8f) {
            atmospheric_flash = std::max(atmospheric_flash, (meteor.brightness - 1.8f) * 60.0f);
        }

        const int ix = static_cast<int>(std::round(meteor.x));
        const int iy = static_cast<int>(std::round(meteor.y));

        if (ix >= 0 && static_cast<size_t>(ix) < width &&
            iy >= 0 && static_cast<size_t>(iy) < height) {

            const uint8_t val = static_cast<uint8_t>(std::clamp(255.0f * meteor.brightness * cloud_mask, 0.0f, 255.0f));
            const size_t idx = (iy * width + ix) * 3;

            buffer[idx + 0] = std::min<uint16_t>(255, buffer[idx + 0] + val);
            buffer[idx + 1] = std::min<uint16_t>(255, buffer[idx + 1] + val);
            buffer[idx + 2] = std::min<uint16_t>(255, buffer[idx + 2] + val);
        }
    }

    if (atmospheric_flash > 0.0f) {
        size_t total_pixels = width * height * 3;
        uint8_t flash_val = static_cast<uint8_t>(std::clamp(atmospheric_flash, 0.0f, 50.0f));
        for (size_t i = 0; i < total_pixels; ++i) {
            buffer[i] = std::min<uint16_t>(255, buffer[i] + flash_val);
        }
    }
}