#pragma once

#include "terraria/core/Application.h"
#include "terraria/entities/Player.h"
#include "terraria/entities/Tools.h"
#include "terraria/game/CombatSystem.h"
#include "terraria/game/CraftingSystem.h"
#include "terraria/game/ChatConsole.h"
#include "terraria/game/DamageNumberSystem.h"
#include "terraria/game/EnemyManager.h"
#include "terraria/game/InventorySystem.h"
#include "terraria/game/MenuSystem.h"
#include "terraria/game/PhysicsSystem.h"
#include "terraria/game/SaveManager.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/Renderer.h"
#include "terraria/world/World.h"
#include "terraria/world/WorldGenerator.h"

#include <chrono>
#include <cstdint>
#include <vector>
#include <string>

namespace terraria::game {

class Game {
public:
    explicit Game(const core::AppConfig& config);

    void initialize();
    bool tick();
    void shutdown();

private:
    void handleInput();
    void update(float dt);
    void render();
    void processActions(float dt);
    void saveActiveSession();
    void clearActiveSession();
    void loadOrCreateSaves();
    void startSession(const WorldInfo& worldInfo, const CharacterInfo& characterInfo);
    entities::Vec2 findSpawnPosition() const;
    bool findNearestOpenSpot(const entities::Vec2& desired, entities::Vec2& outPos) const;
    void executeConsoleCommand(const std::string& text);
    bool cursorWorldTile(int& outX, int& outY) const;
    bool cursorWorldPosition(entities::Vec2& outPos) const;
    bool canBreakTile(int tileX, int tileY) const;
    bool canPlaceTile(int tileX, int tileY) const;
    bool withinPlacementRange(int tileX, int tileY) const;
    bool tileInsidePlayer(int tileX, int tileY) const;
    void handleBreaking(float dt);
    void handlePlacement(float dt);
    void updateHudState();
    void toggleCameraMode();
    entities::Vec2 cameraFocus() const;
    entities::Vec2 clampCameraTarget(const entities::Vec2& desired) const;
    static bool isTreeTile(world::TileType type);
    void updateDayNight(float dt);
    float normalizedTimeOfDay() const;
    void recordPerformanceMetrics(float frameMs, float updateMs, float renderMs);
    entities::ToolTier selectedToolTier(entities::ToolKind kind) const;
    bool hasRequiredTool(world::TileType tileType) const;
    bool canMineTileWithTool(world::TileType tileType, entities::ToolKind kind, entities::ToolTier tier) const;
    float breakSpeedMultiplier(world::TileType tileType) const;

    struct BreakState {
        bool active{false};
        int tileX{0};
        int tileY{0};
        float progress{0.0F};
    };

    core::AppConfig config_;
    world::World world_;
    world::WorldGenerator generator_;
    entities::Player player_;
    PhysicsSystem physics_;
    DamageNumberSystem damageNumbers_{};
    EnemyManager enemyManager_;
    CombatSystem combatSystem_;
    std::unique_ptr<rendering::IRenderer> renderer_;
    std::unique_ptr<input::IInputSystem> inputSystem_;
    InventorySystem inventorySystem_;
    CraftingSystem craftingSystem_;
    BreakState breakState_{};
    float placeCooldown_{0.0F};
    int selectedHotbar_{0};
    rendering::HudState hudState_{};
    bool cameraMode_{false};
    entities::Vec2 cameraPosition_{};
    float cameraSpeed_{80.0F};
    entities::Vec2 spawnPosition_{};
    float playerAttackCooldown_{0.0F};
    float timeOfDay_{0.0F};
    float dayLength_{180.0F};
    bool isNight_{false};
    std::uint32_t worldSeed_{0};
    entities::Vec2 worldSpawn_{};
    float moveInput_{0.0F};
    float jumpBufferTimer_{0.0F};
    float coyoteTimer_{0.0F};
    bool jumpHeld_{false};
    bool prevJumpInput_{false};
    bool prevBreakHeld_{false};
    float perfFrameTimeMs_{0.0F};
    float perfUpdateTimeMs_{0.0F};
    float perfRenderTimeMs_{0.0F};
    float perfFps_{0.0F};
    float bowDrawTimer_{0.0F};
    bool paused_{false};
    bool requestQuit_{false};
    float minimapZoom_{1.0F};
    bool minimapFullscreen_{false};
    float minimapCenterX_{0.0F};
    float minimapCenterY_{0.0F};
    ChatConsole chatConsole_{};
    SaveManager saveManager_{};
    MenuSystem menuSystem_{};
    std::vector<CharacterInfo> characterList_{};
    std::vector<WorldInfo> worldList_{};
    std::string activeCharacterId_{};
    std::string activeCharacterName_{};
    std::string activeWorldId_{};
    std::string activeWorldName_{};
};

} // namespace terraria::game
