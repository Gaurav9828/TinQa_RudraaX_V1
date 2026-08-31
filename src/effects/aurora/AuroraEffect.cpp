#include "effects/aurora/AuroraEffect.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

AuroraEffect::AuroraEffect() 
    : time(0.0f), 
      currentTheme(AuroraTheme::EMERALD_CANOPY), 
      targetTheme(AuroraTheme::VIOLET_TEMPEST),
      transitionProgress(1.0f), 
      themeTimer(0.0f), 
      themeDuration(45.0f),
      dirX1(1.0f), dirY1(0.5f),
      dirX2(-0.7f), dirY2(1.2f),
      waveSpeedMult(1.0f) {}

void AuroraEffect::init() {
    time = 0.0f;
    themeTimer = 0.0f;
    transitionProgress = 1.0f;
    selectRandomThemePair();
}

void AuroraEffect::selectRandomThemePair() {
    currentTheme = targetTheme;
    
    // Pick a new random theme different from the current one
    int nextId = std::rand() % static_cast<int>(AuroraTheme::COUNT);
    while (static_cast<AuroraTheme>(nextId) == currentTheme) {
        nextId = std::rand() % static_cast<int>(AuroraTheme::COUNT);
    }
    targetTheme = static_cast<AuroraTheme>(nextId);
    
    transitionProgress = 0.0f;
    themeTimer = 0.0f;
    
    // Random theme duration between 30 and 90 seconds
    themeDuration = 30.0f + static_cast<float>(std::rand() % 6000) / 100.0f;
    
    updateMotionParameters();
}

void AuroraEffect::updateMotionParameters() {
    // Generate new wave motion vectors and speeds upon theme change
    float angle1 = static_cast<float>(std::rand() % 360) * 0.0174533f;
    float angle2 = static_cast<float>(std::rand() % 360) * 0.0174533f;
    
    dirX1 = std::cos(angle1);
    dirY1 = std::sin(angle1);
    dirX2 = std::cos(angle2);
    dirY2 = std::sin(angle2);
    
    // Variable wave speed modifier (0.6x to 1.8x speed)
    waveSpeedMult = 0.6f + static_cast<float>(std::rand() % 120) / 100.0f;
}

void AuroraEffect::update(uint32_t delta_ms) {
    float delta_sec = static_cast<float>(delta_ms) * 0.001f;
    time += delta_sec * waveSpeedMult;
    themeTimer += delta_sec;

    // Trigger dynamic palette transition when timer expires
    if (themeTimer >= themeDuration) {
        selectRandomThemePair();
    }

    // Smooth transition over 10 seconds
    if (transitionProgress < 1.0f) {
        transitionProgress += delta_sec / 10.0f;
        if (transitionProgress > 1.0f) {
            transitionProgress = 1.0f;
        }
    }
}

void AuroraEffect::getThemeColor(AuroraTheme theme, float normY, float &r, float &g, float &b) {
    switch (theme) {
        case AuroraTheme::EMERALD_CANOPY: // Image 1 & 5 inspired
            if (normY < 0.4f) {
                float t = normY / 0.4f;
                r = 0.02f; g = 0.85f + 0.15f * t; b = 0.25f;
            } else {
                float t = (normY - 0.4f) / 0.6f;
                r = 0.05f + 0.30f * t; g = 1.0f - 0.70f * t; b = 0.3f + 0.2f * t;
            }
            break;

        case AuroraTheme::LIME_HORIZON: // Image 2 inspired
            if (normY < 0.5f) {
                float t = normY / 0.5f;
                r = 0.65f * t; g = 0.90f + 0.10f * t; b = 0.10f;
            } else {
                float t = (normY - 0.5f) / 0.5f;
                r = 0.65f - 0.45f * t; g = 1.0f - 0.50f * t; b = 0.10f + 0.40f * t;
            }
            break;

        case AuroraTheme::CRIMSON_CORONA: // Image 3 inspired
            if (normY < 0.30f) {
                r = 0.10f; g = 0.90f; b = 0.40f;
            } else if (normY < 0.75f) {
                float t = (normY - 0.30f) / 0.45f;
                r = 0.10f + 0.85f * t; g = 0.90f - 0.60f * t; b = 0.40f + 0.30f * t;
            } else {
                float t = (normY - 0.75f) / 0.25f;
                r = 0.95f; g = 0.30f - 0.25f * t; b = 0.70f - 0.20f * t;
            }
            break;

        case AuroraTheme::VIOLET_TEMPEST: // Image 4 inspired
            if (normY < 0.35f) {
                float t = normY / 0.35f;
                r = 0.8f * t; g = 0.95f; b = 0.9f + 0.1f * t;
            } else if (normY < 0.70f) {
                float t = (normY - 0.35f) / 0.35f;
                r = 0.8f + 0.15f * t; g = 0.95f - 0.75f * t; b = 1.0f;
            } else {
                float t = (normY - 0.70f) / 0.30f;
                r = 0.95f - 0.4f * t; g = 0.20f - 0.15f * t; b = 1.0f - 0.3f * t;
            }
            break;

        case AuroraTheme::CLASSIC_ARC: // Image 5 focused
            r = 0.05f;
            g = 0.95f + 0.05f * std::sin(normY * 3.14159f);
            b = 0.15f + 0.35f * normY;
            break;

        case AuroraTheme::POLARIS_DUSK: // Image 6 inspired
        default:
            if (normY < 0.45f) {
                float t = normY / 0.45f;
                r = 0.0f; g = 0.80f + 0.15f * t; b = 0.50f + 0.20f * t;
            } else {
                float t = (normY - 0.45f) / 0.55f;
                r = 0.40f * t; g = 0.95f - 0.65f * t; b = 0.70f + 0.25f * t;
            }
            break;
    }
}

void AuroraEffect::getBlendedColor(float normY, float intensity, uint8_t &r, uint8_t &g, uint8_t &b) {
    float r1, g1, b1;
    float r2, g2, b2;

    getThemeColor(currentTheme, normY, r1, g1, b1);
    getThemeColor(targetTheme, normY, r2, g2, b2);

    // Smooth Cosine Interpolation
    float t = (1.0f - std::cos(transitionProgress * 3.14159265f)) * 0.5f;

    float finalR = (r1 * (1.0f - t) + r2 * t) * intensity;
    float finalG = (g1 * (1.0f - t) + g2 * t) * intensity;
    float finalB = (b1 * (1.0f - t) + b2 * t) * intensity;

    r = static_cast<uint8_t>(std::clamp(finalR, 0.0f, 1.0f) * 255.0f);
    g = static_cast<uint8_t>(std::clamp(finalG, 0.0f, 1.0f) * 255.0f);
    b = static_cast<uint8_t>(std::clamp(finalB, 0.0f, 1.0f) * 255.0f);
}

void AuroraEffect::render(uint8_t* buffer, size_t width, size_t height) {
    if (!buffer || width == 0 || height == 0) return;

    const float GRID_W = static_cast<float>(width);
    const float GRID_H = static_cast<float>(height);

    for (size_t y = 0; y < height; ++y) {
        float normY = 1.0f - (static_cast<float>(y) / (GRID_H - 1.0f));

        for (size_t x = 0; x < width; ++x) {
            float normX = static_cast<float>(x) / (GRID_W - 1.0f);

            // Dynamic Multi-Directional Wave Ribbons
            float proj1 = normX * dirX1 + normY * dirY1;
            float proj2 = normX * dirX2 + normY * dirY2;

            float wave1 = std::sin(proj1 * 4.5f - time * 1.1f);
            float wave2 = std::cos(proj2 * 6.0f + time * 0.8f);
            float wave3 = std::sin((normX + normY) * 2.5f - time * 0.4f);

            float curtainCenter = 0.45f + (wave1 * 0.16f) + (wave2 * 0.09f) + (wave3 * 0.05f);
            float distFromCenter = std::abs(normY - curtainCenter);

            // Dynamic soft organic curtain drop-off
            float curtainBody = std::exp(-distFromCenter * distFromCenter * 9.5f);

            // Variable Ray Streaks
            float rayAngle = (normX * dirX1 + normY * dirY2) * 14.0f + time * 1.6f;
            float verticalRays = std::pow(std::sin(rayAngle) * 0.5f + 0.5f, 3.5f);

            // Combined Field Intensity
            float totalIntensity = curtainBody * (0.50f + 0.50f * verticalRays);
            totalIntensity = std::clamp(totalIntensity, 0.0f, 1.0f);

            uint8_t r = 0, g = 0, b = 0;
            if (totalIntensity > 0.015f) {
                getBlendedColor(normY, totalIntensity, r, g, b);
            }

            size_t idx = (y * width + x) * 3;
            buffer[idx + 0] = r;
            buffer[idx + 1] = g;
            buffer[idx + 2] = b;
        }
    }
}