#pragma once

#include "ILightingLayer.h"

class SolarLightingLayer : public ILightingLayer {
public:
    SolarLightingLayer() = default;
    ~SolarLightingLayer() override = default;

    void render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) override;
};