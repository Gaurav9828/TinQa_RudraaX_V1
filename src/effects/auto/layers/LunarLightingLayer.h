#pragma once

#include "ILightingLayer.h"

class LunarLightingLayer : public ILightingLayer {
public:
    LunarLightingLayer() = default;
    ~LunarLightingLayer() override = default;

    void render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) override;
};