#include "effects/thunder/ThunderEffect.h"
#include "config/AppConfig.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

ThunderEffect::ThunderEffect()
    : stageTimerMs(0),
      nextEventDelayMs(300), 
      lastMediumStrikeTimeMs(0),
      lastMassStrikeTimeMs(0),
      ambientBrightness(0.2f),
      currentIntensity(ThunderIntensity::HIGH), // Default: HIGH
      isStrikeActive(false),
      isRapidBlinking(false),
      rapidBlinkCount(0),
      rapidBlinkTimerMs(0),
      rapidBlinkState(false),
      currentStrikeType(StrikeType::MICRO_FLASH),
      currentColorType(LightningColorType::PURE_WHITE),
      currentBrightness(1.0f),
      decayRate(0.12f),
      flashIntensity(0.0f),
      lastWidth(32),
      lastHeight(32),
      reFlickerCount(0) {}

void ThunderEffect::init() {
    stageTimerMs = 0;
    nextEventDelayMs = calculateNextDelay();
    lastMediumStrikeTimeMs = 0;
    lastMassStrikeTimeMs = 0;
    isStrikeActive = false;
    isRapidBlinking = false;
    flashIntensity = 0.0f;
    reFlickerCount = 0;
    boltPoints.clear();
    microPixelIndices.clear();
}

void ThunderEffect::setIntensity(ThunderIntensity intensity) {
    currentIntensity = intensity;
}

void ThunderEffect::setAmbientBrightness(float level) {
    ambientBrightness = std::clamp(level, 0.0f, 1.0f);
}

void ThunderEffect::updateAmbientFromLux(float lux) {
    float clampedLux = std::clamp(lux, Config::AmbientSensor::MIN_LUX, Config::AmbientSensor::MAX_LUX);
    float logLux = std::log10(clampedLux);
    float logMin = std::log10(Config::AmbientSensor::MIN_LUX);
    float logMax = std::log10(Config::AmbientSensor::MAX_LUX);

    float norm = (logLux - logMin) / (logMax - logMin);
    setAmbientBrightness(norm);
}

uint32_t ThunderEffect::getRandomRange(uint32_t min_val, uint32_t max_val) {
    if (min_val >= max_val) return min_val;
    uint32_t seed = to_us_since_boot(get_absolute_time());
    return min_val + (seed % (max_val - min_val + 1));
}

float ThunderEffect::getRandomFloat() {
    return static_cast<float>(getRandomRange(0, 10000)) / 10000.0f;
}

uint32_t ThunderEffect::calculateNextDelay() {
    switch (currentIntensity) {
        case ThunderIntensity::HIGH:
            // Strictly capped at max 4500ms (5 seconds boundary guaranteed)
            return getRandomRange(300, 4500);

        case ThunderIntensity::MID:
            // Max 9000ms pause (10 seconds boundary guaranteed)
            return getRandomRange(1500, 9000);

        case ThunderIntensity::NORMAL:
        default:
            // Max 18000ms pause (20 seconds boundary guaranteed)
            return getRandomRange(4000, 18000);
    }
}

void ThunderEffect::getLightningRGB(LightningColorType colorType, float intensity, uint8_t &r, uint8_t &g, uint8_t &b) {
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    float baseR = 255.0f, baseG = 255.0f, baseB = 255.0f;

    switch (colorType) {
        case LightningColorType::ELECTRIC_BLUE_WHITE:
            baseR = 200.0f; baseG = 225.0f; baseB = 255.0f;
            break;
        case LightningColorType::WARM_GOLDEN_WHITE:
            baseR = 255.0f; baseG = 240.0f; baseB = 180.0f;
            break;
        case LightningColorType::PURE_WHITE:
        default:
            baseR = 255.0f; baseG = 255.0f; baseB = 255.0f;
            break;
    }

    r = static_cast<uint8_t>(baseR * intensity);
    g = static_cast<uint8_t>(baseG * intensity);
    b = static_cast<uint8_t>(baseB * intensity);
}

void ThunderEffect::generateZigZagPath(bool vertical, float startPos) {
    boltPoints.clear();
    int steps = 5 + getRandomRange(0, 3);
    
    float currX = vertical ? startPos : 0.0f;
    float currY = vertical ? 0.0f : startPos;

    boltPoints.push_back({currX, currY});

    for (int i = 1; i <= steps; ++i) {
        float progress = static_cast<float>(i) / static_cast<float>(steps);
        if (vertical) {
            currY = progress * 0.85f;
            float jitter = (getRandomFloat() - 0.5f) * 0.16f;
            currX = std::clamp(startPos + jitter, 0.05f, 0.95f);
        } else {
            currX = progress;
            float jitter = (getRandomFloat() - 0.5f) * 0.16f;
            currY = std::clamp(startPos + jitter, 0.05f, 0.75f);
        }
        boltPoints.push_back({currX, currY});
    }
}

void ThunderEffect::generateForkedPath() {
    boltPoints.clear();
    float startX = 0.25f + (getRandomFloat() * 0.5f);
    generateZigZagPath(true, startX);

    int forkStartIndex = getRandomRange(2, 3);
    if (forkStartIndex < static_cast<int>(boltPoints.size())) {
        Point2D forkOrigin = boltPoints[forkStartIndex];
        float forkX = forkOrigin.x;
        float forkY = forkOrigin.y;

        for (int i = 0; i < 4; ++i) {
            forkY += 0.07f;
            forkX += (getRandomFloat() > 0.5f ? 0.06f : -0.06f);
            if (forkY > 0.9f || forkX < 0.0f || forkX > 1.0f) break;
            boltPoints.push_back({forkX, forkY});
        }
    }
}

void ThunderEffect::generateMicroFlash(size_t width, size_t height) {
    currentStrikeType = StrikeType::MICRO_FLASH;
    microPixelIndices.clear();

    // Strictly 3 to 4 LEDs total
    int ledCount = getRandomRange(3, 4);
    size_t totalLeds = width * height;
    if (totalLeds == 0) return;

    // Pick a random anchor pixel on the visible matrix
    size_t centerIdx = getRandomRange(0, totalLeds - 1);
    microPixelIndices.push_back(centerIdx);

    for (int i = 1; i < ledCount; ++i) {
        int offset = (getRandomRange(0, 1) == 0 ? 1 : -1) * static_cast<int>(getRandomRange(1, width > 1 ? width : 2));
        size_t neighbor = std::clamp(static_cast<int>(centerIdx) + offset, 0, static_cast<int>(totalLeds - 1));
        microPixelIndices.push_back(neighbor);
    }

    decayRate = 0.18f + (getRandomFloat() * 0.10f); // Fast decay for micro pops
    reFlickerCount = (getRandomFloat() > 0.70f) ? 1 : 0;
    nextEventDelayMs = calculateNextDelay();
}

void ThunderEffect::generateMediumStrike() {
    currentStrikeType = StrikeType::MEDIUM_STRIKE;
    lastMediumStrikeTimeMs = to_ms_since_boot(get_absolute_time());

    if (getRandomFloat() > 0.50f) {
        generateZigZagPath(true, 0.20f + (getRandomFloat() * 0.60f));
    } else {
        generateForkedPath();
    }

    decayRate = 0.07f + (getRandomFloat() * 0.06f);
    reFlickerCount = getRandomRange(1, 2);
    nextEventDelayMs = calculateNextDelay();
}

void ThunderEffect::generateMassStrike() {
    currentStrikeType = StrikeType::MASS_STRIKE;
    lastMassStrikeTimeMs = to_ms_since_boot(get_absolute_time());

    if (getRandomFloat() < 0.35f) {
        isRapidBlinking = true;
        rapidBlinkCount = getRandomRange(4, 7);
        rapidBlinkTimerMs = 0;
        rapidBlinkState = true;
    } else {
        generateZigZagPath(true, 0.5f);
    }

    decayRate = 0.04f + (getRandomFloat() * 0.04f);
    reFlickerCount = getRandomRange(2, 3);
    nextEventDelayMs = calculateNextDelay();
}

void ThunderEffect::generateNextStrike() {
    uint32_t nowMs = to_ms_since_boot(get_absolute_time());
    float roll = getRandomFloat();

    // Color variation
    float colorSeed = getRandomFloat();
    if (colorSeed < 0.65f) currentColorType = LightningColorType::PURE_WHITE;
    else if (colorSeed < 0.85f) currentColorType = LightningColorType::ELECTRIC_BLUE_WHITE;
    else currentColorType = LightningColorType::WARM_GOLDEN_WHITE;

    currentBrightness = 0.85f + (getRandomFloat() * 0.15f);
    flashIntensity = 1.0f;
    isStrikeActive = true;
    isRapidBlinking = false;

    // Intensity-dependent cooldown limits for medium/mass strikes
    uint32_t massCooldown = (currentIntensity == ThunderIntensity::HIGH) ? 12000 : 25000;
    uint32_t medCooldown  = (currentIntensity == ThunderIntensity::HIGH) ? 6000  : 15000;

    bool canDoMass = (nowMs - lastMassStrikeTimeMs >= massCooldown);
    bool canDoMedium = (nowMs - lastMediumStrikeTimeMs >= medCooldown);

    if (roll < 0.68f || (!canDoMass && !canDoMedium)) {
        // 65-70% of the time: Micro-Flashes (3-4 LEDs)
        generateMicroFlash(lastWidth, lastHeight);
    } else if (roll < 0.90f && canDoMedium) {
        generateMediumStrike();
    } else if (canDoMass) {
        generateMassStrike();
    } else {
        generateMicroFlash(lastWidth, lastHeight);
    }
}

void ThunderEffect::update(uint32_t delta_ms) {
    if (!isStrikeActive) {
        stageTimerMs += delta_ms;
        if (stageTimerMs >= nextEventDelayMs) {
            stageTimerMs = 0;
            generateNextStrike();
        }
    } else {
        if (isRapidBlinking) {
            rapidBlinkTimerMs += delta_ms;
            if (rapidBlinkTimerMs >= 35) {
                rapidBlinkTimerMs = 0;
                rapidBlinkState = !rapidBlinkState;
                rapidBlinkCount--;

                if (rapidBlinkCount <= 0) {
                    isRapidBlinking = false;
                    isStrikeActive = false;
                    stageTimerMs = 0;
                    nextEventDelayMs = calculateNextDelay();
                }
            }
            return;
        }

        float deltaFactor = static_cast<float>(delta_ms) / 16.66f;
        flashIntensity -= (decayRate * deltaFactor);

        if (flashIntensity <= 0.0f) {
            if (reFlickerCount > 0) {
                reFlickerCount--;
                flashIntensity = 0.65f + (getRandomFloat() * 0.30f);
            } else {
                flashIntensity = 0.0f;
                isStrikeActive = false;
                stageTimerMs = 0;
                nextEventDelayMs = calculateNextDelay();
            }
        }
    }
}

void ThunderEffect::render(uint8_t* buffer, size_t width, size_t height) {
    // Cache matrix width/height for dynamic micro flash generation
    lastWidth = width;
    lastHeight = height;

    std::fill(buffer, buffer + (width * height * 3), 0);

    if (!isStrikeActive) return;

    if (isRapidBlinking) {
        if (rapidBlinkState) {
            uint8_t capVal = static_cast<uint8_t>(255.0f * (ambientBrightness >= 0.80f ? 1.0f : 0.60f));
            std::fill(buffer, buffer + (width * height * 3), capVal);
        }
        return;
    }

    if (flashIntensity <= 0.01f) return;

    float effectiveIntensity = flashIntensity * currentBrightness;
    uint8_t r = 0, g = 0, b = 0;
    getLightningRGB(currentColorType, effectiveIntensity, r, g, b);

    // 1. Render Micro Flash (3 - 4 LEDs max)
    if (currentStrikeType == StrikeType::MICRO_FLASH) {
        size_t totalLeds = width * height;
        for (size_t idx : microPixelIndices) {
            if (idx < totalLeds) {
                buffer[idx * 3 + 0] = r;
                buffer[idx * 3 + 1] = g;
                buffer[idx * 3 + 2] = b;
            }
        }
        return;
    }

    // 2. Render Mass Panel Flash (50%+ coverage)
    if (currentStrikeType == StrikeType::MASS_STRIKE) {
        for (size_t i = 0; i < width * height; ++i) {
            if (getRandomFloat() <= 0.65f) {
                buffer[i * 3 + 0] = r;
                buffer[i * 3 + 1] = g;
                buffer[i * 3 + 2] = b;
            }
        }
    }

    // 3. Render Medium / Vector paths (16 to 50 LEDs stroke)
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

                    if (width >= 16 && x0 + 1 < static_cast<int>(width)) {
                        buffer[(y0 * width + (x0 + 1)) * 3 + 0] = static_cast<uint8_t>(r * 0.3f);
                        buffer[(y0 * width + (x0 + 1)) * 3 + 1] = static_cast<uint8_t>(g * 0.3f);
                        buffer[(y0 * width + (x0 + 1)) * 3 + 2] = static_cast<uint8_t>(b * 0.3f);
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