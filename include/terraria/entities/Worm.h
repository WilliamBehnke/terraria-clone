#pragma once

#include "terraria/entities/Vec2.h"

#include <algorithm>

namespace terraria::entities {

inline constexpr float kWormRadius = 0.5F;

struct Worm {
    Vec2 position{};
    Vec2 velocity{};
    int maxHealth{80};
    int health{80};
    float attackCooldown{0.0F};
    float knockbackTimer{0.0F};
    float knockbackVelocity{0.0F};
    float airTimer{0.0F};
    float lungeCooldown{0.0F};
    int id{0};
    bool droppedLoot{false};
    float offscreenTimer{0.0F};

    bool alive() const { return health > 0; }
    void takeDamage(int amount) { health = std::max(0, health - amount); }
};

} // namespace terraria::entities
