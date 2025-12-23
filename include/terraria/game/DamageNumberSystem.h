#pragma once

#include "terraria/entities/Vec2.h"
#include "terraria/rendering/HudState.h"

#include <vector>

namespace terraria::game {

class DamageNumberSystem {
public:
    void addDamage(const entities::Vec2& worldPos, int amount, bool isPlayer);
    void addLoot(const entities::Vec2& worldPos, int amount);
    void update(float dt);
    void reset();
    void fillHud(rendering::HudState& hud) const;

private:
    struct DamageNumber {
        entities::Vec2 position{};
        int amount{0};
        float timer{0.0F};
        float lifetime{1.0F};
        bool isPlayer{false};
        bool isLoot{false};
    };

    std::vector<DamageNumber> numbers_{};
};

} // namespace terraria::game
