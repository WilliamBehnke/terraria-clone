#include "terraria/world/World.h"

#include <cstddef>
#include <stdexcept>

namespace terraria::world {

World::World(int width, int height)
    : width_{width},
      height_{height},
      tiles_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

Tile& World::tile(int x, int y) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        throw std::out_of_range("World::tile coordinates out of range");
    }
    const std::size_t index = static_cast<std::size_t>(y * width_ + x);
    return tiles_[index];
}

const Tile& World::tile(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        throw std::out_of_range("World::tile coordinates out of range");
    }
    const std::size_t index = static_cast<std::size_t>(y * width_ + x);
    return tiles_[index];
}

} // namespace terraria::world
