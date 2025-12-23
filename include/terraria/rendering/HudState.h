#pragma once

#include "terraria/entities/Tools.h"
#include "terraria/world/Tile.h"

#include <array>
#include <string>
#include <vector>

namespace terraria::rendering {

constexpr int kMaxHotbarSlots = 9;
constexpr int kInventoryRows = 4;
constexpr int kInventoryColumns = kMaxHotbarSlots;
constexpr int kMaxInventorySlots = kInventoryRows * kInventoryColumns;
constexpr int kInventorySlotWidth = 90;
constexpr int kInventorySlotHeight = 56;
constexpr int kInventorySlotSpacing = 10;
constexpr int kArmorSlotCount = static_cast<int>(entities::ArmorSlot::Count);
constexpr int kAccessorySlotCount = entities::kAccessorySlotCount;
constexpr int kTotalEquipmentSlots = kArmorSlotCount + kAccessorySlotCount;
constexpr int kMaxProjectiles = 64;
constexpr int kMaxFlyingEnemies = 24;
constexpr int kMaxEnemyProjectiles = 64;
constexpr int kMaxWorms = 24;

struct HotbarSlotHud {
    bool isTool{false};
    bool isArmor{false};
    bool isAccessory{false};
    world::TileType tileType{world::TileType::Air};
    entities::ToolKind toolKind{entities::ToolKind::Pickaxe};
    entities::ToolTier toolTier{entities::ToolTier::None};
    entities::ArmorId armorId{entities::ArmorId::None};
    entities::AccessoryId accessoryId{entities::AccessoryId::None};
    int count{0};
};

struct CraftHudEntry {
    bool outputIsTool{false};
    bool outputIsArmor{false};
    bool outputIsAccessory{false};
    world::TileType outputType{world::TileType::Air};
    entities::ToolKind toolKind{entities::ToolKind::Pickaxe};
    entities::ToolTier toolTier{entities::ToolTier::None};
    entities::ArmorSlot armorSlot{entities::ArmorSlot::Head};
    entities::ArmorId armorId{entities::ArmorId::None};
    entities::AccessoryId accessoryId{entities::AccessoryId::None};
    int outputCount{0};
    std::array<world::TileType, 2> ingredientTypes{};
    std::array<int, 2> ingredientCounts{};
    int ingredientCount{0};
    bool canCraft{false};
};

struct EquipmentSlotHud {
    bool isArmor{false};
    int slotIndex{0};
    int x{0};
    int y{0};
    int w{0};
    int h{0};
    bool occupied{false};
    bool hovered{false};
    entities::ArmorId armorId{entities::ArmorId::None};
    entities::AccessoryId accessoryId{entities::AccessoryId::None};
};

struct ProjectileHudEntry {
    bool active{false};
    float x{0.0F};
    float y{0.0F};
    float radius{0.0F};
};

struct EnemyProjectileHudEntry {
    bool active{false};
    float x{0.0F};
    float y{0.0F};
    float radius{0.0F};
    bool isFlame{false};
};

struct FlyingEnemyHudEntry {
    bool active{false};
    float x{0.0F};
    float y{0.0F};
    float radius{0.0F};
    int health{0};
    int maxHealth{0};
};

struct WormHudEntry {
    bool active{false};
    float x{0.0F};
    float y{0.0F};
    float vx{0.0F};
    float vy{0.0F};
    float radius{0.0F};
    int health{0};
    int maxHealth{0};
};

struct DragonHudEntry {
    bool active{false};
    float x{0.0F};
    float y{0.0F};
    float halfWidth{0.0F};
    float height{0.0F};
    float wingPhase{0.0F};
    float facing{1.0F};
    int health{0};
    int maxHealth{0};
};

struct DamageNumberHud {
    float x{0.0F};
    float y{0.0F};
    int amount{0};
    float alpha{1.0F};
    bool isPlayer{false};
    bool isLoot{false};
};

struct ChatLineHud {
    std::string text{};
    bool isSystem{false};
    float age{0.0F};
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
    int playerTileX{0};
    int playerTileY{0};
    bool inventoryOpen{false};
    int inventorySlotCount{0};
    std::array<HotbarSlotHud, kMaxInventorySlots> inventorySlots{};
    int hoveredInventorySlot{-1};
    bool carryingItem{false};
    HotbarSlotHud carriedItem{};
    int hotbarCount{0};
    std::array<HotbarSlotHud, kMaxHotbarSlots> hotbarSlots{};
    HotbarSlotHud ammoSlot{};
    bool ammoSlotHovered{false};
    bool ammoSlotVisible{false};
    int ammoSlotX{0};
    int ammoSlotY{0};
    int ammoSlotW{0};
    int ammoSlotH{0};
    bool trashSlotHovered{false};
    bool trashSlotVisible{false};
    int trashSlotX{0};
    int trashSlotY{0};
    int trashSlotW{0};
    int trashSlotH{0};
    bool useCamera{false};
    float cameraX{0.0F};
    float cameraY{0.0F};
    int playerHealth{0};
    int playerMaxHealth{0};
    int playerDefense{0};
    int zombieCount{0};
    float dayProgress{0.0F};
    bool isNight{false};
    int craftSelection{0};
    int craftRecipeCount{0};
    std::vector<CraftHudEntry> craftRecipes{};
    int craftScrollOffset{0};
    int craftVisibleRows{0};
    int craftPanelX{0};
    int craftPanelY{0};
    int craftPanelWidth{0};
    int craftRowHeight{0};
    int craftRowSpacing{0};
    int craftScrollbarTrackX{0};
    int craftScrollbarTrackY{0};
    int craftScrollbarTrackHeight{0};
    int craftScrollbarThumbY{0};
    int craftScrollbarThumbHeight{0};
    int craftScrollbarWidth{0};
    bool craftScrollbarVisible{false};
    int equipmentSlotCount{0};
    std::array<EquipmentSlotHud, kTotalEquipmentSlots> equipmentSlots{};
    float bowDrawProgress{0.0F};
    bool swordSwingActive{false};
    float swordSwingX{0.0F};
    float swordSwingY{0.0F};
    float swordSwingHalfWidth{0.0F};
    float swordSwingHalfHeight{0.0F};
    int projectileCount{0};
    std::array<ProjectileHudEntry, kMaxProjectiles> projectiles{};
    int enemyProjectileCount{0};
    std::array<EnemyProjectileHudEntry, kMaxEnemyProjectiles> enemyProjectiles{};
    int flyingEnemyCount{0};
    std::array<FlyingEnemyHudEntry, kMaxFlyingEnemies> flyingEnemies{};
    int wormCount{0};
    std::array<WormHudEntry, kMaxWorms> worms{};
    DragonHudEntry dragon{};
    std::vector<DamageNumberHud> damageNumbers{};
    int mouseX{0};
    int mouseY{0};
    float perfFrameMs{0.0F};
    float perfUpdateMs{0.0F};
    float perfRenderMs{0.0F};
    float perfFps{0.0F};
    bool consoleOpen{false};
    std::string consoleInput{};
    std::string consoleStatus{};
    std::size_t consoleCursor{0};
    std::vector<ChatLineHud> chatLines{};
    bool menuOpen{false};
    std::string menuTitle{};
    std::vector<std::string> menuEntries{};
    int menuSelected{0};
    std::string menuHint{};
    std::string menuDetailTitle{};
    std::vector<std::string> menuDetailLines{};
    bool menuEditActive{false};
    std::string menuEditLabel{};
    std::string menuEditValue{};
    std::size_t menuEditCursor{0};
    bool menuHideGameUi{false};
    bool menuHideWorld{false};
    float minimapZoom{1.0F};
    float fullscreenMapZoom{1.0F};
    bool minimapFullscreen{false};
    float minimapCenterX{0.0F};
    float minimapCenterY{0.0F};
    std::string minimapWorldId{};
};

} // namespace terraria::rendering
