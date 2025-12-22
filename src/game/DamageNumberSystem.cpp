#include "terraria/game/DamageNumberSystem.h"

#include <algorithm>

namespace terraria::game {

void DamageNumberSystem::addDamage(const entities::Vec2& worldPos, int amount, bool isPlayer) {
    if (amount <= 0) {
        return;
    }
    DamageNumber entry{};
    entry.position = worldPos;
    entry.amount = amount;
    entry.timer = 0.0F;
    entry.lifetime = 1.0F;
    entry.isPlayer = isPlayer;
    entry.isLoot = false;
    numbers_.push_back(entry);
}

void DamageNumberSystem::addLoot(const entities::Vec2& worldPos, int amount) {
    if (amount <= 0) {
        return;
    }
    DamageNumber entry{};
    entry.position = worldPos;
    entry.amount = amount;
    entry.timer = 0.0F;
    entry.lifetime = 1.1F;
    entry.isPlayer = false;
    entry.isLoot = true;
    numbers_.push_back(entry);
}

void DamageNumberSystem::update(float dt) {
    for (auto& entry : numbers_) {
        entry.timer += dt;
        entry.position.y -= dt * 0.6F;
    }
    numbers_.erase(std::remove_if(numbers_.begin(),
                                  numbers_.end(),
                                  [](const DamageNumber& entry) { return entry.timer >= entry.lifetime; }),
                   numbers_.end());
}

void DamageNumberSystem::fillHud(rendering::HudState& hud) const {
    hud.damageNumbers.clear();
    hud.damageNumbers.reserve(numbers_.size());
    for (const auto& entry : numbers_) {
        rendering::DamageNumberHud hudEntry{};
        if (entry.isLoot) {
            hudEntry.x = entry.position.x + 0.4F;
            hudEntry.y = entry.position.y - 0.9F;
        } else {
            hudEntry.x = entry.position.x;
            hudEntry.y = entry.position.y;
        }
        hudEntry.amount = entry.amount;
        hudEntry.isPlayer = entry.isPlayer;
        hudEntry.isLoot = entry.isLoot;
        hudEntry.alpha = std::clamp(1.0F - entry.timer / entry.lifetime, 0.0F, 1.0F);
        hud.damageNumbers.push_back(hudEntry);
    }
}

} // namespace terraria::game
