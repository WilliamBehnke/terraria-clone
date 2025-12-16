#pragma once

#include <cstdint>

namespace terraria::world {

enum class TileType : std::uint8_t {
    Air,
    Dirt,
    Stone,
    Grass,
    CopperOre,
    IronOre,
    GoldOre,
    Wood,
    Leaves,
    WoodPlank,
    StoneBrick,
    TreeTrunk,
    TreeLeaves
};

struct Tile {
    TileType type{TileType::Air};
    bool active{false};
};

} // namespace terraria::world
