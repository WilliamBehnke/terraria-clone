#pragma once

#include "terraria/entities/Dragon.h"
#include "terraria/entities/FlyingEnemy.h"
#include "terraria/entities/Worm.h"
#include "terraria/entities/Player.h"
#include "terraria/entities/Zombie.h"
#include "terraria/game/DamageNumberSystem.h"
#include "terraria/game/PhysicsSystem.h"
#include "terraria/rendering/HudState.h"
#include "terraria/world/World.h"

#include <vector>

namespace terraria::game {

class EnemyManager;

class CombatSystem {
public:
    CombatSystem(world::World& world,
                 entities::Player& player,
                 PhysicsSystem& physics,
                 DamageNumberSystem& damageNumbers,
                 std::vector<entities::Zombie>& zombies,
                 std::vector<entities::FlyingEnemy>& flyers,
                 std::vector<entities::Worm>& worms,
                 entities::Dragon* dragon,
                 EnemyManager& enemyManager);

    void update(float dt);
    void startSwordSwing(float angleStart, float angleEnd, entities::ToolTier swordTier);
    void spawnProjectile(const entities::Vec2& direction,
                         entities::ToolTier tier,
                         float damageScale = 1.0F,
                         float speedScale = 1.0F,
                         float gravityScale = 1.0F);
    void reset();
    void fillHud(rendering::HudState& hud) const;

private:
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
        int damage{0};
    };

    struct Projectile {
        entities::Vec2 position{};
        entities::Vec2 velocity{};
        float lifetime{0.0F};
        float radius{0.2F};
        int damage{0};
        float gravityScale{1.0F};
    };

    int swordDamageForTier(entities::ToolTier tier) const;
    int rangedDamageForTier(entities::ToolTier tier) const;
    void updateSwordSwing(float dt);
    void updateProjectiles(float dt);
    void damageZombie(entities::Zombie& zombie, int amount, float knockbackDir);
    void damageFlyer(entities::FlyingEnemy& flyer, int amount, float knockbackDir);
    void damageWorm(entities::Worm& worm, int amount, float knockbackDir);
    void damageDragon(entities::Dragon& dragon, int amount, float knockbackDir);

    world::World& world_;
    entities::Player& player_;
    PhysicsSystem& physics_;
    DamageNumberSystem& damageNumbers_;
    std::vector<entities::Zombie>& zombies_;
    std::vector<entities::FlyingEnemy>& flyers_;
    std::vector<entities::Worm>& worms_;
    entities::Dragon* dragon_{nullptr};
    EnemyManager& enemyManager_;
    SwordSwingState swordSwing_{};
    std::vector<int> swordSwingHitIds_{};
    std::vector<int> swordSwingHitFlyerIds_{};
    bool swordSwingHitDragon_{false};
    std::vector<Projectile> projectiles_{};
};

} // namespace terraria::game
