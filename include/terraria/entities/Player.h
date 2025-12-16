#pragma once

#include "terraria/entities/Vec2.h"
#include "terraria/world/Tile.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace terraria::entities {

inline constexpr float kPlayerHalfWidth = 0.35F;
inline constexpr float kPlayerHeight = 1.8F;
inline constexpr int kPlayerMaxHealth = 100;

class Player {
public:
    void addToInventory(world::TileType type);
    bool removeFromInventory(world::TileType type);
    const std::array<int, static_cast<std::size_t>(world::TileType::TreeLeaves) + 1>& inventory() const {
        return inventory_;
    }

    void setPosition(Vec2 pos) { position_ = pos; }
    Vec2 position() const { return position_; }

    void setVelocity(Vec2 velocity) { velocity_ = velocity; }
    Vec2 velocity() const { return velocity_; }
    void setOnGround(bool grounded) { onGround_ = grounded; }
    bool onGround() const { return onGround_; }
    void applyDamage(int amount) { health_ = std::max(0, health_ - amount); }
    void heal(int amount) { health_ = std::min(kPlayerMaxHealth, health_ + amount); }
    int health() const { return health_; }
    void resetHealth() { health_ = kPlayerMaxHealth; }

private:
    Vec2 position_{};
    Vec2 velocity_{};
    bool onGround_{false};
    int health_{kPlayerMaxHealth};
    std::array<int, static_cast<std::size_t>(world::TileType::TreeLeaves) + 1> inventory_{};
};

} // namespace terraria::entities

inline void terraria::entities::Player::addToInventory(world::TileType type) {
    const auto index = static_cast<std::size_t>(type);
    if (index < inventory_.size()) {
        ++inventory_[index];
    }
}

inline bool terraria::entities::Player::removeFromInventory(world::TileType type) {
    const auto index = static_cast<std::size_t>(type);
    if (index >= inventory_.size()) {
        return false;
    }
    if (inventory_[index] <= 0) {
        return false;
    }
    --inventory_[index];
    return true;
}
