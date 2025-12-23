#pragma once

#include "terraria/entities/Tools.h"
#include "terraria/entities/Vec2.h"
#include "terraria/world/Tile.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace terraria::entities {

inline constexpr float kPlayerHalfWidth = 0.35F;
inline constexpr float kPlayerHeight = 1.8F;
inline constexpr int kPlayerMaxHealth = 100;
inline constexpr int kHotbarSlots = 9;
inline constexpr int kInventoryRows = 4;
inline constexpr int kInventorySlots = kHotbarSlots * kInventoryRows;
inline constexpr int kMaxStackCount = 99;

struct InventorySlot {
    ItemCategory category{ItemCategory::Empty};
    world::TileType blockType{world::TileType::Air};
    ToolKind toolKind{ToolKind::Pickaxe};
    ToolTier toolTier{ToolTier::None};
    ArmorId armorId{ArmorId::None};
    AccessoryId accessoryId{AccessoryId::None};
    int count{0};

    bool isBlock() const { return category == ItemCategory::Block && blockType != world::TileType::Air && count > 0; }
    bool isTool() const { return category == ItemCategory::Tool && toolTier != ToolTier::None && count > 0; }
    bool isArmor() const { return category == ItemCategory::Armor && armorId != ArmorId::None && count > 0; }
    bool isAccessory() const { return category == ItemCategory::Accessory && accessoryId != AccessoryId::None && count > 0; }
    bool empty() const { return !isBlock() && !isTool() && !isArmor() && !isAccessory(); }
    bool canStackWith(const InventorySlot& other) const {
        return isBlock() && other.isBlock() && blockType == other.blockType;
    }
    void clear() {
        category = ItemCategory::Empty;
        blockType = world::TileType::Air;
        toolKind = ToolKind::Pickaxe;
        toolTier = ToolTier::None;
        armorId = ArmorId::None;
        accessoryId = AccessoryId::None;
        count = 0;
    }
};

class Player {
public:
    bool addToInventory(world::TileType type, int amount = 1);
    bool addTool(ToolKind kind, ToolTier tier);
    bool addArmor(ArmorId armorId);
    bool addAccessory(AccessoryId accessoryId);
    void setPosition(Vec2 pos) { position_ = pos; }
    Vec2 position() const { return position_; }

    void setVelocity(Vec2 velocity) { velocity_ = velocity; }
    Vec2 velocity() const { return velocity_; }
    void setOnGround(bool grounded) { onGround_ = grounded; }
    bool onGround() const { return onGround_; }
    int applyDamage(int amount) {
        const int mitigated = std::max(1, amount - equipmentStats_.defense);
        health_ = std::max(0, health_ - mitigated);
        return mitigated;
    }
    void heal(int amount) { health_ = std::min(maxHealth(), health_ + amount); }
    int health() const { return health_; }
    void resetHealth() { health_ = maxHealth(); }
    void setHealth(int health) { health_ = std::clamp(health, 0, maxHealth()); }
    int inventoryCount(world::TileType type) const;
    bool consumeFromInventory(world::TileType type, int amount);
    bool consumeAmmo(world::TileType type, int amount);
    InventorySlot& ammoSlot() { return ammoSlot_; }
    const InventorySlot& ammoSlot() const { return ammoSlot_; }
    bool consumeSlot(int slotIndex, int amount = 1);
    std::span<InventorySlot, kHotbarSlots> hotbar() { return std::span<InventorySlot, kHotbarSlots>(inventory_.data(), kHotbarSlots); }
    std::span<const InventorySlot, kHotbarSlots> hotbar() const {
        return std::span<const InventorySlot, kHotbarSlots>(inventory_.data(), kHotbarSlots);
    }
    std::array<InventorySlot, kInventorySlots>& inventory() { return inventory_; }
    const std::array<InventorySlot, kInventorySlots>& inventory() const { return inventory_; }
    ArmorId armorAt(ArmorSlot slot) const { return equippedArmor_[static_cast<std::size_t>(slot)]; }
    AccessoryId accessoryAt(int index) const {
        if (index < 0 || index >= kAccessorySlotCount) {
            return AccessoryId::None;
        }
        return equippedAccessories_[static_cast<std::size_t>(index)];
    }
    void equipArmor(ArmorSlot slot, ArmorId id) {
        equippedArmor_[static_cast<std::size_t>(slot)] = id;
        recalcEquipmentStats();
    }
    void equipAccessory(int index, AccessoryId id) {
        if (index < 0 || index >= kAccessorySlotCount) {
            return;
        }
        equippedAccessories_[static_cast<std::size_t>(index)] = id;
        recalcEquipmentStats();
    }
    const std::array<ArmorId, static_cast<std::size_t>(ArmorSlot::Count)>& equippedArmor() const {
        return equippedArmor_;
    }
    int maxHealth() const { return kPlayerMaxHealth + equipmentStats_.maxHealthBonus; }
    int defense() const { return equipmentStats_.defense; }
    float moveSpeedMultiplier() const { return 1.0F + equipmentStats_.moveSpeedBonus; }
    float jumpBonus() const { return equipmentStats_.jumpBonus; }

private:
    void recalcEquipmentStats();

    Vec2 position_{};
    Vec2 velocity_{};
    bool onGround_{false};
    int health_{kPlayerMaxHealth};
    std::array<InventorySlot, kInventorySlots> inventory_{};
    InventorySlot ammoSlot_{};
    std::array<ArmorId, static_cast<std::size_t>(ArmorSlot::Count)> equippedArmor_{};
    std::array<AccessoryId, kAccessorySlotCount> equippedAccessories_{};
    EquipmentStats equipmentStats_{};
};

} // namespace terraria::entities

inline bool terraria::entities::Player::addToInventory(world::TileType type, int amount) {
    if (amount <= 0 || type == world::TileType::Air) {
        return true;
    }
    int remaining = amount;
    for (auto& slot : inventory_) {
        if (slot.isBlock() && slot.blockType == type && slot.count < kMaxStackCount) {
            const int space = kMaxStackCount - slot.count;
            const int moved = std::min(space, remaining);
            slot.count += moved;
            remaining -= moved;
            if (remaining <= 0) {
                return true;
            }
        }
    }
    for (auto& slot : inventory_) {
        if (slot.empty()) {
            const int moved = std::min(kMaxStackCount, remaining);
            slot.category = ItemCategory::Block;
            slot.blockType = type;
            slot.count = moved;
            remaining -= moved;
            if (remaining <= 0) {
                return true;
            }
        }
    }
    return false;
}

inline bool terraria::entities::Player::addTool(ToolKind kind, ToolTier tier) {
    if (tier == ToolTier::None) {
        return false;
    }
    for (auto& slot : inventory_) {
        if (slot.isTool() && slot.toolKind == kind && slot.toolTier == tier) {
            // Already have this tool in the slot, treat as success.
            return true;
        }
    }
    for (auto& slot : inventory_) {
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

inline bool terraria::entities::Player::addArmor(ArmorId armorId) {
    if (armorId == ArmorId::None) {
        return false;
    }
    for (auto& slot : inventory_) {
        if (slot.empty()) {
            slot.category = ItemCategory::Armor;
            slot.armorId = armorId;
            slot.count = 1;
            return true;
        }
    }
    return false;
}

inline bool terraria::entities::Player::addAccessory(AccessoryId accessoryId) {
    if (accessoryId == AccessoryId::None) {
        return false;
    }
    for (auto& slot : inventory_) {
        if (slot.empty()) {
            slot.category = ItemCategory::Accessory;
            slot.accessoryId = accessoryId;
            slot.count = 1;
            return true;
        }
    }
    return false;
}

inline int terraria::entities::Player::inventoryCount(world::TileType type) const {
    int total = 0;
    for (const auto& slot : inventory_) {
        if (slot.isBlock() && slot.blockType == type) {
            total += slot.count;
        }
    }
    if (ammoSlot_.isBlock() && ammoSlot_.blockType == type) {
        total += ammoSlot_.count;
    }
    return total;
}

inline bool terraria::entities::Player::consumeSlot(int slotIndex, int amount) {
    if (slotIndex < 0 || slotIndex >= kHotbarSlots || amount <= 0) {
        return false;
    }
    auto& slot = inventory_[static_cast<std::size_t>(slotIndex)];
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
    if (ammoSlot_.isBlock() && ammoSlot_.blockType == type) {
        const int used = std::min(ammoSlot_.count, remaining);
        ammoSlot_.count -= used;
        remaining -= used;
        if (ammoSlot_.count <= 0) {
            ammoSlot_.clear();
        }
        if (remaining <= 0) {
            return true;
        }
    }
    for (auto& slot : inventory_) {
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

inline bool terraria::entities::Player::consumeAmmo(world::TileType type, int amount) {
    if (amount <= 0) {
        return true;
    }
    int remaining = amount;
    if (ammoSlot_.isBlock() && ammoSlot_.blockType == type) {
        const int used = std::min(ammoSlot_.count, remaining);
        ammoSlot_.count -= used;
        remaining -= used;
        if (ammoSlot_.count <= 0) {
            ammoSlot_.clear();
        }
    }
    if (remaining <= 0) {
        return true;
    }
    for (auto& slot : inventory_) {
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

inline void terraria::entities::Player::recalcEquipmentStats() {
    EquipmentStats stats{};
    for (ArmorId id : equippedArmor_) {
        stats = CombineEquipmentStats(stats, ArmorStats(id));
    }
    for (AccessoryId id : equippedAccessories_) {
        stats = CombineEquipmentStats(stats, AccessoryStats(id));
    }
    equipmentStats_ = stats;
    health_ = std::min(health_, maxHealth());
}
