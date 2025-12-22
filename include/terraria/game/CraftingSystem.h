#pragma once

#include "terraria/core/Application.h"
#include "terraria/entities/Player.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/HudState.h"
#include "terraria/world/World.h"

#include <array>
#include <vector>

namespace terraria::game {

class CraftingSystem {
public:
    CraftingSystem(const core::AppConfig& config, entities::Player& player, input::IInputSystem& input);

    void handleInput(bool inventoryOpen);
    void handlePointerInput(bool inventoryOpen, bool suppressClick);
    void update(float dt);
    void fillHud(rendering::HudState& hud);

private:
    struct CraftIngredient {
        world::TileType type{world::TileType::Air};
        int count{0};
    };

    struct CraftingRecipe {
        bool outputIsTool{false};
        bool outputIsArmor{false};
        bool outputIsAccessory{false};
        world::TileType output{world::TileType::Air};
        entities::ToolKind toolKind{entities::ToolKind::Pickaxe};
        entities::ToolTier toolTier{entities::ToolTier::None};
        entities::ArmorId armorId{entities::ArmorId::None};
        entities::AccessoryId accessoryId{entities::AccessoryId::None};
        int outputCount{0};
        std::array<CraftIngredient, 2> ingredients{};
        int ingredientCount{0};
    };

    struct CraftLayout {
        int panelX{0};
        int panelY{0};
        int panelWidth{0};
        int rowHeight{0};
        int rowSpacing{0};
        int visibleRows{1};
        int inventoryTop{0};
        int trackX{0};
        int trackY{0};
        int trackWidth{6};
        int trackHeight{0};
        int thumbY{0};
        int thumbHeight{0};
        bool scrollbarVisible{false};
    };

    void updateCraftLayoutMetrics(int recipeCount);
    void ensureCraftSelectionVisible(int recipeCount);
    bool craftSelectedRecipe();
    void updateCraftScrollbarDrag(int mouseY, int recipeCount);
    int craftRecipeIndexAt(int mouseX, int mouseY) const;
    bool tryCraft(const CraftingRecipe& recipe);
    bool canCraft(const CraftingRecipe& recipe) const;
    int totalCraftRecipes() const;

    const core::AppConfig& config_;
    entities::Player& player_;
    input::IInputSystem& input_;
    int craftSelection_{0};
    float craftCooldown_{0.0F};
    std::vector<CraftingRecipe> craftingRecipes_{};
    int craftScrollOffset_{0};
    bool craftScrollbarDragging_{false};
    float craftScrollbarGrabOffset_{0.0F};
    CraftLayout craftLayout_{};
};

} // namespace terraria::game
