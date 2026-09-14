#include "StartupEffect.h"
#include "config/AppConfig.h"
#include <algorithm>
#include <cmath>

StartupEffect::StartupEffect()
    : m_elapsed_ms(0),
      m_duration_ms(12000), // Exactly 10 seconds slow loading sequence
      m_is_complete(false),
      m_pulse_phase(0.0f) {}

void StartupEffect::init() {
    m_elapsed_ms = 0;
    m_is_complete = false;
    m_pulse_phase = 0.0f;
}

void StartupEffect::update(uint32_t delta_ms) {
    if (m_is_complete) return;

    m_elapsed_ms += delta_ms;
    if (m_elapsed_ms >= m_duration_ms) {
        m_is_complete = true;
    }

    // Very slow, smooth breathing/loading frequency
    m_pulse_phase += static_cast<float>(delta_ms) * 0.0012f;
}

void StartupEffect::render(uint8_t* buffer, size_t width, size_t height) {
    if (!buffer || width == 0 || height == 0) return;

    const size_t total_bytes = width * height * 3;
    std::fill(buffer, buffer + total_bytes, 0);

    // Dynamically calculate the center LED of the last row using AppConfig dimensions
    const size_t last_row = Config::MATRIX_HEIGHT - 1;
    const size_t center_col = Config::MATRIX_WIDTH / 2;
    const size_t target_idx = last_row * Config::MATRIX_WIDTH + center_col;

    if (target_idx >= (width * height)) return;

    // Smooth breathing curve (ranges from 0.15 to 1.0)
    float breathing_factor = 0.5f + 0.5f * std::sin(m_pulse_phase);

    // Strict 10% maximum brightness cap
    const float max_brightness_cap = 0.09f; 
    float final_intensity = breathing_factor * max_brightness_cap;

    // Calculate progression through the 10-second window (0.0 to 1.0)
    float progress = static_cast<float>(m_elapsed_ms) / static_cast<float>(m_duration_ms);
    progress = std::clamp(progress, 0.0f, 1.0f);

    // Color palette definitions starting with the requested deep red/ember tone
    float base_r = 220.0f;
    float base_g = 15.0f;
    float base_b = 0.0f;

    if (progress < 0.25f) {
        // Phase 1: Custom Deep Red / Ember (R:220, G:15, B:0)
        base_r = 220.0f; base_g = 15.0f; base_b = 0.0f;
    } else if (progress < 0.50f) {
        // Phase 2: Transition to White (R:220, G:220, B:220)
        float t = (progress - 0.25f) / 0.25f;
        base_r = 220.0f * (1.0f - t) + 220.0f * t;
        base_g = 15.0f  * (1.0f - t) + 220.0f * t;
        base_b = 0.0f   * (1.0f - t) + 220.0f * t;
    } else if (progress < 0.75f) {
        // Phase 3: Transition to Aurora Green (R:40, G:255, B:140)
        float t = (progress - 0.50f) / 0.25f;
        base_r = 220.0f * (1.0f - t) + 40.0f * t;
        base_g = 220.0f * (1.0f - t) + 255.0f * t;
        base_b = 220.0f * (1.0f - t) + 140.0f * t;
    } else {
        // Phase 4: Transition to Warm Yellow (R:255, G:220, B:50)
        float t = (progress - 0.75f) / 0.25f;
        base_r = 40.0f  * (1.0f - t) + 255.0f * t;
        base_g = 255.0f * (1.0f - t) + 220.0f * t;
        base_b = 140.0f * (1.0f - t) + 50.0f * t;
    }

    // Apply color and low-intensity breathing to the dynamically computed center LED of the last row
    size_t idx = target_idx * 3;
    buffer[idx + 0] = static_cast<uint8_t>(base_r * final_intensity);
    buffer[idx + 1] = static_cast<uint8_t>(base_g * final_intensity);
    buffer[idx + 2] = static_cast<uint8_t>(base_b * final_intensity);
}