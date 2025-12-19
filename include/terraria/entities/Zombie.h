#pragma once

#include "terraria/entities/Vec2.h"

#include <algorithm>

namespace terraria::entities {

inline constexpr float kZombieHalfWidth = 0.35F;
inline constexpr float kZombieHeight = 1.6F;

struct Zombie {
    Vec2 position{};
    Vec2 velocity{};
    bool onGround{false};
    int health{35};
    float attackCooldown{0.0F};
    float jumpCooldown{0.0F};
    int id{0};
    float stuckTimer{0.0F};
    float lastX{0.0F};

    bool alive() const { return health > 0; }
    void takeDamage(int amount) { health = std::max(0, health - amount); }
};

} // namespace terraria::entities
