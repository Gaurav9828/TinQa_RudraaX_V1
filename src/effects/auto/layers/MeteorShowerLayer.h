#ifndef METEOR_SHOWER_LAYER_H
#define METEOR_SHOWER_LAYER_H

#include <cstdint>
#include <vector>
#include <cstddef>

struct Meteor {
    float x, y;
    float vx, vy;
    float life;
    float life_speed;
    float brightness;
    float max_brightness;
    bool active;
    bool brightening;
};

class MeteorShowerLayer {
public:
    MeteorShowerLayer();
    ~MeteorShowerLayer() = default;

    void init();
    void update(uint32_t delta_ms, double current_hour, bool is_hyperlapse);
    void render(uint8_t* buffer, size_t width, size_t height, float cloud_density);

private:
    void spawnMeteor(size_t width, size_t height, bool is_mega_meteor);

    static constexpr size_t MAX_METEORS = 5;
    std::vector<Meteor> m_meteors;
    uint32_t m_spawn_timer_ms;
    int m_meteors_spawned_tonight;
    int m_last_checked_hour;
};

#endif // METEOR_SHOWER_LAYER_H