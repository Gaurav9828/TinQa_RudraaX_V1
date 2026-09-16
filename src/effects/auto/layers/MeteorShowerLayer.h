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
    
    // Special diagonal meteor tracking
    bool is_special;
    float start_x, start_y;
    float end_x, end_y;

    enum class Classification {
        FAINT_NORMAL,  // ~85%
        BRIGHT,        // ~14.9%
        FIREBALL       // ~0.1%
    };
    Classification type;
};

class MeteorShowerLayer {
public:
    MeteorShowerLayer();
    ~MeteorShowerLayer() = default;

    void init();
    void update(uint32_t delta_ms, double current_hour, uint16_t day_of_year, bool is_hyperlapse, size_t width, size_t height);
    void render(uint8_t* buffer, size_t width, size_t height, float cloud_density);

private:
    void spawnMeteor(size_t width, size_t height, Meteor::Classification classification, bool force_special_diagonal = false);
    bool isAnnualMeteorShowerDay(uint16_t day_of_year) const;
    uint32_t calculateSpawnInterval(double current_hour, bool is_hyperlapse, bool is_shower_active) const;

    static constexpr size_t MAX_METEORS = 8;
    std::vector<Meteor> m_meteors;
    uint32_t m_spawn_timer_ms;
    int m_meteors_spawned_tonight;
    int m_last_checked_hour;
    bool m_special_meteor_spawned_tonight;
};

#endif // METEOR_SHOWER_LAYER_H