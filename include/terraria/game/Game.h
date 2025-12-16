#pragma once

#include "terraria/core/Application.h"
#include "terraria/entities/Player.h"
#include "terraria/entities/Zombie.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/Renderer.h"
#include "terraria/world/World.h"
#include "terraria/world/WorldGenerator.h"

#include <array>
#include <memory>
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
    bool cursorHitsZombie(const entities::Zombie& zombie, int tileX, int tileY) const;
    bool aabbOverlap(const entities::Vec2& aPos, float aHalfWidth, float aHeight,
                     const entities::Vec2& bPos, float bHalfWidth, float bHeight) const;
    void updateDayNight(float dt);
    float normalizedTimeOfDay() const;

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
    std::vector<std::unique_ptr<entities::Zombie>> zombies_{};
    std::mt19937 zombieRng_;
    float zombieSpawnTimer_{0.0F};
    entities::Vec2 spawnPosition_{};
    float playerAttackCooldown_{0.0F};
    float timeOfDay_{0.0F};
    float dayLength_{180.0F};
    bool isNight_{false};
};

} // namespace terraria::game
