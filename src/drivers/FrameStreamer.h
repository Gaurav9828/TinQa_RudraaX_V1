#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <chrono>
#include "../config/AppConfig.h"

class FrameStreamer {
private:
    const uint8_t width;
    const uint8_t height;
    uint8_t brightness; // 0-100 scale
    uint8_t currentFps;
    
    // FPS Tracking variables
    uint32_t frameCounter;
    std::chrono::steady_clock::time_point lastFpsCheck;

    std::vector<uint8_t> frameBuffer;

public:
    FrameStreamer()
        : width(Config::MATRIX_WIDTH), 
          height(Config::MATRIX_HEIGHT), 
          brightness(100),
          currentFps(0),
          frameCounter(0),
          lastFpsCheck(std::chrono::steady_clock::now()) {
        
        // Header structure: [0]: Width | [1]: Height | [2]: Brightness | [3]: FPS
        // Followed by RGB pixel payload: (width * height * 3 bytes)
        frameBuffer.resize(4 + (width * height * 3));
        
        // Lock static matrix dimensions into header
        frameBuffer[0] = width;
        frameBuffer[1] = height;
        frameBuffer[2] = brightness;
        frameBuffer[3] = currentFps;
    }

    void setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
        if (x >= width || y >= height) return;
        
        size_t index = 4 + ((y * width + x) * 3);
        frameBuffer[index]     = r;
        frameBuffer[index + 1] = g;
        frameBuffer[index + 2] = b;
    }

    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) {
        for (uint8_t y = 0; y < height; y++) {
            for (uint8_t x = 0; x < width; x++) {
                setPixel(x, y, r, g, b);
            }
        }
    }

    void setBrightness(uint8_t b) {
        brightness = (b > 100) ? 100 : b;
        frameBuffer[2] = brightness;
    }

    // Call this each time a new frame render is dispatched to calculate dynamic FPS
    void updateFps() {
        frameCounter++;
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsCheck).count();

        if (elapsedMs >= 1000) {
            currentFps = static_cast<uint8_t>((frameCounter * 1000) / elapsedMs);
            frameBuffer[3] = currentFps; // Update dynamic FPS in binary header
            
            frameCounter = 0;
            lastFpsCheck = now;
        }
    }

    const uint8_t* getBufferData() {
        updateFps(); // Keeps FPS updated on binary transmission
        return frameBuffer.data();
    }

    size_t getBufferSize() const {
        return frameBuffer.size();
    }

    uint8_t getWidth() const { return width; }
    uint8_t getHeight() const { return height; }
    uint8_t getFps() const { return currentFps; }
};