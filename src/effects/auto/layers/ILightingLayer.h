#pragma once

#include <cstdint>
#include <cstddef>
#include "../weather/WeatherProvider.h" 

struct RenderContext {
    double current_hour;
    uint16_t day_of_year;
    float moon_phase_factor;
    bool is_hyperlapse;
    ActiveWeatherState weather;
};

class ILightingLayer {
public:
    virtual ~ILightingLayer() = default;

    virtual void init() {}
    virtual void update(uint32_t delta_ms, const RenderContext& ctx) { (void)delta_ms; (void)ctx; }
    virtual void render(uint8_t* buffer, size_t width, size_t height, const RenderContext& ctx) = 0;
};