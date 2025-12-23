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
    struct DragonDenInfo {
        int centerX{0};
        int centerY{0};
        int radiusX{0};
        int radiusY{0};
    };

    void generate(World& world);
    void generate(World& world, std::uint32_t seed);
    void generate(World& world, std::uint32_t seed, const WorldGenConfig& config);
    DragonDenInfo dragonDenInfo(const World& world, std::uint32_t seed) const;

private:
    void carveCaves(World& world, std::uint32_t seed, const WorldGenConfig& config);
    void carveDragonDen(World& world, const DragonDenInfo& info);
    void carveCircle(World& world, int centerX, int centerY, int radius);
    void placeOres(World& world, std::uint32_t seed, const WorldGenConfig& config);
};

} // namespace terraria::world
