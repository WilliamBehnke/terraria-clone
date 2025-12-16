#pragma once

#include <cstdint>

namespace terraria::entities {

enum class ItemCategory : std::uint8_t {
    Empty,
    Block,
    Tool
};

enum class ToolKind : std::uint8_t {
    Pickaxe,
    Axe,
    Sword
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

} // namespace terraria::entities
