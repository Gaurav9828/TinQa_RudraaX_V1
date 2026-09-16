#pragma once

#include "ILightingLayer.h"
#include <vector>
#include <utility>

class SolarLightingLayer : public ILightingLayer {
public:
    SolarLightingLayer();
    ~SolarLightingLayer() override = default;

    void init();
    void render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) override;

private:
    void parseConfig();
    float clampf(float val, float min_val, float max_val) const;

    std::vector<std::pair<size_t, size_t>> m_peak_coordinates;
    float m_sun_direction_x;
    float m_sun_direction_y;
};