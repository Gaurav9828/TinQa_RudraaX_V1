#ifndef THUNDER_EFFECT_H
#define THUNDER_EFFECT_H

#include "effects/IEffect.h"
#include <vector>
#include <cstdint>
#include <cstddef>

enum class LightningColorType {
    PURE_WHITE,         // Pure 255, 255, 255
    ELECTRIC_BLUE_WHITE,// Ionized white with high blue saturation
    WARM_GOLDEN_WHITE   // Ground-strike warm white
};

enum class StrikeType {
    MICRO_FLASH,   // 3-4 LEDs max
    MEDIUM_STRIKE, // 16 to 50 LEDs
    MASS_STRIKE    // 50%+ of full panel
};

enum class ThunderIntensity {
    NORMAL, // Max pause: 20s between strikes
    MID,    // Max pause: 10s between strikes
    HIGH    // Max pause: 5s between strikes (Default)
};

struct Point2D {
    float x; // Normalised 0.0 -> 1.0
    float y; // Normalised 0.0 -> 1.0
};

class ThunderEffect : public IEffect {
public:
    ThunderEffect();
    ~ThunderEffect() override = default;

    void init() override;
    void update(uint32_t delta_ms) override;
    void render(uint8_t* buffer, size_t width, size_t height) override;
    const char* getName() const override { return "Thunder"; }

    // Ambient Lux Integration
    void setAmbientBrightness(float level); 
    void updateAmbientFromLux(float lux);   

    // Intensity Controls
    void setIntensity(ThunderIntensity intensity);
    ThunderIntensity getIntensity() const { return currentIntensity; }

private:
    uint32_t stageTimerMs;
    uint32_t nextEventDelayMs;
    uint32_t lastMediumStrikeTimeMs;
    uint32_t lastMassStrikeTimeMs;
    float ambientBrightness; // Normalized ambient light factor (0.0f to 1.0f)

    ThunderIntensity currentIntensity; // Active intensity setting

    bool isStrikeActive;

    // Rapid Strobe Mode
    bool isRapidBlinking;
    int rapidBlinkCount;
    uint32_t rapidBlinkTimerMs;
    bool rapidBlinkState;

    // Active Strike Attributes
    StrikeType currentStrikeType;
    LightningColorType currentColorType;
    float currentBrightness; 
    float decayRate;         
    float flashIntensity;    
    
    // Cached render dimensions to keep micro-flashes strictly in matrix range
    size_t lastWidth;
    size_t lastHeight;

    // Micro-Flash pixel indices
    std::vector<size_t> microPixelIndices;

    // Strobe / Multi-hit handling
    int reFlickerCount;

    // Vector Path Points (Normalised 0.0 -> 1.0)
    std::vector<Point2D> boltPoints;

    // Helper functions
    uint32_t getRandomRange(uint32_t min_val, uint32_t max_val);
    float getRandomFloat();
    uint32_t calculateNextDelay();
    void generateNextStrike();
    void generateMicroFlash(size_t width, size_t height);
    void generateMediumStrike();
    void generateMassStrike();
    void generateZigZagPath(bool vertical, float startPos);
    void generateForkedPath();
    void getLightningRGB(LightningColorType colorType, float intensity, uint8_t &r, uint8_t &g, uint8_t &b);
};

#endif // THUNDER_EFFECT_H