#pragma once

#include "terraria/world/Tile.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace terraria::world {

class World {
public:
    World(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    Tile& tile(int x, int y);
    const Tile& tile(int x, int y) const;

    void setTile(int x, int y, TileType type, bool active);
    void setTileType(int x, int y, TileType type);

    const std::vector<std::unique_ptr<Tile>>& data() const { return tiles_; }

private:
    std::size_t index(int x, int y) const;

    int width_;
    int height_;
    std::vector<std::unique_ptr<Tile>> tiles_;
};

} // namespace terraria::world
