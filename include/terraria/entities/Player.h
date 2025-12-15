#pragma once

#include "terraria/world/Tile.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace terraria::entities {

inline constexpr float kPlayerHalfWidth = 0.35F;
inline constexpr float kPlayerHeight = 1.8F;

struct Vec2 {
    float x{0.0F};
    float y{0.0F};
};

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

private:
    Vec2 position_{};
    Vec2 velocity_{};
    bool onGround_{false};
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
