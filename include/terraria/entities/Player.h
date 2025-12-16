#pragma once

#include "terraria/entities/Tools.h"
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
inline constexpr int kHotbarSlots = 10;

struct HotbarSlot {
    ItemCategory category{ItemCategory::Empty};
    world::TileType blockType{world::TileType::Air};
    ToolKind toolKind{ToolKind::Pickaxe};
    ToolTier toolTier{ToolTier::None};
    int count{0};

    bool isBlock() const { return category == ItemCategory::Block && blockType != world::TileType::Air && count > 0; }
    bool isTool() const { return category == ItemCategory::Tool && toolTier != ToolTier::None && count > 0; }
    bool empty() const { return !isBlock() && !isTool(); }
    void clear() {
        category = ItemCategory::Empty;
        blockType = world::TileType::Air;
        toolKind = ToolKind::Pickaxe;
        toolTier = ToolTier::None;
        count = 0;
    }
};

class Player {
public:
    bool addToInventory(world::TileType type, int amount = 1);
    bool addTool(ToolKind kind, ToolTier tier);
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
    int inventoryCount(world::TileType type) const;
    bool consumeFromInventory(world::TileType type, int amount);
    bool consumeSlot(int slotIndex, int amount = 1);
    const std::array<HotbarSlot, kHotbarSlots>& hotbar() const { return hotbar_; }

private:
    Vec2 position_{};
    Vec2 velocity_{};
    bool onGround_{false};
    int health_{kPlayerMaxHealth};
    std::array<HotbarSlot, kHotbarSlots> hotbar_{};
};

} // namespace terraria::entities

inline bool terraria::entities::Player::addToInventory(world::TileType type, int amount) {
    if (amount <= 0 || type == world::TileType::Air) {
        return true;
    }
    for (auto& slot : hotbar_) {
        if (slot.isBlock() && slot.blockType == type) {
            slot.count += amount;
            return true;
        }
    }
    for (auto& slot : hotbar_) {
        if (slot.empty()) {
            slot.category = ItemCategory::Block;
            slot.blockType = type;
            slot.count = amount;
            return true;
        }
    }
    return false;
}

inline bool terraria::entities::Player::addTool(ToolKind kind, ToolTier tier) {
    if (tier == ToolTier::None) {
        return false;
    }
    for (auto& slot : hotbar_) {
        if (slot.isTool() && slot.toolKind == kind && slot.toolTier == tier) {
            // Already have this tool in the slot, treat as success.
            return true;
        }
    }
    for (auto& slot : hotbar_) {
        if (slot.empty()) {
            slot.category = ItemCategory::Tool;
            slot.toolKind = kind;
            slot.toolTier = tier;
            slot.count = 1;
            return true;
        }
    }
    return false;
}

inline int terraria::entities::Player::inventoryCount(world::TileType type) const {
    int total = 0;
    for (const auto& slot : hotbar_) {
        if (slot.isBlock() && slot.blockType == type) {
            total += slot.count;
        }
    }
    return total;
}

inline bool terraria::entities::Player::consumeSlot(int slotIndex, int amount) {
    if (slotIndex < 0 || slotIndex >= kHotbarSlots || amount <= 0) {
        return false;
    }
    auto& slot = hotbar_[static_cast<std::size_t>(slotIndex)];
    if (!slot.isBlock() || slot.count < amount) {
        return false;
    }
    slot.count -= amount;
    if (slot.count <= 0) {
        slot.clear();
    }
    return true;
}

inline bool terraria::entities::Player::consumeFromInventory(world::TileType type, int amount) {
    if (amount <= 0) {
        return true;
    }
    int remaining = amount;
    for (auto& slot : hotbar_) {
        if (slot.isBlock() && slot.blockType == type) {
            const int used = std::min(slot.count, remaining);
            slot.count -= used;
            remaining -= used;
            if (slot.count <= 0) {
                slot.clear();
            }
            if (remaining <= 0) {
                break;
            }
        }
    }
    return remaining <= 0;
}
