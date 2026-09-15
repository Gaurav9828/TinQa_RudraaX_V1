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

bool MeteorShowerLayer::isAnnualMeteorShowerDay(uint16_t day_of_year) const {
    return (day_of_year == 225 || day_of_year == 347);
}

uint32_t MeteorShowerLayer::calculateSpawnInterval(double current_hour, bool is_hyperlapse, bool is_shower_active) const {
    if (is_shower_active) {
        return is_hyperlapse ? 50 : 300; 
    }

    bool is_post_midnight = (current_hour >= 0.0 && current_hour < 4.5);
    
    if (is_hyperlapse) {
        return is_post_midnight ? 100 : 200; // Accelerated spawn rate for hyperlapse observation
    } else {
        return is_post_midnight ? 2500 : 5000;
    }
}

void MeteorShowerLayer::spawnMeteor(size_t width, size_t height, Meteor::Classification classification) {
    for (auto& meteor : m_meteors) {
        if (!meteor.active) {
            meteor.active = true;
            meteor.type = classification;

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
            float speed = 3.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 6.0f;

            meteor.vx = std::cos(angle) * speed;
            meteor.vy = std::sin(angle) * speed;

            meteor.life = 0.0f;
            meteor.life_speed = 0.5f + (static_cast<float>(std::rand()) / RAND_MAX) * 1.0f;

            if (classification == Meteor::Classification::FIREBALL) {
                meteor.max_brightness = 3.2f;
                meteor.brightening = true;
            } else if (classification == Meteor::Classification::BRIGHT) {
                meteor.max_brightness = 1.6f + (static_cast<float>(std::rand()) / RAND_MAX) * 0.8f;
                meteor.brightening = true;
            } else {
                meteor.max_brightness = 0.4f + (static_cast<float>(std::rand()) / RAND_MAX) * 0.8f;
                meteor.brightening = ((std::rand() % 100) < 30);
            }

            meteor.brightness = meteor.brightening ? 0.05f : meteor.max_brightness;
            break;
        }
    }
}

// Updated to accept width and height so meteors span the actual display area
void MeteorShowerLayer::update(uint32_t delta_ms, double current_hour, uint16_t day_of_year, bool is_hyperlapse, size_t width, size_t height) {
    // Fixed: Changed 20.5 to 20.0 so meteors start appearing immediately at 8:00 PM
    const bool is_deep_night = (current_hour >= 20.0 || current_hour < 4.5);
    if (!is_deep_night) {
        init();
        m_last_checked_hour = -1;
        return;
    }

    int current_hour_int = static_cast<int>(current_hour);
    if (m_last_checked_hour != current_hour_int && current_hour >= 20.0) {
        m_meteors_spawned_tonight = 0;
        m_last_checked_hour = current_hour_int;
    }

    const float delta_sec = static_cast<float>(delta_ms) / 1000.0f;
    m_spawn_timer_ms += delta_ms;

    bool is_shower_active = isAnnualMeteorShowerDay(day_of_year);
    uint32_t spawn_interval = calculateSpawnInterval(current_hour, is_hyperlapse, is_shower_active);

    if (m_spawn_timer_ms >= spawn_interval && m_meteors_spawned_tonight < MAX_METEORS) {
        m_spawn_timer_ms = 0;

        int roll = std::rand() % 1000;
        Meteor::Classification classification;
        if (roll < 1) {
            classification = Meteor::Classification::FIREBALL; 
        } else if (roll < 150) {
            classification = Meteor::Classification::BRIGHT;   
        } else {
            classification = Meteor::Classification::FAINT_NORMAL; 
        }

        spawnMeteor(width, height, classification); // Pass actual display dimensions
        m_meteors_spawned_tonight++;
    }

    float hyperlapse_multiplier = is_hyperlapse ? 4.0f : 1.0f;

    for (auto& meteor : m_meteors) {
        if (!meteor.active) continue;

        meteor.x += meteor.vx * delta_sec * hyperlapse_multiplier;
        meteor.y += meteor.vy * delta_sec * hyperlapse_multiplier;
        
        // Scale life advancement in hyperlapse so meteors don't vanish instantly
        meteor.life += delta_sec * meteor.life_speed * hyperlapse_multiplier;

        if (meteor.brightening) {
            meteor.brightness = meteor.max_brightness * (meteor.life * meteor.life);
        } else {
            meteor.brightness = meteor.max_brightness * (1.0f - meteor.life);
        }

        if (meteor.life >= 1.0f || meteor.x < 0.0f || meteor.x >= static_cast<float>(width) || meteor.y < 0.0f || meteor.y >= static_cast<float>(height)) {
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

        if (meteor.type == Meteor::Classification::FIREBALL && meteor.brightness > 2.0f) {
            atmospheric_flash = std::max(atmospheric_flash, (meteor.brightness - 2.0f) * 50.0f);
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
        uint8_t flash_val = static_cast<uint8_t>(std::clamp(atmospheric_flash, 0.0f, 60.0f));
        for (size_t i = 0; i < total_pixels; ++i) {
            buffer[i] = std::min<uint16_t>(255, buffer[i] + flash_val);
        }
    }
}