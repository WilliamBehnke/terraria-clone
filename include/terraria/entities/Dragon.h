#pragma once

#include "terraria/entities/Vec2.h"

#include <algorithm>

namespace terraria::entities {

inline constexpr float kDragonHalfWidth = 3.0F;
inline constexpr float kDragonHeight = 2.2F;

struct Dragon {
    Vec2 position{};
    Vec2 velocity{};
    int maxHealth{420};
    int health{420};
    float attackCooldown{0.0F};
    float fireCooldown{0.0F};
    float breathTimer{0.0F};
    float breathTick{0.0F};
    float chargeTimer{0.0F};
    float chargeCooldown{0.0F};
    float volleyCooldown{0.0F};
    float facing{1.0F};
    float knockbackTimer{0.0F};
    float knockbackVelocity{0.0F};
    bool droppedLoot{false};

    bool alive() const { return health > 0; }
    void takeDamage(int amount) { health = std::max(0, health - amount); }
};

} // namespace terraria::entities
