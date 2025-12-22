#pragma once

#include <array>
#include <cstdint>

namespace terraria::entities {

enum class ItemCategory : std::uint8_t {
    Empty,
    Block,
    Tool,
    Armor,
    Accessory
};

enum class ToolKind : std::uint8_t {
    Pickaxe,
    Axe,
    Shovel,
    Hoe,
    Sword,
    Bow
};

enum class ToolTier : std::uint8_t {
    None,
    Wood,
    Stone,
    Copper,
    Iron,
    Gold
};

inline int ToolTierValue(ToolTier tier) {
    switch (tier) {
    case ToolTier::Wood: return 1;
    case ToolTier::Stone: return 2;
    case ToolTier::Copper: return 3;
    case ToolTier::Iron: return 4;
    case ToolTier::Gold: return 5;
    default: return 0;
    }
}

enum class ArmorSlot : std::uint8_t {
    Head,
    Body,
    Legs,
    Count
};

enum class ArmorId : std::uint8_t {
    None,
    CopperHelmet,
    CopperChest,
    CopperLeggings,
    IronHelmet,
    IronChest,
    IronLeggings,
    GoldHelmet,
    GoldChest,
    GoldLeggings
};

enum class AccessoryId : std::uint8_t {
    None,
    FleetBoots,
    VitalityCharm,
    MinerRing
};

struct EquipmentStats {
    int defense{0};
    float moveSpeedBonus{0.0F};
    float jumpBonus{0.0F};
    int maxHealthBonus{0};
};

inline ArmorSlot ArmorSlotFor(ArmorId id) {
    switch (id) {
    case ArmorId::CopperHelmet:
    case ArmorId::IronHelmet:
    case ArmorId::GoldHelmet: return ArmorSlot::Head;
    case ArmorId::CopperChest:
    case ArmorId::IronChest:
    case ArmorId::GoldChest: return ArmorSlot::Body;
    case ArmorId::CopperLeggings:
    case ArmorId::IronLeggings:
    case ArmorId::GoldLeggings: return ArmorSlot::Legs;
    default: return ArmorSlot::Head;
    }
}

inline EquipmentStats ArmorStats(ArmorId id) {
    switch (id) {
    case ArmorId::CopperHelmet: return EquipmentStats{2, 0.0F, 0.0F, 0};
    case ArmorId::CopperChest: return EquipmentStats{3, 0.0F, 0.0F, 5};
    case ArmorId::CopperLeggings: return EquipmentStats{2, 0.02F, 0.0F, 0};
    case ArmorId::IronHelmet: return EquipmentStats{3, 0.0F, 0.0F, 5};
    case ArmorId::IronChest: return EquipmentStats{4, 0.02F, 0.0F, 5};
    case ArmorId::IronLeggings: return EquipmentStats{3, 0.04F, 0.05F, 0};
    case ArmorId::GoldHelmet: return EquipmentStats{4, 0.0F, 0.0F, 10};
    case ArmorId::GoldChest: return EquipmentStats{5, 0.03F, 0.0F, 10};
    case ArmorId::GoldLeggings: return EquipmentStats{4, 0.05F, 0.06F, 0};
    default: return EquipmentStats{};
    }
}

inline EquipmentStats AccessoryStats(AccessoryId id) {
    switch (id) {
    case AccessoryId::FleetBoots: return EquipmentStats{0, 0.08F, 0.1F, 0};
    case AccessoryId::VitalityCharm: return EquipmentStats{1, 0.0F, 0.0F, 20};
    case AccessoryId::MinerRing: return EquipmentStats{0, 0.0F, 0.0F, 10};
    default: return EquipmentStats{};
    }
}

inline const char* ArmorName(ArmorId id) {
    switch (id) {
    case ArmorId::CopperHelmet: return "COPPERHELM";
    case ArmorId::CopperChest: return "COPPERCHEST";
    case ArmorId::CopperLeggings: return "COPPERLEGGINGS";
    case ArmorId::IronHelmet: return "IRONHELM";
    case ArmorId::IronChest: return "IRONCHEST";
    case ArmorId::IronLeggings: return "IRONLEGGINGS";
    case ArmorId::GoldHelmet: return "GOLDHELM";
    case ArmorId::GoldChest: return "GOLDCHEST";
    case ArmorId::GoldLeggings: return "GOLDLEGGINGS";
    default: return "";
    }
}

inline const char* AccessoryName(AccessoryId id) {
    switch (id) {
    case AccessoryId::FleetBoots: return "FLEETBOOTS";
    case AccessoryId::VitalityCharm: return "VITALITY";
    case AccessoryId::MinerRing: return "MINERRING";
    default: return "";
    }
}

inline constexpr int kAccessorySlotCount = 3;

inline EquipmentStats CombineEquipmentStats(const EquipmentStats& a, const EquipmentStats& b) {
    EquipmentStats result;
    result.defense = a.defense + b.defense;
    result.moveSpeedBonus = a.moveSpeedBonus + b.moveSpeedBonus;
    result.jumpBonus = a.jumpBonus + b.jumpBonus;
    result.maxHealthBonus = a.maxHealthBonus + b.maxHealthBonus;
    return result;
}

} // namespace terraria::entities
