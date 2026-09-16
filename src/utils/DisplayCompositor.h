#pragma once

#include <cstdint>
#include <algorithm>
#include <cstring>

class DisplayCompositor {
public:
    /**
     * Scales the raw rendered frame buffer by the master ambient brightness factor
     * and copies it safely into the display buffer for Core 1 transmission.
     */
    static void applyBrightnessAndPublish(
        const uint8_t* sourceBuffer, 
        uint8_t* destBuffer, 
        size_t bufferSize, 
        uint8_t brightnessVal
    ) {
        // Convert 0-255 brightness level to a normalized scaling factor (0.0f to 1.0f)
        float factor = static_cast<float>(brightnessVal) / 255.0f;

        for (size_t i = 0; i < bufferSize; i++) {
            // Apply scaling uniformly across RGB color channels
            destBuffer[i] = static_cast<uint8_t>(static_cast<float>(sourceBuffer[i]) * factor);
        }
    }

    /**
     * Helper to completely blank out both buffers (used during transitions and power-off).
     */
    static void clearBuffers(uint8_t* renderBuffer, uint8_t* displayBuffer, size_t bufferSize) {
        std::memset(renderBuffer, 0, bufferSize);
        std::memset(displayBuffer, 0, bufferSize);
    }
};