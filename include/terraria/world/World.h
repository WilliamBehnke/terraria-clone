#pragma once

#include "terraria/world/Tile.h"

#include <vector>

namespace terraria::world {

class World {
public:
    World(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    Tile& tile(int x, int y);
    const Tile& tile(int x, int y) const;

    const std::vector<Tile>& data() const { return tiles_; }

private:
    int width_;
    int height_;
    std::vector<Tile> tiles_;
};

} // namespace terraria::world
