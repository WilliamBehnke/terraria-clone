#include "terraria/game/CraftingSystem.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace terraria::game {

namespace {
constexpr float kCraftCooldown = 0.12F;
}

CraftingSystem::CraftingSystem(const core::AppConfig& config, entities::Player& player, input::IInputSystem& input)
    : config_{config},
      player_{player},
      input_{input} {
    craftingRecipes_.reserve(32);
    const auto addTileRecipe = [&](world::TileType output, int count, std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.output = output;
        recipe.outputCount = count;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    const auto addToolRecipe = [&](entities::ToolKind kind,
                                   entities::ToolTier tier,
                                   std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.outputIsTool = true;
        recipe.toolKind = kind;
        recipe.toolTier = tier;
        recipe.outputCount = 1;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    const auto addArmorRecipe = [&](entities::ArmorId armorId, std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.outputIsArmor = true;
        recipe.armorId = armorId;
        recipe.outputCount = 1;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    const auto addAccessoryRecipe = [&](entities::AccessoryId accessoryId,
                                        std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.outputIsAccessory = true;
        recipe.accessoryId = accessoryId;
        recipe.outputCount = 1;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    addTileRecipe(world::TileType::WoodPlank, 1, {CraftIngredient{world::TileType::Wood, 4}});
    addTileRecipe(world::TileType::StoneBrick, 1, {CraftIngredient{world::TileType::Stone, 4}});
    addTileRecipe(world::TileType::Arrow, 10, {CraftIngredient{world::TileType::Wood, 1}});

    addToolRecipe(entities::ToolKind::Pickaxe, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 8}});
    addToolRecipe(entities::ToolKind::Axe, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 6}});
    addToolRecipe(entities::ToolKind::Shovel, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 6}});
    addToolRecipe(entities::ToolKind::Hoe, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 5}});
    addToolRecipe(entities::ToolKind::Sword, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 7}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 10}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 8}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Shovel,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 8}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Hoe,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 7}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 9}, CraftIngredient{world::TileType::Wood, 2}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 10}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 8}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Shovel,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 8}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Hoe,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 7}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 9}, CraftIngredient{world::TileType::Wood, 2}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 12}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 10}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Shovel,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 10}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Hoe,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 9}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 11}, CraftIngredient{world::TileType::Wood, 2}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 14}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 12}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Shovel,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 12}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Hoe,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 11}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 12}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Bow,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 16}, CraftIngredient{world::TileType::Wood, 6}});

    addArmorRecipe(entities::ArmorId::CopperHelmet,
                   {CraftIngredient{world::TileType::CopperOre, 12}, CraftIngredient{world::TileType::Wood, 3}});
    addArmorRecipe(entities::ArmorId::CopperChest,
                   {CraftIngredient{world::TileType::CopperOre, 18}, CraftIngredient{world::TileType::Wood, 4}});
    addArmorRecipe(entities::ArmorId::CopperLeggings,
                   {CraftIngredient{world::TileType::CopperOre, 14}, CraftIngredient{world::TileType::Wood, 3}});
    addArmorRecipe(entities::ArmorId::IronHelmet,
                   {CraftIngredient{world::TileType::IronOre, 14}, CraftIngredient{world::TileType::Wood, 3}});
    addArmorRecipe(entities::ArmorId::IronChest,
                   {CraftIngredient{world::TileType::IronOre, 22}, CraftIngredient{world::TileType::Wood, 4}});
    addArmorRecipe(entities::ArmorId::IronLeggings,
                   {CraftIngredient{world::TileType::IronOre, 16}, CraftIngredient{world::TileType::Wood, 3}});
    addArmorRecipe(entities::ArmorId::GoldHelmet,
                   {CraftIngredient{world::TileType::GoldOre, 16}, CraftIngredient{world::TileType::Wood, 4}});
    addArmorRecipe(entities::ArmorId::GoldChest,
                   {CraftIngredient{world::TileType::GoldOre, 24}, CraftIngredient{world::TileType::Wood, 5}});
    addArmorRecipe(entities::ArmorId::GoldLeggings,
                   {CraftIngredient{world::TileType::GoldOre, 20}, CraftIngredient{world::TileType::Wood, 4}});

    addAccessoryRecipe(entities::AccessoryId::FleetBoots,
                       {CraftIngredient{world::TileType::IronOre, 8}, CraftIngredient{world::TileType::Wood, 4}});
    addAccessoryRecipe(entities::AccessoryId::VitalityCharm,
                       {CraftIngredient{world::TileType::CopperOre, 10}, CraftIngredient{world::TileType::Wood, 3}});
    addAccessoryRecipe(entities::AccessoryId::MinerRing,
                       {CraftIngredient{world::TileType::Stone, 20}, CraftIngredient{world::TileType::CopperOre, 6}});
}

void CraftingSystem::update(float dt) {
    craftCooldown_ = std::max(0.0F, craftCooldown_ - dt);
}

int CraftingSystem::totalCraftRecipes() const {
    return static_cast<int>(craftingRecipes_.size());
}

void CraftingSystem::handleInput(bool inventoryOpen) {
    const auto& inputState = input_.state();
    const int recipeCount = totalCraftRecipes();
    if (inventoryOpen) {
        updateCraftLayoutMetrics(recipeCount);
        if (recipeCount > 0) {
            int delta = 0;
            if (inputState.craftPrev) {
                delta -= 1;
            }
            if (inputState.craftNext) {
                delta += 1;
            }
            if (delta != 0) {
                craftSelection_ = (craftSelection_ + delta + recipeCount) % recipeCount;
                ensureCraftSelectionVisible(recipeCount);
                updateCraftLayoutMetrics(recipeCount);
            }
            if (inputState.craftExecute) {
                craftSelectedRecipe();
            }
        }
    } else {
        craftScrollbarDragging_ = false;
        craftScrollOffset_ = 0;
    }
}

void CraftingSystem::handlePointerInput(bool inventoryOpen, bool suppressClick) {
    if (!inventoryOpen) {
        craftScrollbarDragging_ = false;
        return;
    }

    const auto& state = input_.state();
    const int recipeCount = totalCraftRecipes();
    if (recipeCount <= 0) {
        craftScrollbarDragging_ = false;
        return;
    }
    if (!state.breakHeld) {
        craftScrollbarDragging_ = false;
    }

    if (state.inventoryClick && craftLayout_.scrollbarVisible) {
        const int mouseX = state.mouseX;
        const int mouseY = state.mouseY;
        const bool insideThumb = mouseX >= craftLayout_.trackX
            && mouseX <= craftLayout_.trackX + craftLayout_.trackWidth
            && mouseY >= craftLayout_.thumbY
            && mouseY <= craftLayout_.thumbY + craftLayout_.thumbHeight;
        if (insideThumb) {
            craftScrollbarDragging_ = true;
            craftScrollbarGrabOffset_ = static_cast<float>(mouseY - craftLayout_.thumbY);
        }
    }

    if (craftScrollbarDragging_ && state.breakHeld) {
        updateCraftScrollbarDrag(state.mouseY, recipeCount);
    }

    if (suppressClick) {
        return;
    }

    if (state.inventoryClick && recipeCount > 0) {
        const int index = craftRecipeIndexAt(state.mouseX, state.mouseY);
        if (index >= 0) {
            craftSelection_ = index;
            ensureCraftSelectionVisible(recipeCount);
            updateCraftLayoutMetrics(recipeCount);
            craftSelectedRecipe();
        }
    }
}

bool CraftingSystem::canCraft(const CraftingRecipe& recipe) const {
    for (int i = 0; i < recipe.ingredientCount; ++i) {
        const auto& ingredient = recipe.ingredients[static_cast<std::size_t>(i)];
        if (player_.inventoryCount(ingredient.type) < ingredient.count) {
            return false;
        }
    }
    return true;
}

bool CraftingSystem::tryCraft(const CraftingRecipe& recipe) {
    if (!canCraft(recipe)) {
        return false;
    }
    const auto canStoreTile = [&](world::TileType type) {
        for (const auto& slot : player_.inventory()) {
            if (slot.isBlock() && slot.blockType == type && slot.count < entities::kMaxStackCount) {
                return true;
            }
            if (slot.empty()) {
                return true;
            }
        }
        return false;
    };
    const auto canStoreTool = [&](entities::ToolKind kind, entities::ToolTier tier) {
        for (const auto& slot : player_.inventory()) {
            if (slot.empty()) {
                return true;
            }
            if (slot.isTool() && slot.toolKind == kind && slot.toolTier == tier) {
                return true;
            }
        }
        return false;
    };
    const auto canStoreArmor = [&]() {
        for (const auto& slot : player_.inventory()) {
            if (slot.empty()) {
                return true;
            }
        }
        return false;
    };
    const auto canStoreAccessory = [&]() {
        for (const auto& slot : player_.inventory()) {
            if (slot.empty()) {
                return true;
            }
        }
        return false;
    };

    if (recipe.outputIsTool) {
        if (!canStoreTool(recipe.toolKind, recipe.toolTier)) {
            return false;
        }
    } else if (recipe.outputIsArmor) {
        if (!canStoreArmor()) {
            return false;
        }
    } else if (recipe.outputIsAccessory) {
        if (!canStoreAccessory()) {
            return false;
        }
    } else if (!canStoreTile(recipe.output)) {
        return false;
    }

    for (int i = 0; i < recipe.ingredientCount; ++i) {
        const auto& ingredient = recipe.ingredients[static_cast<std::size_t>(i)];
        player_.consumeFromInventory(ingredient.type, ingredient.count);
    }
    if (recipe.outputIsTool) {
        return player_.addTool(recipe.toolKind, recipe.toolTier);
    }
    if (recipe.outputIsArmor) {
        return player_.addArmor(recipe.armorId);
    }
    if (recipe.outputIsAccessory) {
        return player_.addAccessory(recipe.accessoryId);
    }

    bool success = true;
    for (int i = 0; i < recipe.outputCount; ++i) {
        if (!player_.addToInventory(recipe.output)) {
            success = false;
        }
    }
    return success;
}

void CraftingSystem::updateCraftLayoutMetrics(int recipeCount) {
    CraftLayout layout{};
    const int margin = rendering::kInventorySlotSpacing;
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int rows = rendering::kInventoryRows;
    const int baseY = config_.windowHeight - slotHeight - margin;
    layout.inventoryTop = baseY - (rows - 1) * (slotHeight + spacing);
    const int minPanelWidth = 260;
    const int scrollbarGap = 4;
    layout.rowHeight = 26;
    layout.rowSpacing = 4;
    const int rowSpan = layout.rowHeight + layout.rowSpacing;
    const int desiredPanelX = margin + rendering::kInventoryColumns * (slotWidth + spacing) + spacing;
    layout.panelX = desiredPanelX;
    const int rightMargin = 1;
    const int minTotalWidth = minPanelWidth + layout.trackWidth + scrollbarGap;
    const int totalWidth = std::max(minTotalWidth, config_.windowWidth - layout.panelX - rightMargin);
    layout.panelWidth = std::max(minPanelWidth, totalWidth - layout.trackWidth - scrollbarGap);
    layout.panelY = layout.inventoryTop;
    const int contentHeight = std::max(1, config_.windowHeight - layout.panelY - margin);
    layout.visibleRows = std::max(1, contentHeight / rowSpan);
    layout.trackHeight = layout.visibleRows * rowSpan - layout.rowSpacing;
    layout.trackX = layout.panelX + layout.panelWidth + scrollbarGap;
    layout.trackY = layout.panelY;
    const int maxStart = std::max(0, recipeCount - layout.visibleRows);
    craftScrollOffset_ = std::clamp(craftScrollOffset_, 0, maxStart);
    layout.scrollbarVisible = maxStart > 0;
    if (layout.scrollbarVisible) {
        const float visibleRatio = static_cast<float>(layout.visibleRows) / static_cast<float>(recipeCount);
        const int thumbMin = 24;
        layout.thumbHeight = std::max(thumbMin, static_cast<int>(std::round(static_cast<float>(layout.trackHeight) * visibleRatio)));
        const float ratio =
            (maxStart > 0) ? static_cast<float>(craftScrollOffset_) / static_cast<float>(maxStart) : 0.0F;
        const int maxThumbOffset = std::max(0, layout.trackHeight - layout.thumbHeight);
        layout.thumbY = layout.trackY + static_cast<int>(std::round(ratio * static_cast<float>(maxThumbOffset)));
    } else {
        layout.thumbHeight = layout.trackHeight;
        layout.thumbY = layout.trackY;
    }
    craftLayout_ = layout;
}

void CraftingSystem::ensureCraftSelectionVisible(int recipeCount) {
    if (recipeCount <= 0) {
        craftScrollOffset_ = 0;
        return;
    }
    craftSelection_ = std::clamp(craftSelection_, 0, recipeCount - 1);
    const int visible = craftLayout_.visibleRows;
    const int maxStart = std::max(0, recipeCount - visible);
    if (craftSelection_ < craftScrollOffset_) {
        craftScrollOffset_ = craftSelection_;
    } else if (craftSelection_ >= craftScrollOffset_ + visible) {
        craftScrollOffset_ = craftSelection_ - visible + 1;
    }
    craftScrollOffset_ = std::clamp(craftScrollOffset_, 0, maxStart);
}

bool CraftingSystem::craftSelectedRecipe() {
    if (craftCooldown_ > 0.0F) {
        return false;
    }
    const int recipeCount = totalCraftRecipes();
    craftSelection_ = std::clamp(craftSelection_, 0, recipeCount - 1);
    if (craftSelection_ < 0 || craftSelection_ >= recipeCount) {
        return false;
    }
    if (tryCraft(craftingRecipes_[static_cast<std::size_t>(craftSelection_)])) {
        craftCooldown_ = kCraftCooldown;
        return true;
    }
    return false;
}

void CraftingSystem::updateCraftScrollbarDrag(int mouseY, int recipeCount) {
    if (!craftScrollbarDragging_ || !craftLayout_.scrollbarVisible || recipeCount <= 0) {
        return;
    }
    const int trackSpan = craftLayout_.trackHeight - craftLayout_.thumbHeight;
    if (trackSpan <= 0) {
        return;
    }
    int thumbOffset = mouseY - craftLayout_.trackY - static_cast<int>(craftScrollbarGrabOffset_);
    thumbOffset = std::clamp(thumbOffset, 0, trackSpan);
    const float ratio = static_cast<float>(thumbOffset) / static_cast<float>(trackSpan);
    const int maxStart = std::max(0, recipeCount - craftLayout_.visibleRows);
    craftScrollOffset_ = std::clamp(static_cast<int>(std::round(ratio * static_cast<float>(maxStart))), 0, maxStart);
    updateCraftLayoutMetrics(recipeCount);
}

int CraftingSystem::craftRecipeIndexAt(int mouseX, int mouseY) const {
    const int recipeCount = totalCraftRecipes();
    if (recipeCount <= 0) {
        return -1;
    }
    if (mouseX < craftLayout_.panelX || mouseX > craftLayout_.panelX + craftLayout_.panelWidth) {
        return -1;
    }
    int rowY = craftLayout_.panelY;
    for (int row = 0; row < craftLayout_.visibleRows && (craftScrollOffset_ + row) < recipeCount; ++row) {
        const int top = rowY + row * (craftLayout_.rowHeight + craftLayout_.rowSpacing);
        const int bottom = top + craftLayout_.rowHeight;
        if (mouseY >= top && mouseY <= bottom) {
            return craftScrollOffset_ + row;
        }
    }
    return -1;
}

void CraftingSystem::fillHud(rendering::HudState& hud) {
    if (!hud.inventoryOpen) {
        hud.craftRecipeCount = 0;
        hud.craftSelection = 0;
        hud.craftScrollOffset = 0;
        hud.craftVisibleRows = 0;
        hud.craftPanelX = 0;
        hud.craftPanelY = 0;
        hud.craftPanelWidth = 0;
        hud.craftRowHeight = 0;
        hud.craftRowSpacing = 0;
        hud.craftScrollbarTrackX = 0;
        hud.craftScrollbarTrackY = 0;
        hud.craftScrollbarTrackHeight = 0;
        hud.craftScrollbarThumbY = 0;
        hud.craftScrollbarThumbHeight = 0;
        hud.craftScrollbarWidth = 0;
        hud.craftScrollbarVisible = false;
        hud.craftRecipes.clear();
        return;
    }

    hud.craftRecipeCount = totalCraftRecipes();
    hud.craftRecipes.assign(static_cast<std::size_t>(hud.craftRecipeCount), {});
    if (hud.craftRecipeCount > 0) {
        const int clampedSelection = std::clamp(craftSelection_, 0, hud.craftRecipeCount - 1);
        hud.craftSelection = clampedSelection;
        for (int i = 0; i < hud.craftRecipeCount; ++i) {
            const auto& recipe = craftingRecipes_[static_cast<std::size_t>(i)];
            auto& entry = hud.craftRecipes[static_cast<std::size_t>(i)];
            entry = {};
            entry.outputIsTool = recipe.outputIsTool;
            entry.outputIsArmor = recipe.outputIsArmor;
            entry.outputIsAccessory = recipe.outputIsAccessory;
            entry.outputType = recipe.output;
            entry.outputCount = recipe.outputCount;
            entry.toolKind = recipe.toolKind;
            entry.toolTier = recipe.toolTier;
            entry.armorSlot = ArmorSlotFor(recipe.armorId);
            entry.armorId = recipe.armorId;
            entry.accessoryId = recipe.accessoryId;
            entry.ingredientCount = recipe.ingredientCount;
            for (int ing = 0; ing < recipe.ingredientCount; ++ing) {
                entry.ingredientTypes[static_cast<std::size_t>(ing)] = recipe.ingredients[static_cast<std::size_t>(ing)].type;
                entry.ingredientCounts[static_cast<std::size_t>(ing)] = recipe.ingredients[static_cast<std::size_t>(ing)].count;
            }
            entry.canCraft = canCraft(recipe);
        }
    } else {
        hud.craftSelection = 0;
    }
    updateCraftLayoutMetrics(hud.craftRecipeCount);
    hud.craftScrollOffset = craftScrollOffset_;
    hud.craftVisibleRows = craftLayout_.visibleRows;
    hud.craftPanelX = craftLayout_.panelX;
    hud.craftPanelY = craftLayout_.panelY;
    hud.craftPanelWidth = craftLayout_.panelWidth;
    hud.craftRowHeight = craftLayout_.rowHeight;
    hud.craftRowSpacing = craftLayout_.rowSpacing;
    hud.craftScrollbarTrackX = craftLayout_.trackX;
    hud.craftScrollbarTrackY = craftLayout_.trackY;
    hud.craftScrollbarTrackHeight = craftLayout_.trackHeight;
    hud.craftScrollbarThumbY = craftLayout_.thumbY;
    hud.craftScrollbarThumbHeight = craftLayout_.thumbHeight;
    hud.craftScrollbarWidth = craftLayout_.trackWidth;
    hud.craftScrollbarVisible = craftLayout_.scrollbarVisible;
}

} // namespace terraria::game
