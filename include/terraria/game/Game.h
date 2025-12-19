#pragma once

#include "terraria/core/Application.h"
#include "terraria/entities/Player.h"
#include "terraria/entities/Tools.h"
#include "terraria/entities/Zombie.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/Renderer.h"
#include "terraria/world/World.h"
#include "terraria/world/WorldGenerator.h"

#include <array>
#include <chrono>
#include <random>
#include <vector>

namespace terraria::game {

class Game {
public:
    explicit Game(const core::AppConfig& config);

    void initialize();
    bool tick();
    void shutdown();

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

    void handleInput();
    void update(float dt);
    void render();
    bool collides(const entities::Vec2& pos) const;
    bool collidesAabb(const entities::Vec2& pos, float halfWidth, float height) const;
    bool isSolidTile(int x, int y) const;
    void resolveHorizontal(entities::Vec2& position, entities::Vec2& velocity);
    void resolveVertical(entities::Vec2& position, entities::Vec2& velocity);
    void resolveHorizontalAabb(entities::Vec2& position, entities::Vec2& velocity, float halfWidth, float height);
    void resolveVerticalAabb(entities::Vec2& position, entities::Vec2& velocity, float halfWidth, float height, bool& onGround);
    void processActions(float dt);
    bool cursorWorldTile(int& outX, int& outY) const;
    bool canBreakTile(int tileX, int tileY) const;
    bool canPlaceTile(int tileX, int tileY) const;
    bool withinPlacementRange(int tileX, int tileY) const;
    bool tileInsidePlayer(int tileX, int tileY) const;
    void handleBreaking(float dt);
    void handlePlacement(float dt);
    void handleCrafting(float dt);
    void handleInventoryInput();
    void handleInventoryClick(int slotIndex);
    int inventorySlotIndexAt(int mouseX, int mouseY) const;
    bool placeCarriedStack();
    bool carryingItem() const;
    void updateHudState();
    void toggleCameraMode();
    entities::Vec2 cameraFocus() const;
    entities::Vec2 clampCameraTarget(const entities::Vec2& desired) const;
    static bool isTreeTile(world::TileType type);
    void updateZombies(float dt);
    void applyZombiePhysics(entities::Zombie& zombie, float dt);
    void spawnZombie();
    bool attackZombiesAtCursor();
    bool zombiesOverlapPlayer(const entities::Zombie& zombie) const;
    bool aabbOverlap(const entities::Vec2& aPos, float aHalfWidth, float aHeight,
                     const entities::Vec2& bPos, float bHalfWidth, float bHeight) const;
    void updateDayNight(float dt);
    float normalizedTimeOfDay() const;
    void recordPerformanceMetrics(float frameMs, float updateMs, float renderMs);
    int totalCraftRecipes() const;
    void updateCraftLayoutMetrics(int recipeCount);
    void ensureCraftSelectionVisible(int recipeCount);
    void handleCraftingPointerInput();
    int craftRecipeIndexAt(int mouseX, int mouseY) const;
    bool craftSelectedRecipe();
    void updateCraftScrollbarDrag(int mouseY, int recipeCount);
    void updateEquipmentLayout();
    bool handleEquipmentInput();
    bool handleArmorSlotClick(int slotIndex);
    bool handleAccessorySlotClick(int slotIndex);
    int equipmentSlotAt(int mouseX, int mouseY, bool armor) const;
    bool tryCraft(const CraftingRecipe& recipe);
    bool canCraft(const CraftingRecipe& recipe) const;
    entities::ToolTier selectedToolTier(entities::ToolKind kind) const;
    bool hasRequiredPickaxe(world::TileType tileType) const;
    int activeSwordDamage() const;
    int activeRangedDamage() const;
    float breakSpeedMultiplier(world::TileType tileType) const;
    void startSwordSwing(float aimAngle);
    void updateSwordSwing(float dt);
    void spawnPlayerProjectile(const entities::Vec2& direction);
    void updateProjectiles(float dt);
    bool solidAtPosition(const entities::Vec2& pos) const;

    struct BreakState {
        bool active{false};
        int tileX{0};
        int tileY{0};
        float progress{0.0F};
    };

    struct SwordSwingState {
        bool active{false};
        float timer{0.0F};
        float duration{0.0F};
        float angleStart{0.0F};
        float angleEnd{0.0F};
        float radius{0.0F};
        entities::Vec2 center{};
        float halfWidth{0.0F};
        float halfHeight{0.0F};
    };

    struct Projectile {
        entities::Vec2 position{};
        entities::Vec2 velocity{};
        float lifetime{0.0F};
        float radius{0.2F};
        int damage{0};
        bool fromPlayer{false};
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

    struct EquipmentSlotRect {
        bool isArmor{false};
        int slotIndex{0};
        int x{0};
        int y{0};
        int w{0};
        int h{0};
    };

    core::AppConfig config_;
    world::World world_;
    world::WorldGenerator generator_;
    entities::Player player_;
    std::unique_ptr<rendering::IRenderer> renderer_;
    std::unique_ptr<input::IInputSystem> inputSystem_;
    BreakState breakState_{};
    float placeCooldown_{0.0F};
    int selectedHotbar_{0};
    rendering::HudState hudState_{};
    bool cameraMode_{false};
    entities::Vec2 cameraPosition_{};
    float cameraSpeed_{80.0F};
    std::vector<entities::Zombie> zombies_{};
    int nextZombieId_{1};
    SwordSwingState swordSwing_{};
    std::vector<int> swordSwingHitIds_{};
    std::vector<Projectile> projectiles_{};
    std::mt19937 zombieRng_;
    float zombieSpawnTimer_{0.0F};
    entities::Vec2 spawnPosition_{};
    float playerAttackCooldown_{0.0F};
    float timeOfDay_{0.0F};
    float dayLength_{180.0F};
    bool isNight_{false};
    int craftSelection_{0};
    float craftCooldown_{0.0F};
    std::vector<CraftingRecipe> craftingRecipes_{};
    bool inventoryOpen_{false};
    int hoveredInventorySlot_{-1};
    entities::InventorySlot carriedSlot_{};
    float moveInput_{0.0F};
    float jumpBufferTimer_{0.0F};
    float coyoteTimer_{0.0F};
    bool jumpHeld_{false};
    bool prevJumpInput_{false};
    float perfFrameTimeMs_{0.0F};
    float perfUpdateTimeMs_{0.0F};
    float perfRenderTimeMs_{0.0F};
    float perfFps_{0.0F};
    int craftScrollOffset_{0};
    bool craftScrollbarDragging_{false};
    float craftScrollbarGrabOffset_{0.0F};
    CraftLayout craftLayout_{};
    std::array<EquipmentSlotRect, rendering::kTotalEquipmentSlots> equipmentRects_{};
    int hoveredArmorSlot_{-1};
    int hoveredAccessorySlot_{-1};
};

} // namespace terraria::game
