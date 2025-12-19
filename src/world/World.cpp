#include "terraria/world/World.h"

#include <cstddef>
#include <stdexcept>

namespace terraria::world {

World::World(int width, int height)
    : width_{width},
      height_{height} {
    const std::size_t total = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    tiles_.reserve(total);
    for (std::size_t i = 0; i < total; ++i) {
        tiles_.push_back(MakeTile(TileType::Air, false));
    }
}

Tile& World::tile(int x, int y) {
    const std::size_t idx = index(x, y);
    return *tiles_[idx];
}

const Tile& World::tile(int x, int y) const {
    const std::size_t idx = index(x, y);
    return *tiles_[idx];
}

void World::setTile(int x, int y, TileType type, bool active) {
    const std::size_t idx = index(x, y);
    tiles_[idx] = MakeTile(type, active);
}

void World::setTileType(int x, int y, TileType type) {
    const bool wasActive = tile(x, y).active();
    setTile(x, y, type, wasActive);
}

std::size_t World::index(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        throw std::out_of_range("World::tile coordinates out of range");
    }
    return static_cast<std::size_t>(y * width_ + x);
}

} // namespace terraria::world
