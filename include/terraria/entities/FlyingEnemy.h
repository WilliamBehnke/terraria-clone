#pragma once

#include "terraria/entities/Vec2.h"

#include <algorithm>

namespace terraria::entities {

inline constexpr float kFlyingEnemyRadius = 0.45F;

struct FlyingEnemy {
    Vec2 position{};
    Vec2 velocity{};
    int maxHealth{60};
    int health{60};
    float attackCooldown{0.0F};
    float contactCooldown{0.0F};
    int id{0};
    float knockbackTimer{0.0F};
    float knockbackVelocity{0.0F};
    bool droppedLoot{false};
    float offscreenTimer{0.0F};

    bool alive() const { return health > 0; }
    void takeDamage(int amount) { health = std::max(0, health - amount); }
};

} // namespace terraria::entities
