#include "effects/aurora/AuroraEffect.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr float PI = 3.14159265f;

inline float smoothStep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float smootherStep(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

inline float auroraEnvelope(float pos) {
    pos = std::clamp(pos, 0.0f, 1.0f);
    float fadeIn = smootherStep(smoothStep(0.0f, 0.22f, pos));
    float fadeOut = 1.0f - smootherStep(smoothStep(0.68f, 1.0f, pos));
    return fadeIn * fadeOut;
}

inline float auroraRibbon(float distance, float radius) {
    float x = std::clamp(std::abs(distance) / radius, 0.0f, 1.0f);
    return 1.0f - smootherStep(x);
}

inline float cycleHash(int cycle, int seed) {
    int n = cycle * 15731 + seed * 789221;
    n = (n << 13) ^ n;
    return (1.0f - static_cast<float>((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f) * 0.5f + 0.5f;
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline void getIsolatedColor(int colorMode, float wavePos, float &r, float &g, float &b) {
    float glow = 0.82f + 0.18f * smootherStep(1.0f - std::abs(wavePos - 0.5f) * 2.0f);

    if (colorMode == 0) {
        // Vibrant Pink
        r = 1.00f * glow;
        g = 0.12f * glow;
        b = 0.55f * glow;
    } else if (colorMode == 1) {
        // Soft Violet Pink / Magenta
        r = 0.85f * glow;
        g = 0.08f * glow;
        b = 0.80f * glow;
    } else if (colorMode == 2) {
        // Deep Emerald Green
        r = 0.02f * glow;
        g = 0.95f * glow;
        b = 0.22f * glow;
    } else {
        // Bright Lime Green
        r = 0.45f * glow;
        g = 1.00f * glow;
        b = 0.05f * glow;
    }
}

inline void getStreamCoords(int direction, float normX, float normY, float baseY, float wave, float waveAmplitude, float &pathPos, float &crossPos, float &snakeCenter) {
    direction = (direction % 8 + 8) % 8;

    if (direction == 0) {
        pathPos = normX;
        crossPos = normY;
        snakeCenter = baseY + wave * waveAmplitude;
    } else if (direction == 1) {
        pathPos = normY;
        crossPos = normX;
        snakeCenter = baseY + wave * waveAmplitude;
    } else if (direction == 2) {
        pathPos = 1.0f - normX;
        crossPos = normY;
        snakeCenter = baseY + wave * waveAmplitude;
    } else if (direction == 3) {
        pathPos = 1.0f - normY;
        crossPos = normX;
        snakeCenter = baseY + wave * waveAmplitude;
    } else if (direction == 4) {
        pathPos = (normX + normY) * 0.5f;
        crossPos = (normY - normX + 1.0f) * 0.5f;
        snakeCenter = 0.5f + wave * waveAmplitude;
    } else if (direction == 5) {
        pathPos = 1.0f - (normX + normY) * 0.5f;
        crossPos = (normY - normX + 1.0f) * 0.5f;
        snakeCenter = 0.5f + wave * waveAmplitude;
    } else if (direction == 6) {
        pathPos = (1.0f - normX + normY) * 0.5f;
        crossPos = (normX + normY) * 0.5f;
        snakeCenter = 0.5f + wave * waveAmplitude;
    } else {
        pathPos = (normX + 1.0f - normY) * 0.5f;
        crossPos = (normX + normY) * 0.5f;
        snakeCenter = 0.5f + wave * waveAmplitude;
    }
}

struct StreamState {
    float head;
    float tail;
    int direction;
    float baseY;
    float waveAmp;
    int colorMode;
    bool active;
};

inline StreamState calculateStream(int cycle, float cycleOffset, float progressOverTime) {
    StreamState state;
    float currentProgress = progressOverTime - static_cast<float>(cycle) + cycleOffset;

    state.head = currentProgress * 2.56f - 0.78f;
    state.tail = state.head - 0.78f;

    // Strict boundary life cycle: stream is active until tail is completely > 1.78f
    state.active = (state.head >= -0.78f && state.tail <= 1.78f);

    state.direction = (cycle % 8 + 8) % 8;
    state.baseY = 0.25f + cycleHash(cycle, 303) * 0.48f;
    state.waveAmp = 0.040f + cycleHash(cycle, 606) * 0.015f;

    state.colorMode = -1;
    if (cycleHash(cycle, 909) < 0.50f) {
        state.colorMode = static_cast<int>(cycleHash(cycle, 707) * 4.0f);
        state.colorMode = std::clamp(state.colorMode, 0, 3);
    }

    return state;
}
}

AuroraEffect::AuroraEffect()
    : time(0.0f),
      currentTheme(AuroraTheme::EMERALD_CANOPY),
      targetTheme(AuroraTheme::VIOLET_TEMPEST),
      transitionProgress(1.0f),
      themeTimer(0.0f),
      themeDuration(45.0f),
      dirX1(1.0f),
      dirY1(0.15f),
      dirX2(-0.7f),
      dirY2(1.2f),
      waveSpeedMult(0.270f) {}

void AuroraEffect::init() {
    time = 0.0f;
    themeTimer = 0.0f;
    transitionProgress = 1.0f;
    selectRandomThemePair();
}

void AuroraEffect::selectRandomThemePair() {
    currentTheme = targetTheme;

    int nextId = std::rand() % static_cast<int>(AuroraTheme::COUNT);

    while (static_cast<AuroraTheme>(nextId) == currentTheme)
        nextId = std::rand() % static_cast<int>(AuroraTheme::COUNT);

    targetTheme = static_cast<AuroraTheme>(nextId);
    transitionProgress = 0.0f;
    themeTimer = 0.0f;
    themeDuration = 45.0f + static_cast<float>(std::rand() % 3000) / 100.0f;

    updateMotionParameters();
}

void AuroraEffect::updateMotionParameters() {
    constexpr float angle = 0.10f;

    dirX1 = std::cos(angle);
    dirY1 = std::sin(angle);
    waveSpeedMult = 0.270f + static_cast<float>(std::rand() % 12) / 100.0f;
}

void AuroraEffect::update(uint32_t delta_ms) {
    float dt = static_cast<float>(delta_ms) * 0.001f;

    time += dt * waveSpeedMult;
    themeTimer += dt;

    if (themeTimer >= themeDuration)
        selectRandomThemePair();

    if (transitionProgress < 1.0f) {
        transitionProgress += dt / 14.0f;
        transitionProgress = std::min(transitionProgress, 1.0f);
    }
}

void AuroraEffect::getThemeColor(AuroraTheme theme, float wavePos, float &r, float &g, float &b) {
    wavePos = std::clamp(wavePos, 0.0f, 1.0f);

    if (theme == AuroraTheme::VIOLET_TEMPEST) {
        if (wavePos < 0.55f) {
            float t = smootherStep(wavePos / 0.55f);
            r = lerp(0.85f, 0.20f, t);
            g = lerp(0.10f, 0.08f, t);
            b = lerp(0.70f, 0.62f, t);
        } else {
            float t = smootherStep((wavePos - 0.55f) / 0.45f);
            r = lerp(0.20f, 0.015f, t);
            g = lerp(0.08f, 0.85f, t);
            b = lerp(0.62f, 0.20f, t);
        }
        return;
    }

    if (wavePos < 0.55f) {
        float t = smootherStep(wavePos / 0.55f);
        r = lerp(0.90f, 0.05f, t);
        g = lerp(0.15f, 0.85f, t);
        b = lerp(0.50f, 0.20f, t);
    } else {
        float t = smootherStep((wavePos - 0.55f) / 0.45f);
        r = lerp(0.05f, 0.30f, t);
        g = lerp(0.85f, 0.95f, t);
        b = lerp(0.20f, 0.10f, t);
    }
}

void AuroraEffect::getBlendedColor(float wavePos, float intensity, uint8_t &r, uint8_t &g, uint8_t &b) {
    float r1, g1, b1, r2, g2, b2;

    getThemeColor(currentTheme, wavePos, r1, g1, b1);
    getThemeColor(targetTheme, wavePos, r2, g2, b2);

    float blend = (1.0f - std::cos(transitionProgress * PI)) * 0.5f;
    blend = smootherStep(blend);

    float finalR = lerp(r1, r2, blend);
    float finalG = lerp(g1, g2, blend);
    float finalB = lerp(b1, b2, blend);

    constexpr float BRIGHTNESS = 0.44f;

    finalR *= intensity * BRIGHTNESS;
    finalG *= intensity * BRIGHTNESS;
    finalB *= intensity * BRIGHTNESS;

    r = static_cast<uint8_t>(std::clamp(finalR, 0.0f, 1.0f) * 255.0f);
    g = static_cast<uint8_t>(std::clamp(finalG, 0.0f, 1.0f) * 255.0f);
    b = static_cast<uint8_t>(std::clamp(finalB, 0.0f, 1.0f) * 255.0f);
}

void AuroraEffect::render(uint8_t* buffer, size_t width, size_t height) {
    if (!buffer || width == 0 || height == 0)
        return;

    const float gridW = static_cast<float>(width);
    const float gridH = static_cast<float>(height);
    const float streamRadius = std::max(0.065f, 1.10f / std::min(gridW, gridH));

    float cycleTime = time * 0.12f;
    int currentCycle = static_cast<int>(std::floor(cycleTime));

    // Evaluate up to 3 active stream candidates across cycle boundaries
    // 1. Stream from previous cycle (finishing its tail)
    StreamState prevStream = calculateStream(currentCycle - 1, 0.0f, cycleTime);
    // 2. Stream from current cycle
    StreamState currStream = calculateStream(currentCycle, 0.0f, cycleTime);
    // 3. Secondary stream from current cycle (mid-section start)
    bool hasSecondary = (cycleHash(currentCycle, 444) < 0.65f);
    StreamState secStream = calculateStream(currentCycle + 3, -0.30f, cycleTime);
    if (!hasSecondary) secStream.active = false;

    // Check if any stream is actively emitting light on screen
    bool anyStreamActive = prevStream.active || currStream.active || secStream.active;

    // Pause ONLY when zero active streams are on screen
    if (!anyStreamActive) {
        float pauseChance = cycleHash(currentCycle, 111);
        if (pauseChance < 0.45f) {
            std::fill(buffer, buffer + (width * height * 3), 0);
            return;
        }
    }

    for (size_t y = 0; y < height; ++y) {
        float normY = height > 1 ? static_cast<float>(y) / (gridH - 1.0f) : 0.0f;

        for (size_t x = 0; x < width; ++x) {
            float normX = width > 1 ? static_cast<float>(x) / (gridW - 1.0f) : 0.0f;

            float wave = std::sin(normX * PI * 2.0f * 1.35f + time * 0.336f) * 0.70f +
                        std::sin(normX * PI * 2.0f * 0.62f - time * 0.168f + 1.7f) * 0.30f;

            float totalR = 0.0f;
            float totalG = 0.0f;
            float totalB = 0.0f;

            const StreamState* streams[3] = { &prevStream, &currStream, &secStream };
            float brightnessScale[3] = { 1.0f, 1.0f, 0.30f };

            for (int i = 0; i < 3; ++i) {
                const StreamState& s = *streams[i];
                if (!s.active) continue;

                float pathPos, crossPos, snakeCenter;
                getStreamCoords(s.direction, normX, normY, s.baseY, wave, s.waveAmp, pathPos, crossPos, snakeCenter);

                if (pathPos >= s.tail && pathPos <= s.head) {
                    float pos = (pathPos - s.tail) / (s.head - s.tail);
                    float intensity = auroraRibbon(crossPos - snakeCenter, streamRadius) * auroraEnvelope(pos) * brightnessScale[i];

                    if (intensity > 0.001f) {
                        uint8_t r = 0, g = 0, b = 0;
                        if (s.colorMode >= 0) {
                            float cr, cg, cb;
                            getIsolatedColor(s.colorMode, pathPos, cr, cg, cb);
                            cr *= intensity * 0.44f; cg *= intensity * 0.44f; cb *= intensity * 0.44f;
                            r = static_cast<uint8_t>(std::clamp(cr, 0.0f, 1.0f) * 255.0f);
                            g = static_cast<uint8_t>(std::clamp(cg, 0.0f, 1.0f) * 255.0f);
                            b = static_cast<uint8_t>(std::clamp(cb, 0.0f, 1.0f) * 255.0f);
                        } else {
                            getBlendedColor(pathPos, intensity, r, g, b);
                        }

                        totalR += r;
                        totalG += g;
                        totalB += b;
                    }
                }
            }

            size_t index = (y * width + x) * 3;
            buffer[index]     = static_cast<uint8_t>(std::min(255.0f, totalR));
            buffer[index + 1] = static_cast<uint8_t>(std::min(255.0f, totalG));
            buffer[index + 2] = static_cast<uint8_t>(std::min(255.0f, totalB));
        }
    }
}