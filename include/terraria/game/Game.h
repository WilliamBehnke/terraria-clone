#pragma once

#include "terraria/core/Application.h"
#include "terraria/entities/Player.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/Renderer.h"
#include "terraria/world/World.h"
#include "terraria/world/WorldGenerator.h"

#include <memory>
#include <array>

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
    bool collides(const entities::Vec2& pos) const;
    bool isSolidTile(int x, int y) const;
    void resolveHorizontal(entities::Vec2& position, entities::Vec2& velocity);
    void resolveVertical(entities::Vec2& position, entities::Vec2& velocity);
    void processActions(float dt);
    bool cursorWorldTile(int& outX, int& outY) const;
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
    std::unique_ptr<rendering::IRenderer> renderer_;
    std::unique_ptr<input::IInputSystem> inputSystem_;
    bool jumpRequested_{false};
    BreakState breakState_{};
    float placeCooldown_{0.0F};
    int selectedHotbar_{0};
    const std::array<world::TileType, rendering::kMaxHotbarSlots> hotbarTypes_{
        world::TileType::Dirt,
        world::TileType::Stone,
        world::TileType::Grass,
        world::TileType::Wood,
        world::TileType::Leaves,
        world::TileType::CopperOre,
        world::TileType::IronOre,
        world::TileType::GoldOre};
    rendering::HudState hudState_{};
    bool cameraMode_{false};
    entities::Vec2 cameraPosition_{};
    float cameraSpeed_{80.0F};
};

} // namespace terraria::game
