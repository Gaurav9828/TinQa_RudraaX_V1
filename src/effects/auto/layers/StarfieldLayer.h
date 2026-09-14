#pragma once

#include "ILightingLayer.h"
#include <vector>

struct FixedStar {
    size_t x;
    size_t y;
    uint32_t seed;
};

class StarfieldLayer : public ILightingLayer {
public:
    StarfieldLayer();
    ~StarfieldLayer() override = default;

    void init() override;
    void render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) override;

private:
    void generateFixedStars(size_t width, size_t height);

    std::vector<FixedStar> m_stars;
    size_t m_cached_width;
    size_t m_cached_height;
};