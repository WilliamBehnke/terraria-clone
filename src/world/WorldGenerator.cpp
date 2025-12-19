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

std::vector<int> buildSurfaceProfile(int width, int height) {
    std::vector<int> surface(static_cast<std::size_t>(width), height / 3);
    const int chunkSize = 192;
    const int chunkCount = width / chunkSize + 2;
    std::vector<float> chunkHeights(static_cast<std::size_t>(chunkCount));
    for (int i = 0; i < chunkCount; ++i) {
        const int sampleX = i * chunkSize;
        const float broad = octaveNoise(sampleX, 2001, 0.00005F, 3);
        const float mesas = octaveNoise(sampleX, 3311, 0.00018F, 2);
        chunkHeights[static_cast<std::size_t>(i)] = 0.32F + broad * 0.5F + (mesas - 0.5F) * 0.2F;
    }

    for (int x = 0; x < width; ++x) {
        const int chunkIndex = x / chunkSize;
        const float t = static_cast<float>(x % chunkSize) / static_cast<float>(chunkSize);
        float normalized = std::lerp(chunkHeights[static_cast<std::size_t>(chunkIndex)],
                                     chunkHeights[static_cast<std::size_t>(chunkIndex + 1)],
                                     t);

        const float continental = octaveNoise(x, 7231, 0.00035F, 4) - 0.5F;
        const float rolling = octaveNoise(x, 9127, 0.0009F, 4) - 0.5F;
        const float ridge = std::sin(static_cast<float>(x) * 0.00045F) * 0.18F;
        normalized += continental * 0.2F + rolling * 0.12F + ridge;

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

void paintColumn(World& world, int x, int surfaceY) {
    const int height = world.height();
    const int soilMin = 16;
    const int soilMax = 28;
    const int surface = std::clamp(surfaceY, 0, std::max(0, height - 2));

    const int soilDepth = soilMin
        + static_cast<int>(hashNoise(x * 37, 9001) * static_cast<float>(soilMax - soilMin));
    const int transitionDepth = soilDepth + 18;
    const bool mountainTop = surface <= height / 6;

    for (int y = 0; y < height; ++y) {
        if (y < surface) {
            world.setTile(x, y, TileType::Air, false);
            continue;
        }

        const int depth = y - surface;
        if (depth == 0) {
            if (mountainTop && hashNoise(x * 73, 18511) > 0.7F) {
                world.setTile(x, y, TileType::Stone, true);
            } else {
                world.setTile(x, y, TileType::Grass, true);
            }
            continue;
        }

        if (depth <= soilDepth) {
            world.setTile(x, y, TileType::Dirt, true);
        } else if (depth <= transitionDepth) {
            const float blend = hashNoise(x * 131 + depth * 53, 7001);
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

void scatterSurfaceTrees(World& world, const std::vector<int>& surfaceY, std::mt19937& rng) {
    if (world.width() < 8) {
        return;
    }

    std::uniform_int_distribution<int> gapDist{4, 9};
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
    const int width = world.width();
    const int height = world.height();
    if (width <= 0 || height <= 0) {
        return;
    }

    std::vector<int> surfaceY = buildSurfaceProfile(width, height);
    const int verticalInset = height / 8;
    for (int x = 0; x < width; ++x) {
        surfaceY[static_cast<std::size_t>(x)] = std::max(0, surfaceY[static_cast<std::size_t>(x)] - verticalInset);
        paintColumn(world, x, surfaceY[static_cast<std::size_t>(x)]);
    }

    carveCaves(world);
    placeOres(world);

    std::mt19937 rng{static_cast<std::uint32_t>(width * 977 + height * 131)};
    scatterSurfaceTrees(world, surfaceY, rng);
}

void WorldGenerator::carveCaves(World& world) {
    if (world.width() < 16 || world.height() < 16) {
        return;
    }

    std::mt19937 rng{1337};
    std::uniform_int_distribution<int> startX{8, world.width() - 8};
    std::uniform_int_distribution<int> startY{world.height() / 3, world.height() - 8};
    std::uniform_int_distribution<int> lengthDist{80, 200};
    std::uniform_int_distribution<int> radiusDist{2, 4};
    std::uniform_real_distribution<float> angleDist{0.0F, 2.0F * 3.1415926F};
    std::uniform_real_distribution<float> turnDist{-0.35F, 0.35F};

    const int wormCount = std::max(3, world.width() / 32);
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

void WorldGenerator::placeOres(World& world) {
    std::mt19937 rng{4242};
    const std::array<OreConfig, 3> configs{{
        {TileType::CopperOre, 0.30F, 0.60F, 20, 14, 10, 18, 1, 2},
        {TileType::IronOre, 0.55F, 0.88F, 16, 18, 10, 20, 1, 2},
        {TileType::GoldOre, 0.78F, 0.98F, 10, 22, 8, 16, 1, 2},
    }};

    for (const auto& config : configs) {
        placeOreVeins(world, config, rng);
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
