#pragma once

#include "terraria/entities/Tools.h"
#include "terraria/world/Tile.h"

#include <array>

namespace terraria::rendering {

constexpr int kMaxHotbarSlots = 10;
constexpr int kMaxCraftRecipes = 20;

struct HotbarSlotHud {
    bool isTool{false};
    world::TileType tileType{world::TileType::Air};
    entities::ToolKind toolKind{entities::ToolKind::Pickaxe};
    entities::ToolTier toolTier{entities::ToolTier::None};
    int count{0};
};

struct CraftHudEntry {
    bool outputIsTool{false};
    world::TileType outputType{world::TileType::Air};
    entities::ToolKind toolKind{entities::ToolKind::Pickaxe};
    entities::ToolTier toolTier{entities::ToolTier::None};
    int outputCount{0};
    std::array<world::TileType, 2> ingredientTypes{};
    std::array<int, 2> ingredientCounts{};
    int ingredientCount{0};
    bool canCraft{false};
};

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
    std::array<HotbarSlotHud, kMaxHotbarSlots> hotbarSlots{};
    bool useCamera{false};
    float cameraX{0.0F};
    float cameraY{0.0F};
    int playerHealth{0};
    int playerMaxHealth{0};
    int zombieCount{0};
    float dayProgress{0.0F};
    bool isNight{false};
    int craftSelection{0};
    int craftRecipeCount{0};
    std::array<CraftHudEntry, kMaxCraftRecipes> craftRecipes{};
};

} // namespace terraria::rendering
