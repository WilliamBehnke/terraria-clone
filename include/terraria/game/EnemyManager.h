#pragma once

#include "terraria/core/Application.h"
#include "terraria/entities/FlyingEnemy.h"
#include "terraria/entities/Player.h"
#include "terraria/entities/Worm.h"
#include "terraria/entities/Zombie.h"
#include "terraria/game/DamageNumberSystem.h"
#include "terraria/game/PhysicsSystem.h"
#include "terraria/rendering/HudState.h"
#include "terraria/world/World.h"

#include <random>
#include <vector>

namespace terraria::game {

class EnemyManager {
public:
    EnemyManager(const core::AppConfig& config,
                 world::World& world,
                 entities::Player& player,
                 PhysicsSystem& physics,
                 DamageNumberSystem& damageNumbers);

    void update(float dt, bool isNight, const entities::Vec2& cameraFocus);
    void fillHud(rendering::HudState& hud) const;
    bool removeEnemyProjectilesInBox(const entities::Vec2& center, float halfWidth, float halfHeight);
    bool removeEnemyProjectileAt(const entities::Vec2& position, float radius);

    std::vector<entities::Zombie>& zombies() { return zombies_; }
    const std::vector<entities::Zombie>& zombies() const { return zombies_; }
    std::vector<entities::FlyingEnemy>& flyers() { return flyers_; }
    const std::vector<entities::FlyingEnemy>& flyers() const { return flyers_; }
    std::vector<entities::Worm>& worms() { return worms_; }
    const std::vector<entities::Worm>& worms() const { return worms_; }

private:
    struct ViewBounds {
        int startX{0};
        int startY{0};
        int tilesWide{0};
        int tilesTall{0};
    };

    struct EnemyProjectile {
        entities::Vec2 position{};
        entities::Vec2 velocity{};
        float lifetime{0.0F};
        float radius{0.2F};
        int damage{0};
    };

    ViewBounds computeViewBounds(const entities::Vec2& cameraFocus) const;

    void updateZombies(float dt, bool isNight, const ViewBounds& view);
    void updateFlyers(float dt, bool isNight, const ViewBounds& view);
    void updateWorms(float dt, const ViewBounds& view);
    void updateEnemyProjectiles(float dt);

    void spawnZombie(const ViewBounds& view);
    void spawnFlyer(const ViewBounds& view);
    void spawnWorm(const ViewBounds& view);

    int computeZombiePathDirection(const entities::Zombie& zombie) const;
    bool isWalkableSpot(int x, int footY) const;
    bool pathClear(int startX, int endX, int footY) const;
    void applyZombiePhysics(entities::Zombie& zombie, float dt);

    const core::AppConfig& config_;
    world::World& world_;
    entities::Player& player_;
    PhysicsSystem& physics_;
    DamageNumberSystem& damageNumbers_;
    std::vector<entities::Zombie> zombies_{};
    std::vector<entities::FlyingEnemy> flyers_{};
    std::vector<entities::Worm> worms_{};
    std::vector<EnemyProjectile> enemyProjectiles_{};
    std::mt19937 rng_{};
    float spawnTimerZombies_{0.0F};
    float spawnTimerFlyers_{0.0F};
    float spawnTimerWorms_{0.0F};
    int nextZombieId_{1};
    int nextFlyerId_{1};
    int nextWormId_{1};
    float swoopTimer_{0.0F};
};

} // namespace terraria::game
