#pragma once

#include "terraria/world/World.h"

namespace terraria::world {

class WorldGenerator {
public:
    void generate(World& world);

private:
    void carveCaves(World& world);
    void carveCircle(World& world, int centerX, int centerY, int radius);
    void placeOres(World& world);
};

} // namespace terraria::world
