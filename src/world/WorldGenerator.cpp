#include "terraria/world/WorldGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace terraria::world {

namespace {

float hashNoise(int x, int seed) {
    std::uint32_t v = static_cast<std::uint32_t>(x) * 374761393U + static_cast<std::uint32_t>(seed) * 668265263U;
    v ^= v >> 13;
    v *= 1274126177U;
    return static_cast<float>(v & 0xFFFFFFU) / static_cast<float>(0xFFFFFFU);
}

float octaveNoise(int x, int seed, float baseFreq, int octaves) {
    float amplitude = 1.0F;
    float value = 0.0F;
    float total = 0.0F;
    float freq = baseFreq;
    for (int i = 0; i < octaves; ++i) {
        value += hashNoise(static_cast<int>(x * freq), seed + i * 31) * amplitude;
        total += amplitude;
        amplitude *= 0.5F;
        freq *= 2.0F;
    }
    return total > 0.0F ? value / total : 0.0F;
}

std::vector<int> buildSurfaceProfile(int width, int height, std::uint32_t seed, float amplitude) {
    std::vector<int> surface(static_cast<std::size_t>(width), height / 3);
    const int chunkSize = 192;
    const int chunkCount = width / chunkSize + 2;
    std::vector<float> chunkHeights(static_cast<std::size_t>(chunkCount));
    for (int i = 0; i < chunkCount; ++i) {
        const int sampleX = i * chunkSize;
        const float broad = octaveNoise(sampleX, static_cast<int>(2001 + seed), 0.00005F, 3);
        const float mesas = octaveNoise(sampleX, static_cast<int>(3311 + seed), 0.00018F, 2);
        chunkHeights[static_cast<std::size_t>(i)] = 0.32F + broad * 0.5F + (mesas - 0.5F) * 0.2F;
    }

    for (int x = 0; x < width; ++x) {
        const int chunkIndex = x / chunkSize;
        const float t = static_cast<float>(x % chunkSize) / static_cast<float>(chunkSize);
        float normalized = std::lerp(chunkHeights[static_cast<std::size_t>(chunkIndex)],
                                     chunkHeights[static_cast<std::size_t>(chunkIndex + 1)],
                                     t);

        const float continental = (octaveNoise(x, static_cast<int>(7231 + seed), 0.00035F, 4) - 0.5F) * amplitude;
        const float rolling = (octaveNoise(x, static_cast<int>(9127 + seed), 0.0009F, 4) - 0.5F) * amplitude;
        const float ridge = std::sin(static_cast<float>(x) * 0.00045F) * 0.18F;
        normalized += continental * 0.2F + rolling * 0.12F + ridge * amplitude;

        normalized = std::clamp(normalized, 0.12F, 0.85F);
        int surfaceY = static_cast<int>(normalized * static_cast<float>(height));
        surfaceY = std::clamp(surfaceY, height / 8, (height * 5) / 6);
        surface[static_cast<std::size_t>(x)] = surfaceY;
    }

    for (int iteration = 0; iteration < 2; ++iteration) {
        std::vector<int> copy = surface;
        for (int x = 1; x < width - 1; ++x) {
            surface[static_cast<std::size_t>(x)] =
                (copy[static_cast<std::size_t>(x - 1)] + copy[static_cast<std::size_t>(x)] * 2
                 + copy[static_cast<std::size_t>(x + 1)]) / 4;
        }
    }

    for (int x = 1; x < width; ++x) {
        const int diff = surface[static_cast<std::size_t>(x)] - surface[static_cast<std::size_t>(x - 1)];
        if (diff > 16) {
            surface[static_cast<std::size_t>(x)] = surface[static_cast<std::size_t>(x - 1)] + 16;
        } else if (diff < -16) {
            surface[static_cast<std::size_t>(x)] = surface[static_cast<std::size_t>(x - 1)] - 16;
        }
    }

    return surface;
}

void paintColumn(World& world, int x, int surfaceY, std::uint32_t seed, float soilScale) {
    const int height = world.height();
    const int soilMin = std::max(6, static_cast<int>(std::round(16.0F * soilScale)));
    const int soilMax = std::max(soilMin + 4, static_cast<int>(std::round(28.0F * soilScale)));
    const int surface = std::clamp(surfaceY, 0, std::max(0, height - 2));

    const int soilDepth = soilMin
        + static_cast<int>(hashNoise(x * 37, static_cast<int>(9001 + seed)) * static_cast<float>(soilMax - soilMin));
    const int transitionDepth = soilDepth + 18;
    const bool mountainTop = surface <= height / 6;

    for (int y = 0; y < height; ++y) {
        if (y < surface) {
            world.setTile(x, y, TileType::Air, false);
            continue;
        }

        const int depth = y - surface;
        if (depth == 0) {
            if (mountainTop && hashNoise(x * 73, static_cast<int>(18511 + seed)) > 0.7F) {
                world.setTile(x, y, TileType::Stone, true);
            } else {
                world.setTile(x, y, TileType::Grass, true);
            }
            continue;
        }

        if (depth <= soilDepth) {
            world.setTile(x, y, TileType::Dirt, true);
        } else if (depth <= transitionDepth) {
            const float blend = hashNoise(x * 131 + depth * 53, static_cast<int>(7001 + seed));
            world.setTile(x, y, blend > 0.35F ? TileType::Stone : TileType::Dirt, true);
        } else {
            world.setTile(x, y, TileType::Stone, true);
        }
    }
}

bool placeTree(World& world, int x, int surfaceY, int height) {
    if (height < 3 || surfaceY <= 0 || surfaceY + 1 >= world.height()) {
        return false;
    }

    const Tile& support = world.tile(x, surfaceY);
    const Tile& below = world.tile(x, surfaceY + 1);
    if (!support.active() || (support.type() != TileType::Grass && support.type() != TileType::Dirt)) {
        return false;
    }
    if (!below.active() || below.type() != TileType::Dirt) {
        return false;
    }

    const int baseY = surfaceY - 1;
    if (baseY - (height - 1) < 0) {
        return false;
    }

    for (int i = 0; i < height; ++i) {
        const int ty = baseY - i;
        if (ty < 0) {
            return false;
        }
        if (world.tile(x, ty).active()) {
            return false;
        }
    }

    for (int i = 0; i < height; ++i) {
        const int ty = baseY - i;
        world.setTile(x, ty, TileType::TreeTrunk, true);
    }

    const int trunkTop = baseY - (height - 1);
    const int canopyCenter = std::max(0, trunkTop - 1);
    const int canopyRadius = std::max(2, height / 3 + 1);
    for (int dy = -canopyRadius; dy <= canopyRadius; ++dy) {
        for (int dx = -canopyRadius; dx <= canopyRadius; ++dx) {
            if (dx * dx + dy * dy > canopyRadius * canopyRadius) {
                continue;
            }
            const int tx = x + dx;
            const int ty = canopyCenter + dy;
            if (tx < 0 || tx >= world.width() || ty < 0 || ty >= world.height()) {
                continue;
            }
            const Tile& tile = world.tile(tx, ty);
            if (!tile.active() || tile.type() == TileType::TreeLeaves) {
                world.setTile(tx, ty, TileType::TreeLeaves, true);
            }
        }
    }

    return true;
}

void scatterSurfaceTrees(World& world,
                         const std::vector<int>& surfaceY,
                         std::mt19937& rng,
                         float treeDensity) {
    if (world.width() < 8) {
        return;
    }

    const int gapMin = std::max(2, static_cast<int>(std::round(4.0F / std::max(0.2F, treeDensity))));
    const int gapMax = std::max(gapMin + 2, static_cast<int>(std::round(9.0F / std::max(0.2F, treeDensity))));
    std::uniform_int_distribution<int> gapDist{gapMin, gapMax};
    std::uniform_int_distribution<int> heightDist{5, 9};
    int cooldown = gapDist(rng);

    for (int x = 3; x < world.width() - 3; ++x) {
        if (cooldown > 0) {
            --cooldown;
            continue;
        }

        const int surface = surfaceY[static_cast<std::size_t>(x)];
        if (surface < 6 || surface >= world.height() - 12) {
            continue;
        }

        const int leftDiff = std::abs(surface - surfaceY[static_cast<std::size_t>(x - 1)]);
        const int rightDiff = std::abs(surface - surfaceY[static_cast<std::size_t>(x + 1)]);
        if (leftDiff > 3 || rightDiff > 3) {
            continue;
        }

        if (placeTree(world, x, surface, heightDist(rng))) {
            cooldown = gapDist(rng);
        } else {
            cooldown = 1;
        }
    }
}

struct OreConfig {
    TileType type;
    float minDepthRatio;
    float maxDepthRatio;
    int minVeins;
    int veinScale;
    int minLength;
    int maxLength;
    int minRadius;
    int maxRadius;
};

void placeOreVeins(World& world, const OreConfig& config, std::mt19937& rng) {
    const int minY = std::clamp(static_cast<int>(config.minDepthRatio * static_cast<float>(world.height())), 0, world.height() - 1);
    const int maxY = std::clamp(static_cast<int>(config.maxDepthRatio * static_cast<float>(world.height())), 0, world.height() - 1);
    if (minY >= maxY) {
        return;
    }

    std::uniform_int_distribution<int> startX{0, world.width() - 1};
    std::uniform_int_distribution<int> startY{minY, maxY};
    std::uniform_int_distribution<int> lengthDist{config.minLength, config.maxLength};
    std::uniform_int_distribution<int> radiusDist{config.minRadius, config.maxRadius};
    std::uniform_real_distribution<float> angleDist{0.0F, 2.0F * 3.1415926F};
    std::uniform_real_distribution<float> turnDist{-0.35F, 0.35F};

    const int baseVeins = std::max(config.minVeins, world.width() / std::max(1, config.veinScale));
    for (int i = 0; i < baseVeins; ++i) {
        float x = static_cast<float>(startX(rng));
        float y = static_cast<float>(startY(rng));
        float angle = angleDist(rng);
        const int length = lengthDist(rng);
        int radius = radiusDist(rng);

        for (int step = 0; step < length; ++step) {
            const int tileX = static_cast<int>(std::round(x));
            const int tileY = static_cast<int>(std::round(y));
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (dx * dx + dy * dy > radius * radius) continue;
                    const int tx = tileX + dx;
                    const int ty = tileY + dy;
                    if (tx >= 0 && tx < world.width() && ty >= 0 && ty < world.height()) {
                        const Tile& tile = world.tile(tx, ty);
                        if (tile.active() && (tile.type() == TileType::Stone || tile.type() == TileType::Dirt)) {
                            world.setTileType(tx, ty, config.type);
                        }
                    }
                }
            }

            x += std::cos(angle);
            y += std::sin(angle) * 0.9F;
            angle += turnDist(rng);
            radius += static_cast<int>(turnDist(rng) * 2.0F);
            radius = std::clamp(radius, config.minRadius, config.maxRadius);

            if (x < 1 || x >= world.width() - 1 || y < minY || y > maxY) {
                break;
            }
        }
    }
}

} // namespace

void WorldGenerator::generate(World& world) {
    generate(world, 0);
}

void WorldGenerator::generate(World& world, std::uint32_t seed) {
    generate(world, seed, WorldGenConfig{});
}

void WorldGenerator::generate(World& world, std::uint32_t seed, const WorldGenConfig& config) {
    const int width = world.width();
    const int height = world.height();
    if (width <= 0 || height <= 0) {
        return;
    }

    const float terrainAmp = std::clamp(config.terrainAmplitude, 0.2F, 2.0F);
    const float soilScale = std::clamp(config.soilDepthScale, 0.4F, 2.0F);
    std::vector<int> surfaceY = buildSurfaceProfile(width, height, seed, terrainAmp);
    const int verticalInset = height / 8;
    for (int x = 0; x < width; ++x) {
        surfaceY[static_cast<std::size_t>(x)] = std::max(0, surfaceY[static_cast<std::size_t>(x)] - verticalInset);
        paintColumn(world, x, surfaceY[static_cast<std::size_t>(x)], seed, soilScale);
    }

    carveCaves(world, seed, config);
    placeOres(world, seed, config);
    carveDragonDen(world, dragonDenInfo(world, seed));

    std::mt19937 rng{static_cast<std::uint32_t>(width * 977 + height * 131 + seed)};
    scatterSurfaceTrees(world, surfaceY, rng, config.treeDensity);
}

WorldGenerator::DragonDenInfo WorldGenerator::dragonDenInfo(const World& world, std::uint32_t seed) const {
    const int width = world.width();
    const int height = world.height();
    if (width < 40 || height < 40) {
        return {};
    }
    const int radiusX = std::clamp(width / 9, 16, 32);
    const int radiusY = std::clamp(height / 14, 10, 22);
    const int minX = radiusX + 6;
    const int maxX = std::max(minX + 1, width - radiusX - 6);
    const int minY = std::max(static_cast<int>(height * 0.65F), radiusY + 6);
    const int maxY = std::max(minY + 1, static_cast<int>(height * 0.88F));
    std::mt19937 rng{static_cast<std::uint32_t>(seed ^ 0xBADC0DEU ^ (width * 31U) ^ (height * 131U))};
    std::uniform_int_distribution<int> xDist(minX, maxX);
    std::uniform_int_distribution<int> yDist(minY, maxY);
    DragonDenInfo info{};
    info.centerX = xDist(rng);
    info.centerY = yDist(rng);
    info.radiusX = radiusX;
    info.radiusY = radiusY;
    return info;
}

void WorldGenerator::carveCaves(World& world, std::uint32_t seed, const WorldGenConfig& config) {
    if (world.width() < 16 || world.height() < 16) {
        return;
    }

    std::mt19937 rng{static_cast<std::uint32_t>(1337 + seed)};
    std::uniform_int_distribution<int> startX{8, world.width() - 8};
    std::uniform_int_distribution<int> startY{world.height() / 3, world.height() - 8};
    const float caveScale = std::clamp(config.caveDensity, 0.3F, 2.5F);
    const int lengthMin = std::max(30, static_cast<int>(std::round(80.0F * caveScale)));
    const int lengthMax = std::max(lengthMin + 20, static_cast<int>(std::round(200.0F * caveScale)));
    std::uniform_int_distribution<int> lengthDist{lengthMin, lengthMax};
    std::uniform_int_distribution<int> radiusDist{2, 4};
    std::uniform_real_distribution<float> angleDist{0.0F, 2.0F * 3.1415926F};
    std::uniform_real_distribution<float> turnDist{-0.35F, 0.35F};

    const int baseWorms = std::max(3, world.width() / 32);
    const int wormCount = std::max(1, static_cast<int>(std::round(static_cast<float>(baseWorms) * caveScale)));
    for (int i = 0; i < wormCount; ++i) {
        float x = static_cast<float>(startX(rng));
        float y = static_cast<float>(startY(rng));
        float angle = angleDist(rng);
        int radius = radiusDist(rng);
        const int length = lengthDist(rng);

        for (int step = 0; step < length; ++step) {
            carveCircle(world, static_cast<int>(x), static_cast<int>(y), radius);

            x += std::cos(angle) * 1.5F;
            y += std::sin(angle) * 1.2F;
            angle += turnDist(rng);

            radius += static_cast<int>(turnDist(rng) * 2.0F);
            radius = std::clamp(radius, 1, 5);

            if (x <= 4 || x >= static_cast<float>(world.width() - 4) || y <= static_cast<float>(world.height() / 5)) {
                break;
            }
            if (y >= static_cast<float>(world.height() - 4)) {
                y = static_cast<float>(world.height() - 5);
                angle = -std::abs(angle);
            }
        }
    }
}

void WorldGenerator::carveDragonDen(World& world, const DragonDenInfo& info) {
    if (info.radiusX <= 0 || info.radiusY <= 0) {
        return;
    }
    std::mt19937 rng{static_cast<std::uint32_t>(info.centerX * 977U + info.centerY * 131U)};
    std::uniform_real_distribution<float> offsetDist(-0.6F, 0.6F);
    const int oreClusters = 7;
    const int oreRadius = 3;
    for (int dy = -info.radiusY; dy <= info.radiusY; ++dy) {
        for (int dx = -info.radiusX; dx <= info.radiusX; ++dx) {
            const float nx = static_cast<float>(dx) / static_cast<float>(info.radiusX);
            const float ny = static_cast<float>(dy) / static_cast<float>(info.radiusY);
            const float dist = nx * nx + ny * ny;
            if (dist > 1.0F) {
                continue;
            }
            const int x = info.centerX + dx;
            const int y = info.centerY + dy;
            if (x < 1 || y < 1 || x >= world.width() - 1 || y >= world.height() - 1) {
                continue;
            }
            if (dist >= 0.86F) {
                world.setTile(x, y, TileType::GoldOre, true);
            } else {
                world.setTile(x, y, TileType::Air, false);
            }
        }
    }

    for (int i = 0; i < oreClusters; ++i) {
        const float baseX = static_cast<float>(info.centerX)
            + (static_cast<float>(info.radiusX) - 2.0F) * (0.6F + offsetDist(rng));
        const float baseY = static_cast<float>(info.centerY)
            + (static_cast<float>(info.radiusY) - 2.0F) * (0.6F + offsetDist(rng));
        for (int oy = -oreRadius; oy <= oreRadius; ++oy) {
            for (int ox = -oreRadius; ox <= oreRadius; ++ox) {
                if (ox * ox + oy * oy > oreRadius * oreRadius) {
                    continue;
                }
                const int x = static_cast<int>(std::round(baseX + static_cast<float>(ox)));
                const int y = static_cast<int>(std::round(baseY + static_cast<float>(oy)));
                if (x < 1 || y < 1 || x >= world.width() - 1 || y >= world.height() - 1) {
                    continue;
                }
                if (!world.tile(x, y).active()) {
                    continue;
                }
                world.setTileType(x, y, TileType::GoldOre);
            }
        }
    }
}

void WorldGenerator::placeOres(World& world, std::uint32_t seed, const WorldGenConfig& config) {
    std::mt19937 rng{static_cast<std::uint32_t>(4242 + seed)};
    const float oreScale = std::clamp(config.oreDensity, 0.3F, 2.5F);
    const std::array<OreConfig, 3> configs{{
        {TileType::CopperOre, 0.30F, 0.60F, 20, 14, 10, 18, 1, 2},
        {TileType::IronOre, 0.55F, 0.88F, 16, 18, 10, 20, 1, 2},
        {TileType::GoldOre, 0.78F, 0.98F, 10, 22, 8, 16, 1, 2},
    }};

    for (const auto& oreConfig : configs) {
        OreConfig adjusted = oreConfig;
        adjusted.minVeins = std::max(1, static_cast<int>(std::round(static_cast<float>(oreConfig.minVeins) * oreScale)));
        adjusted.veinScale = std::max(1, static_cast<int>(std::round(static_cast<float>(oreConfig.veinScale) / oreScale)));
        placeOreVeins(world, adjusted, rng);
    }
}

void WorldGenerator::carveCircle(World& world, int centerX, int centerY, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }
            const int x = centerX + dx;
            const int y = centerY + dy;
            if (x >= 0 && x < world.width() && y >= 0 && y < world.height()) {
                world.setTile(x, y, TileType::Air, false);
            }
        }
    }
}

} // namespace terraria::world
