#include "effects/thunder/ThunderEffect.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

ThunderEffect::ThunderEffect()
    : stageTimerMs(0),
      nextEventDelayMs(800),
      isStrikeActive(false),
      isRapidBlinking(false),
      rapidBlinkCount(0),
      rapidBlinkTimerMs(0),
      rapidBlinkState(false),
      activePatternId(0),
      currentColorType(LightningColorType::PURE_WHITE),
      currentBrightness(1.0f),
      decayRate(0.08f),
      flashIntensity(0.0f),
      reFlickerCount(0) {}

void ThunderEffect::init() {
    stageTimerMs = 0;
    nextEventDelayMs = getRandomRange(400, 1800);
    isStrikeActive = false;
    isRapidBlinking = false;
    flashIntensity = 0.0f;
    reFlickerCount = 0;
    boltPoints.clear();
}

uint32_t ThunderEffect::getRandomRange(uint32_t min_val, uint32_t max_val) {
    if (min_val >= max_val) return min_val;
    uint32_t seed = to_us_since_boot(get_absolute_time());
    return min_val + (seed % (max_val - min_val + 1));
}

float ThunderEffect::getRandomFloat() {
    return static_cast<float>(getRandomRange(0, 10000)) / 10000.0f;
}

void ThunderEffect::getLightningRGB(LightningColorType colorType, float intensity, uint8_t &r, uint8_t &g, uint8_t &b) {
    intensity = std::clamp(intensity, 0.0f, 1.0f);

    float baseR = 255.0f;
    float baseG = 255.0f;
    float baseB = 255.0f;

    switch (colorType) {
        case LightningColorType::ELECTRIC_BLUE_WHITE:
            // High blue-white tint, green suppressed
            baseR = 220.0f;
            baseG = 235.0f;
            baseB = 255.0f;
            break;
        case LightningColorType::WARM_GOLDEN_WHITE:
            // Golden-tinted strike, no green predominance
            baseR = 255.0f;
            baseG = 245.0f;
            baseB = 200.0f;
            break;
        case LightningColorType::PURE_WHITE:
        default:
            baseR = 255.0f;
            baseG = 255.0f;
            baseB = 255.0f;
            break;
    }

    r = static_cast<uint8_t>(baseR * intensity);
    g = static_cast<uint8_t>(baseG * intensity);
    b = static_cast<uint8_t>(baseB * intensity);
}

void ThunderEffect::generateZigZagPath(bool vertical, float startPos) {
    boltPoints.clear();
    int steps = 10 + getRandomRange(0, 8);
    
    float currX = vertical ? startPos : 0.0f;
    float currY = vertical ? 0.0f : startPos;

    boltPoints.push_back({currX, currY});

    for (int i = 1; i <= steps; ++i) {
        float progress = static_cast<float>(i) / static_cast<float>(steps);
        if (vertical) {
            currY = progress;
            float jitter = (getRandomFloat() - 0.5f) * 0.22f;
            currX = std::clamp(startPos + jitter, 0.02f, 0.98f);
        } else {
            currX = progress;
            float jitter = (getRandomFloat() - 0.5f) * 0.22f;
            currY = std::clamp(startPos + jitter, 0.02f, 0.98f);
        }
        boltPoints.push_back({currX, currY});
    }
}

void ThunderEffect::generateForkedPath() {
    boltPoints.clear();
    float startX = 0.2f + (getRandomFloat() * 0.6f);
    generateZigZagPath(true, startX);

    int forkStartIndex = getRandomRange(3, 6);
    if (forkStartIndex < static_cast<int>(boltPoints.size())) {
        Point2D forkOrigin = boltPoints[forkStartIndex];
        float forkX = forkOrigin.x;
        float forkY = forkOrigin.y;

        for (int i = 0; i < 5; ++i) {
            forkY += 0.09f;
            forkX += (getRandomFloat() > 0.5f ? 0.07f : -0.07f);
            if (forkY > 1.0f || forkX < 0.0f || forkX > 1.0f) break;
            boltPoints.push_back({forkX, forkY});
        }
    }
}

void ThunderEffect::executePattern(int patternId) {
    // 1. Color Distribution: 70% Pure White, 20% Electric Blue-White, 10% Golden-White
    float colorSeed = getRandomFloat();
    if (colorSeed < 0.70f) {
        currentColorType = LightningColorType::PURE_WHITE;
    } else if (colorSeed < 0.90f) {
        currentColorType = LightningColorType::ELECTRIC_BLUE_WHITE;
    } else {
        currentColorType = LightningColorType::WARM_GOLDEN_WHITE;
    }

    // 2. Brightness is high most of the time (85% to 100%)
    currentBrightness = (getRandomFloat() > 0.20f) ? 1.0f : (0.85f + getRandomFloat() * 0.15f);

    // 3. Strobe / Rapid Full-Screen Blink Check (25% chance of triggering rapid whole-panel blink)
    if (getRandomFloat() < 0.25f) {
        isRapidBlinking = true;
        rapidBlinkCount = getRandomRange(6, 14); // 6 to 14 rapid full-screen flashes
        rapidBlinkTimerMs = 0;
        rapidBlinkState = true;
        currentColorType = LightningColorType::PURE_WHITE;
        currentBrightness = 1.0f;
        isStrikeActive = true;
        return;
    }

    isRapidBlinking = false;
    decayRate = 0.04f + (getRandomFloat() * 0.10f);
    reFlickerCount = (getRandomFloat() > 0.50f) ? getRandomRange(1, 4) : 0;

    flashIntensity = 1.0f;
    isStrikeActive = true;

    // 4. Generate Geometry based on pattern IDs
    switch (patternId) {
        case 10:
            generateZigZagPath(true, 0.5f);
            break;
        case 11:
            generateZigZagPath(true, 0.25f);
            break;
        case 12:
            generateZigZagPath(true, 0.75f);
            break;
        case 13:
            generateZigZagPath(false, 0.5f);
            break;
        case 14:
            generateZigZagPath(false, 0.2f);
            break;
        case 15:
            generateZigZagPath(false, 0.8f);
            break;
        case 16:
        case 17:
            generateForkedPath();
            break;
        default:
            boltPoints.clear();
            break;
    }
}

void ThunderEffect::generateNextStrike() {
    activePatternId = getRandomRange(0, 23);
    executePattern(activePatternId);
}

void ThunderEffect::update(uint32_t delta_ms) {
    if (!isStrikeActive) {
        stageTimerMs += delta_ms;
        if (stageTimerMs >= nextEventDelayMs) {
            stageTimerMs = 0;
            generateNextStrike();
        }
    } else {
        // Handling Rapid Full-Screen Blinks
        if (isRapidBlinking) {
            rapidBlinkTimerMs += delta_ms;
            // Toggle panel state every ~33ms (30 FPS Strobe rate)
            if (rapidBlinkTimerMs >= 33) {
                rapidBlinkTimerMs = 0;
                rapidBlinkState = !rapidBlinkState;
                rapidBlinkCount--;

                if (rapidBlinkCount <= 0) {
                    isRapidBlinking = false;
                    isStrikeActive = false;
                    stageTimerMs = 0;
                    nextEventDelayMs = getRandomRange(300, 1500);
                }
            }
            return;
        }

        // Standard decay for normal lightning strikes
        float deltaFactor = static_cast<float>(delta_ms) / 16.66f;
        flashIntensity -= (decayRate * deltaFactor);

        if (flashIntensity <= 0.0f) {
            if (reFlickerCount > 0) {
                reFlickerCount--;
                flashIntensity = 0.90f + (getRandomFloat() * 0.10f);
                decayRate = 0.06f + (getRandomFloat() * 0.08f);
            } else {
                flashIntensity = 0.0f;
                isStrikeActive = false;
                stageTimerMs = 0;
                nextEventDelayMs = getRandomRange(400, 2200);
            }
        }
    }
}

void ThunderEffect::render(uint8_t* buffer, size_t width, size_t height) {
    // Clear buffer to dark background
    std::fill(buffer, buffer + (width * height * 3), 0);

    if (!isStrikeActive) return;

    // 1. Render Rapid Full-Screen Strobe Flash
    if (isRapidBlinking) {
        if (rapidBlinkState) {
            // Fill entire buffer with 100% intense pure white
            std::fill(buffer, buffer + (width * height * 3), 255);
        }
        return;
    }

    if (flashIntensity <= 0.01f) return;

    // 2. Render Standard Strike Patterns
    float effectiveIntensity = flashIntensity * currentBrightness;
    uint8_t r = 0, g = 0, b = 0;
    getLightningRGB(currentColorType, effectiveIntensity, r, g, b);

    for (size_t y = 0; y < height; ++y) {
        float normY = static_cast<float>(y) / static_cast<float>(height);
        
        for (size_t x = 0; x < width; ++x) {
            float normX = static_cast<float>(x) / static_cast<float>(width);
            size_t idx = (y * width + x) * 3;
            bool illuminate = false;

            switch (activePatternId) {
                case 0:  // Full Screen Strobe Flash
                case 1:  // Slow Mountain Glow
                    illuminate = true;
                    break;
                case 2:  // Top-Half Flash
                    illuminate = (normY <= 0.5f);
                    break;
                case 3:  // Bottom-Half Flash
                    illuminate = (normY > 0.5f);
                    break;
                case 4:  // Left-Half Flash
                    illuminate = (normX <= 0.5f);
                    break;
                case 5:  // Right-Half Flash
                    illuminate = (normX > 0.5f);
                    break;
                case 6:  // TL Quadrant
                    illuminate = (normX <= 0.5f && normY <= 0.5f);
                    break;
                case 7:  // TR Quadrant
                    illuminate = (normX > 0.5f && normY <= 0.5f);
                    break;
                case 8:  // BL Quadrant
                    illuminate = (normX <= 0.5f && normY > 0.5f);
                    break;
                case 9:  // BR Quadrant
                    illuminate = (normX > 0.5f && normY > 0.5f);
                    break;
                case 18: // Horizon Line
                    illuminate = (std::abs(normY - 0.5f) < 0.08f);
                    break;
                case 19: // Double Horizon
                    illuminate = (std::abs(normY - 0.3f) < 0.06f || std::abs(normY - 0.7f) < 0.06f);
                    break;
                case 20: // Diagonal Slash TL -> BR
                    illuminate = (std::abs(normX - normY) < 0.12f);
                    break;
                case 21: // Diagonal Slash TR -> BL
                    illuminate = (std::abs((1.0f - normX) - normY) < 0.12f);
                    break;
                case 22: // Outer Frame Border Flash
                    illuminate = (normX < 0.1f || normX > 0.9f || normY < 0.1f || normY > 0.9f);
                    break;
                case 23: // Center Cloud Burst
                    illuminate = (std::sqrt(std::pow(normX - 0.5f, 2) + std::pow(normY - 0.5f, 2)) < 0.35f);
                    break;
                default:
                    illuminate = false;
                    break;
            }

            if (illuminate) {
                buffer[idx + 0] = r;
                buffer[idx + 1] = g;
                buffer[idx + 2] = b;
            }
        }
    }

    // Render Procedural Vector Bolt Lines
    if (!boltPoints.empty() && boltPoints.size() >= 2) {
        for (size_t i = 0; i < boltPoints.size() - 1; ++i) {
            int x0 = static_cast<int>(boltPoints[i].x * (width - 1));
            int y0 = static_cast<int>(boltPoints[i].y * (height - 1));
            int x1 = static_cast<int>(boltPoints[i + 1].x * (width - 1));
            int y1 = static_cast<int>(boltPoints[i + 1].y * (height - 1));

            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy, e2;

            while (true) {
                if (x0 >= 0 && x0 < static_cast<int>(width) && y0 >= 0 && y0 < static_cast<int>(height)) {
                    size_t bIdx = (y0 * width + x0) * 3;
                    buffer[bIdx + 0] = r;
                    buffer[bIdx + 1] = g;
                    buffer[bIdx + 2] = b;

                    // Core blooming
                    if (width >= 32 && x0 + 1 < static_cast<int>(width)) {
                        buffer[(y0 * width + (x0 + 1)) * 3 + 0] = static_cast<uint8_t>(r * 0.7f);
                        buffer[(y0 * width + (x0 + 1)) * 3 + 1] = static_cast<uint8_t>(g * 0.7f);
                        buffer[(y0 * width + (x0 + 1)) * 3 + 2] = static_cast<uint8_t>(b * 0.7f);
                    }
                }
                if (x0 == x1 && y0 == y1) break;
                e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        }
    }
}