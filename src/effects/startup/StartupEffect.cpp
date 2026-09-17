#include "StartupEffect.h"
#include "config/AppConfig.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

StartupEffect::StartupEffect()
    : m_elapsed_ms(0),
      m_duration_ms(8000), // Total duration: 8 seconds (5s starfield + 3s meteor & boom)
      m_is_complete(false),
      m_sparkle_seed(0.0f) {}

void StartupEffect::init() {
    m_elapsed_ms = 0;
    m_is_complete = false;
    m_sparkle_seed = 0.0f;
}

void StartupEffect::update(uint32_t delta_ms) {
    if (m_is_complete) return;

    m_elapsed_ms += delta_ms;
    if (m_elapsed_ms >= m_duration_ms) {
        m_elapsed_ms = m_duration_ms;
        m_is_complete = true;
    }

    m_sparkle_seed += static_cast<float>(delta_ms) * 0.012f;
}

void StartupEffect::render(uint8_t* buffer, size_t width, size_t height) {
    if (!buffer || width == 0 || height == 0) return;

    const size_t total_bytes = width * height * 3;
    std::memset(buffer, 0, total_bytes);

    float total_seconds = static_cast<float>(m_elapsed_ms) / 1000.0f;

    // =========================================================================
    // PHASE 1 & 2: EVENLY DISTRIBUTED MULTI-COLOR TWINKLING & DIMMING (0.0s to 5.0s)
    // =========================================================================
    if (total_seconds < 5.0f) {
        float global_alpha = 1.0f;
        if (total_seconds >= 3.0f) {
            global_alpha = 1.0f - ((total_seconds - 3.0f) / 2.0f);
            global_alpha = std::clamp(global_alpha, 0.0f, 1.0f);
        }

        const int target_total_stars = 60;
        int rows = static_cast<int>(height);
        int stars_per_row = std::max(1, target_total_stars / std::max(1, rows));

        for (int y = 0; y < rows; ++y) {
            for (int s = 0; s < stars_per_row; ++s) {
                size_t star_x = (y * 17 + s * 23 + 7) % width;
                size_t star_y = static_cast<size_t>(y);

                int star_id = y * stars_per_row + s;
                float twinkle_speed = 2.5f + (static_cast<float>(star_id % 7) * 0.4f);
                float twinkle = 0.4f + 0.6f * std::sin(m_sparkle_seed * twinkle_speed + static_cast<float>(star_id * 13));
                
                float brightness = twinkle * 0.20f * global_alpha;

                float color_shift = std::fmod(m_sparkle_seed * 0.5f + static_cast<float>(star_id * 0.3f), 3.0f);
                float r = 255.0f, g = 255.0f, b = 255.0f;

                if (color_shift < 1.0f) {
                    r = 255.0f; g = 180.0f + 75.0f * color_shift; b = 100.0f;
                } else if (color_shift < 2.0f) {
                    float t = color_shift - 1.0f;
                    r = 100.0f; g = 200.0f + 55.0f * t; b = 255.0f;
                } else {
                    float t = color_shift - 2.0f;
                    r = 200.0f + 55.0f * t; g = 255.0f; b = 180.0f + 75.0f * t;
                }

                size_t idx = (star_y * width + star_x) * 3;
                buffer[idx + 0] = static_cast<uint8_t>(std::clamp(r * brightness, 0.0f, 255.0f));
                buffer[idx + 1] = static_cast<uint8_t>(std::clamp(g * brightness, 0.0f, 255.0f));
                buffer[idx + 2] = static_cast<uint8_t>(std::clamp(b * brightness, 0.0f, 255.0f));
            }
        }
        return; 
    }

    // =========================================================================
    // PHASE 3: DISTANT APPROACH METEOR & EXPANDING WAVE BOOM (5.0s to 8.0s)
    // =========================================================================
    float meteor_time = total_seconds - 5.0f; // 0.0 to 3.0 seconds window
    float linear_progress = meteor_time / 3.0f;
    linear_progress = std::clamp(linear_progress, 0.0f, 1.0f);

    float progress = linear_progress * linear_progress;

    float max_x = static_cast<float>(width - 1);
    float max_y = static_cast<float>(height - 1);

    const int tail_length = std::max(6, static_cast<int>(width / 3));

    // Global approach envelope: fades in smoothly from 0.0 to full brightness 
    // over the first 35% of its travel so it genuinely looks like it's approaching from afar.
    float approach_envelope = std::clamp(linear_progress / 0.35f, 0.0f, 1.0f);
    approach_envelope = approach_envelope * approach_envelope; // Smooth quad curve entry

    for (int i = tail_length; i >= 0; --i) {
        float tail_offset = static_cast<float>(i) * 0.06f;
        float p_tail = progress - tail_offset;
        
        if (p_tail < 0.0f) continue;

        float x = max_x * (1.0f - p_tail);
        float y = max_y * (1.0f - p_tail);

        int ix = static_cast<int>(std::round(x));
        int iy = static_cast<int>(std::round(y));

        if (ix < 0 || static_cast<size_t>(ix) >= width || iy < 0 || static_cast<size_t>(iy) >= height) {
            continue;
        }

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

        // Multiply by the distance approach envelope so it fades up smoothly from deep space
        intensity *= 0.35f * approach_envelope; 

        size_t idx = (iy * width + ix) * 3;
        buffer[idx + 0] = std::min(255, static_cast<int>(buffer[idx + 0] + r * intensity));
        buffer[idx + 1] = std::min(255, static_cast<int>(buffer[idx + 1] + g * intensity));
        buffer[idx + 2] = std::min(255, static_cast<int>(buffer[idx + 2] + b * intensity));
    }

    // =========================================================================
    // EXPANDING WAVE BLAST EFFECT FROM METEOR IMPACT POINT
    // =========================================================================
    if (linear_progress >= 0.88f) {
        float boom_progress = (linear_progress - 0.88f) / 0.12f; // ~0.36 second window
        boom_progress = std::clamp(boom_progress, 0.0f, 1.0f);

        // Meteor ends at top-left corner (0, 0)
        float impact_x = 0.0f;
        float impact_y = 0.0f;

        float max_possible_dist = std::hypot(static_cast<float>(width), static_cast<float>(height));
        float wave_radius = boom_progress * max_possible_dist * 1.3f;
        float wave_thickness = std::max(12.0f, max_possible_dist * 0.18f);

        // Quick flash up, rapid dimming down within the window
        float global_boom_intensity = 0.0f;
        if (boom_progress < 0.25f) {
            global_boom_intensity = boom_progress / 0.25f; 
        } else {
            global_boom_intensity = 1.0f - ((boom_progress - 0.25f) / 0.75f); 
        }
        global_boom_intensity = std::clamp(global_boom_intensity, 0.0f, 1.0f);

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);

                float dist = std::hypot(fx - impact_x, fy - impact_y);
                float dist_from_wave = std::abs(dist - wave_radius);

                float wave_intensity = 0.0f;
                // Interior fill behind the shockwave ring
                if (dist <= wave_radius) {
                    float interior_factor = 1.0f - (dist / std::max(1.0f, wave_radius));
                    wave_intensity = 0.3f + 0.5f * interior_factor;
                }
                
                // Bright scattering shockwave ring boundary
                if (dist_from_wave < wave_thickness) {
                    float ring_factor = 1.0f - (dist_from_wave / wave_thickness);
                    wave_intensity += ring_factor * 1.8f;
                }

                wave_intensity = std::clamp(wave_intensity, 0.0f, 2.2f);
                float final_flash = 255.0f * wave_intensity * global_boom_intensity;

                size_t idx = (y * width + x) * 3;
                buffer[idx + 0] = std::min(255, static_cast<int>(buffer[idx + 0] + final_flash));
                buffer[idx + 1] = std::min(255, static_cast<int>(buffer[idx + 1] + final_flash * 0.92f));
                buffer[idx + 2] = std::min(255, static_cast<int>(buffer[idx + 2] + final_flash));
            }
        }
    }
}