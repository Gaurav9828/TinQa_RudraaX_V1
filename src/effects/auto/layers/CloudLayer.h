#ifndef CLOUD_LAYER_H
#define CLOUD_LAYER_H

#include <stdint.h>
#include <stddef.h>

enum class CloudDensityTier : uint8_t {
    LEVEL_0_CLEAR = 0,         
    LEVEL_1_FEW = 1,           
    LEVEL_2_SCATTERED = 2,     
    LEVEL_3_BROKEN = 3,        
    LEVEL_4_MID_OVERCAST = 4,  
    LEVEL_5_HEAVY = 5,         
    LEVEL_6_VERY_HEAVY = 6,    
    LEVEL_7_THUNDER_STORM = 7  
};

class CloudLayer {
public:
    CloudLayer();
    ~CloudLayer() = default;

    void init();
    void update(uint32_t delta_ms, bool is_hyperlapse);
    void render(uint8_t* buffer, size_t width, size_t height, CloudDensityTier tier, bool is_daytime);

private:
    float m_scroll_x;
};

#endif // CLOUD_LAYER_H