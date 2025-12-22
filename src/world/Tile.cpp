#include "terraria/world/Tile.h"

#include <memory>

namespace terraria::world {

std::unique_ptr<Tile> MakeTile(TileType type, bool active) {
    switch (type) {
    case TileType::TreeTrunk:
        return std::make_unique<TreeTrunkTile>(active);
    case TileType::TreeLeaves:
        return std::make_unique<TreeLeavesTile>(active);
    case TileType::Arrow:
        return std::make_unique<PassableTile>(TileType::Arrow, active);
    case TileType::Coin:
        return std::make_unique<PassableTile>(TileType::Coin, active);
    case TileType::Air:
        return std::make_unique<PassableTile>(TileType::Air, active);
    default:
        return std::make_unique<SolidTile>(type, active);
    }
}

} // namespace terraria::world
