#pragma once

#include "../IEffect.h"
#include <cstdint>
#include <vector>

class StartupEffect : public IEffect {
public:
    StartupEffect();
    ~StartupEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Startup"; }

    bool isComplete() const { return m_is_complete; }

private:
    uint32_t m_elapsed_ms;
    uint32_t m_duration_ms; // Total 8000ms (8 seconds)
    bool m_is_complete;
    float m_sparkle_seed;
};