#include "MeteorShowerLayer.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstring>

MeteorShowerLayer::MeteorShowerLayer()
    : m_spawn_timer_ms(0), m_meteors_spawned_tonight(0), m_last_checked_hour(-1), m_special_meteor_spawned_tonight(false)
{
    m_meteors.resize(MAX_METEORS);
    init();
}

void MeteorShowerLayer::init() {
    m_spawn_timer_ms = 0;
    m_meteors_spawned_tonight = 0;
    m_special_meteor_spawned_tonight = false;
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
        return is_post_midnight ? 100 : 200; 
    } else {
        return is_post_midnight ? 2500 : 5000;
    }
}

void MeteorShowerLayer::spawnMeteor(size_t width, size_t height, Meteor::Classification classification, bool force_special_diagonal) {
    for (auto& meteor : m_meteors) {
        if (!meteor.active) {
            meteor.active = true;
            meteor.type = classification;

            float max_x = static_cast<float>(width - 1);
            float max_y = static_cast<float>(height - 1);

            if (force_special_diagonal) {
                meteor.is_special = true;

                // Pick one of 4 corners randomly as starting position (A, B, C, or D)
                // 0: Top-Left (0,0), 1: Top-Right (max_x, 0)
                // 2: Bottom-Left (0, max_y), 3: Bottom-Right (max_x, max_y)
                int start_corner = std::rand() % 4;
                int target_corner = 3 - start_corner; // Opposite diagonal corner

                meteor.start_x = (start_corner == 1 || start_corner == 3) ? max_x : 0.0f;
                meteor.start_y = (start_corner == 2 || start_corner == 3) ? max_y : 0.0f;

                meteor.end_x = (target_corner == 1 || target_corner == 3) ? max_x : 0.0f;
                meteor.end_y = (target_corner == 2 || target_corner == 3) ? max_y : 0.0f;

                meteor.x = meteor.start_x;
                meteor.y = meteor.start_y;

                // For rendering tail direction vector
                float dx = meteor.end_x - meteor.start_x;
                float dy = meteor.end_y - meteor.start_y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < 1.0f) dist = 1.0f;
                
                meteor.vx = (dx / dist) * 5.0f;
                meteor.vy = (dy / dist) * 5.0f;

                meteor.type = Meteor::Classification::FIREBALL;
                meteor.max_brightness = 3.5f;
                meteor.brightening = true;
                meteor.life_speed = 0.25f; // Slower duration so it takes its full time across the screen
            } else {
                meteor.is_special = false;
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
                float speed = 2.0f + (static_cast<float>(std::rand()) / RAND_MAX * 3.0f);

                meteor.vx = std::cos(angle) * speed;
                meteor.vy = std::sin(angle) * speed;
                meteor.life_speed = 0.4f + (static_cast<float>(std::rand()) / RAND_MAX * 0.6f);

                if (classification == Meteor::Classification::FIREBALL) {
                    meteor.max_brightness = 3.2f;
                    meteor.brightening = true;
                } else if (classification == Meteor::Classification::BRIGHT) {
                    meteor.max_brightness = 1.6f + (static_cast<float>(std::rand()) / RAND_MAX * 0.8f);
                    meteor.brightening = true;
                } else {
                    meteor.max_brightness = 0.4f + (static_cast<float>(std::rand()) / RAND_MAX * 0.8f);
                    meteor.brightening = ((std::rand() % 100) < 30);
                }
            }

            meteor.life = 0.0f;
            meteor.brightness = meteor.brightening ? 0.05f : meteor.max_brightness;
            break;
        }
    }
}

void MeteorShowerLayer::update(uint32_t delta_ms, double current_hour, uint16_t day_of_year, bool is_hyperlapse, size_t width, size_t height) {
    const bool is_deep_night = (current_hour >= 20.0 || current_hour < 4.5);
    if (!is_deep_night) {
        init();
        m_last_checked_hour = -1;
        return;
    }

    int current_hour_int = static_cast<int>(current_hour);
    if (m_last_checked_hour != current_hour_int && current_hour >= 20.0) {
        m_meteors_spawned_tonight = 0;
        m_special_meteor_spawned_tonight = false;
        m_last_checked_hour = current_hour_int;
    }

    const float delta_sec = static_cast<float>(delta_ms) / 1000.0f;
    m_spawn_timer_ms += delta_ms;

    bool is_shower_active = isAnnualMeteorShowerDay(day_of_year);
    uint32_t spawn_interval = calculateSpawnInterval(current_hour, is_hyperlapse, is_shower_active);

    if (m_spawn_timer_ms >= spawn_interval && m_meteors_spawned_tonight < MAX_METEORS) {
        m_spawn_timer_ms = 0;

        bool force_special = false;
        if (!m_special_meteor_spawned_tonight) {
            force_special = true;
            m_special_meteor_spawned_tonight = true;
        }

        int roll = std::rand() % 1000;
        Meteor::Classification classification;
        if (force_special || roll < 1) {
            classification = Meteor::Classification::FIREBALL; 
        } else if (roll < 150) {
            classification = Meteor::Classification::BRIGHT;   
        } else {
            classification = Meteor::Classification::FAINT_NORMAL; 
        }

        spawnMeteor(width, height, classification, force_special);
        m_meteors_spawned_tonight++;
    }

    float hyperlapse_multiplier = is_hyperlapse ? 4.0f : 1.0f;

    for (auto& meteor : m_meteors) {
        if (!meteor.active) continue;

        if (meteor.is_special) {
            // For the special diagonal meteor, limit hyperlapse speedup so it takes its full visible time
            float effective_hyperlapse = is_hyperlapse ? 1.5f : 1.0f;
            meteor.life += delta_sec * meteor.life_speed * effective_hyperlapse;
            
            // Guaranteed exact interpolation from start corner to end corner using quadratic acceleration
            float linear_progress = std::clamp(meteor.life, 0.0f, 1.0f);
            float progress = linear_progress * linear_progress; 

            meteor.x = meteor.start_x + (meteor.end_x - meteor.start_x) * progress;
            meteor.y = meteor.start_y + (meteor.end_y - meteor.start_y) * progress;

            if (meteor.brightening) {
                meteor.brightness = meteor.max_brightness * (linear_progress * linear_progress);
            } else {
                meteor.brightness = meteor.max_brightness * (1.0f - linear_progress);
            }

            if (meteor.life >= 1.0f) {
                meteor.active = false;
            }
        } else {
            // Standard random meteors update logic
            float acceleration_factor = 0.2f + 1.8f * (meteor.life * meteor.life);

            meteor.x += meteor.vx * acceleration_factor * delta_sec * hyperlapse_multiplier;
            meteor.y += meteor.vy * acceleration_factor * delta_sec * hyperlapse_multiplier;
            
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
}

void MeteorShowerLayer::render(uint8_t* buffer, size_t width, size_t height, float cloud_density) {
    if (!buffer || width == 0 || height == 0) return;

    const float cloud_mask = 1.0f - (cloud_density * 0.80f);
    if (cloud_mask <= 0.0f) return;

    for (const auto& meteor : m_meteors) {
        if (!meteor.active) continue;

        float speed = std::sqrt(meteor.vx * meteor.vx + meteor.vy * meteor.vy);
        float dir_x = (speed > 0.001f) ? (meteor.vx / speed) : 0.0f;
        float dir_y = (speed > 0.001f) ? (meteor.vy / speed) : 0.0f;

        const int tail_length = std::max(6, static_cast<int>(width / 3));

        for (int i = tail_length; i >= 0; --i) {
            float tail_dist = static_cast<float>(i) * 0.8f;
            float px = meteor.x - (dir_x * tail_dist);
            float py = meteor.y - (dir_y * tail_dist);

            int ix = static_cast<int>(std::round(px));
            int iy = static_cast<int>(std::round(py));

            if (ix < 0 || static_cast<size_t>(ix) >= width || iy < 0 || static_cast<size_t>(iy) >= height) {
                continue;
            }

            float p_tail = 1.0f - (static_cast<float>(i) / static_cast<float>(tail_length));
            float r = 255.0f, g = 255.0f, b = 255.0f;

            if (p_tail < 0.2f) {
                float t = p_tail / 0.2f;
                r = 240.0f * (1.0f - t) + 100.0f * t;
                g = 240.0f * (1.0f - t) + 180.0f * t;
                b = 255.0f;
            } else if (p_tail < 0.4f) {
                float t = (p_tail - 0.2f) / 0.2f;
                r = 100.0f * (1.0f - t) + 20.0f * t;
                g = 180.0f * (1.0f - t) + 120.0f * t;
                b = 255.0f;
            } else if (p_tail < 0.6f) {
                float t = (p_tail - 0.4f) / 0.2f;
                r = 20.0f  * (1.0f - t) + 30.0f * t;
                g = 120.0f * (1.0f - t) + 240.0f * t;
                b = 255.0f * (1.0f - t) + 80.0f  * t;
            } else if (p_tail < 0.85f) {
                float t = (p_tail - 0.6f) / 0.25f;
                r = 30.0f  * (1.0f - t) + 255.0f * t;
                g = 240.0f * (1.0f - t) + 220.0f * t;
                b = 80.0f  * (1.0f - t) + 50.0f  * t;
            } else {
                r = 255.0f; g = 255.0f; b = 255.0f;
            }

            float intensity = 1.0f - (static_cast<float>(i) / static_cast<float>(tail_length));
            intensity = std::clamp(intensity, 0.0f, 1.0f);
            if (i == 0) intensity = 1.3f; 

            intensity *= meteor.brightness * cloud_mask * 0.35f;

            size_t idx = (iy * width + ix) * 3;
            buffer[idx + 0] = std::min<uint16_t>(255, buffer[idx + 0] + static_cast<uint16_t>(r * intensity));
            buffer[idx + 1] = std::min<uint16_t>(255, buffer[idx + 1] + static_cast<uint16_t>(g * intensity));
            buffer[idx + 2] = std::min<uint16_t>(255, buffer[idx + 2] + static_cast<uint16_t>(b * intensity));
        }

        // Precise 5-LED local circular boom effect when special diagonal meteor reaches the final corner (life >= 0.88f)
        if (meteor.life >= 0.88f) {
            float boom_progress = (meteor.life - 0.88f) / 0.12f;
            boom_progress = std::clamp(boom_progress, 0.0f, 1.0f);

            float flash_intensity = 0.0f;
            if (boom_progress < 0.5f) {
                flash_intensity = boom_progress * 2.0f;
            } else {
                flash_intensity = (1.0f - boom_progress) * 2.0f;
            }
            flash_intensity = std::clamp(flash_intensity, 0.0f, 1.0f) * meteor.brightness * cloud_mask;

            int center_x = static_cast<int>(std::round(meteor.x));
            int center_y = static_cast<int>(std::round(meteor.y));

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (std::abs(dx) + std::abs(dy) <= 1) {
                        int nx = center_x + dx;
                        int ny = center_y + dy;

                        if (nx >= 0 && static_cast<size_t>(nx) < width &&
                            ny >= 0 && static_cast<size_t>(ny) < height) {
                            
                            size_t idx = (ny * width + nx) * 3;
                            uint16_t boom_val = static_cast<uint16_t>(255.0f * flash_intensity);
                            buffer[idx + 0] = std::min<uint16_t>(255, buffer[idx + 0] + boom_val);
                            buffer[idx + 1] = std::min<uint16_t>(255, buffer[idx + 1] + boom_val);
                            buffer[idx + 2] = std::min<uint16_t>(255, buffer[idx + 2] + boom_val);
                        }
                    }
                }
            }
        }
    }
}