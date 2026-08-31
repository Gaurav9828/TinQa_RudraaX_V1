#ifndef IEFFECT_H
#define IEFFECT_H

#include <cstdint>
#include <cstddef>
#include "config/AppConfig.h"

class IEffect {
public:
    virtual ~IEffect() = default;

    // Called once when switching to this effect
    virtual void init() = 0;

    // Called every frame to advance animation physics/state
    virtual void update(uint32_t delta_ms) = 0;

    // Renders the current state into an RGB buffer (size = WIDTH * HEIGHT * 3)
    virtual void render(uint8_t* buffer, size_t width, size_t height) = 0;

    // Get human-readable name of the effect
    virtual const char* getName() const = 0;
};

#endif // IEFFECT_H