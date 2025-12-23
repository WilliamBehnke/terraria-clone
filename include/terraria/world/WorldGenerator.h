#pragma once

#include "terraria/world/World.h"

#include <cstdint>

namespace terraria::world {

class WorldGenerator {
public:
    struct WorldGenConfig {
        float terrainAmplitude{1.0F};
        float soilDepthScale{1.0F};
        float caveDensity{1.0F};
        float oreDensity{1.0F};
        float treeDensity{1.0F};
    };

    void generate(World& world);
    void generate(World& world, std::uint32_t seed);
    void generate(World& world, std::uint32_t seed, const WorldGenConfig& config);

private:
    void carveCaves(World& world, std::uint32_t seed, const WorldGenConfig& config);
    void carveCircle(World& world, int centerX, int centerY, int radius);
    void placeOres(World& world, std::uint32_t seed, const WorldGenConfig& config);
};

} // namespace terraria::world
