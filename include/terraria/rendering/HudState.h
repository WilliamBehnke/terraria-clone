#pragma once

#include "terraria/world/Tile.h"

#include <array>

namespace terraria::rendering {

constexpr int kMaxHotbarSlots = 8;

struct HudState {
    bool breakActive{false};
    int breakTileX{0};
    int breakTileY{0};
    float breakProgress{0.0F};
    int selectedSlot{0};
    bool cursorHighlight{false};
    bool cursorInRange{false};
    bool cursorCanPlace{false};
    int cursorTileX{0};
    int cursorTileY{0};
    int hotbarCount{0};
    std::array<world::TileType, kMaxHotbarSlots> hotbarTypes{};
    bool useCamera{false};
    float cameraX{0.0F};
    float cameraY{0.0F};
    int playerHealth{0};
    int playerMaxHealth{0};
    int zombieCount{0};
    float dayProgress{0.0F};
    bool isNight{false};
};

} // namespace terraria::rendering
