#include "terraria/game/CombatSystem.h"

#include "terraria/game/EnemyManager.h"

#include <algorithm>
#include <cmath>

namespace terraria::game {

namespace {
constexpr float kSwordSwingDuration = 0.22F;
constexpr float kSwordSwingRadius = 1.3F;
constexpr float kSwordSwingHalfWidth = 0.6F;
constexpr float kSwordSwingHalfHeight = 0.5F;
constexpr float kProjectileSpeed = 22.0F;
constexpr float kProjectileLifetime = 2.5F;
constexpr float kProjectileGravity = 28.0F;
}

CombatSystem::CombatSystem(world::World& world,
                           entities::Player& player,
                           PhysicsSystem& physics,
                           DamageNumberSystem& damageNumbers,
                           std::vector<entities::Zombie>& zombies,
                           std::vector<entities::FlyingEnemy>& flyers,
                           std::vector<entities::Worm>& worms,
                           entities::Dragon* dragon,
                           EnemyManager& enemyManager)
    : world_{world},
      player_{player},
      physics_{physics},
      damageNumbers_{damageNumbers},
      zombies_{zombies},
      flyers_{flyers},
      worms_{worms},
      dragon_{dragon},
      enemyManager_{enemyManager} {}

int CombatSystem::swordDamageForTier(entities::ToolTier tier) const {
    const int value = entities::ToolTierValue(tier);
    if (value <= 0) {
        return 18;
    }
    return 18 + value * 6;
}

int CombatSystem::rangedDamageForTier(entities::ToolTier tier) const {
    const int value = entities::ToolTierValue(tier);
    if (value <= 0) {
        return 14;
    }
    return 14 + value * 5;
}

void CombatSystem::startSwordSwing(float angleStart, float angleEnd, entities::ToolTier swordTier) {
    if (swordSwing_.active) {
        return;
    }
    swordSwing_.active = true;
    swordSwing_.timer = 0.0F;
    swordSwing_.duration = kSwordSwingDuration;
    swordSwing_.radius = kSwordSwingRadius;
    swordSwing_.halfWidth = kSwordSwingHalfWidth;
    swordSwing_.halfHeight = kSwordSwingHalfHeight;
    swordSwing_.damage = swordDamageForTier(swordTier);
    swordSwing_.angleStart = angleStart;
    swordSwing_.angleEnd = angleEnd;
    swordSwingHitIds_.clear();
    swordSwingHitFlyerIds_.clear();
    swordSwingHitDragon_ = false;
    updateSwordSwing(0.0F);
}

void CombatSystem::spawnProjectile(const entities::Vec2& direction,
                                   entities::ToolTier tier,
                                   float damageScale,
                                   float speedScale,
                                   float gravityScale) {
    Projectile projectile{};
    projectile.position = {player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
    const float speed = kProjectileSpeed * std::clamp(speedScale, 0.2F, 2.0F);
    projectile.velocity = {direction.x * speed, direction.y * speed};
    projectile.lifetime = kProjectileLifetime;
    projectile.radius = 0.2F;
    const int baseDamage = rangedDamageForTier(tier);
    projectile.damage = std::max(1, static_cast<int>(std::round(static_cast<float>(baseDamage) * damageScale)));
    projectile.gravityScale = std::clamp(gravityScale, 0.1F, 2.0F);
    projectiles_.push_back(projectile);
}

void CombatSystem::reset() {
    swordSwing_ = {};
    swordSwingHitIds_.clear();
    swordSwingHitFlyerIds_.clear();
    swordSwingHitDragon_ = false;
    projectiles_.clear();
}

void CombatSystem::update(float dt) {
    updateSwordSwing(dt);
    updateProjectiles(dt);
}

void CombatSystem::updateSwordSwing(float dt) {
    if (!swordSwing_.active) {
        return;
    }
    swordSwing_.timer += dt;
    const float progress = std::clamp(swordSwing_.timer / swordSwing_.duration, 0.0F, 1.0F);
    const float angle = swordSwing_.angleStart + (swordSwing_.angleEnd - swordSwing_.angleStart) * progress;
    const entities::Vec2 pivot{player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
    swordSwing_.center = {pivot.x + std::cos(angle) * swordSwing_.radius, pivot.y + std::sin(angle) * swordSwing_.radius};
    for (auto& zombie : zombies_) {
        if (!zombie.alive()) {
            continue;
        }
        if (std::find(swordSwingHitIds_.begin(), swordSwingHitIds_.end(), zombie.id) != swordSwingHitIds_.end()) {
            continue;
        }
        if (physics_.aabbOverlap(swordSwing_.center,
                                 swordSwing_.halfWidth,
                                 swordSwing_.halfHeight * 2.0F,
                                 zombie.position,
                                 entities::kZombieHalfWidth,
                                 entities::kZombieHeight)) {
            const float knockDir = (zombie.position.x < swordSwing_.center.x) ? -1.0F : 1.0F;
            damageZombie(zombie, swordSwing_.damage, knockDir);
            swordSwingHitIds_.push_back(zombie.id);
        }
    }
    for (auto& flyer : flyers_) {
        if (!flyer.alive()) {
            continue;
        }
        if (std::find(swordSwingHitFlyerIds_.begin(), swordSwingHitFlyerIds_.end(), flyer.id) != swordSwingHitFlyerIds_.end()) {
            continue;
        }
        if (physics_.aabbOverlap(swordSwing_.center,
                                 swordSwing_.halfWidth,
                                 swordSwing_.halfHeight * 2.0F,
                                 flyer.position,
                                 entities::kFlyingEnemyRadius,
                                 entities::kFlyingEnemyRadius * 2.0F)) {
            const float knockDir = (flyer.position.x < swordSwing_.center.x) ? -1.0F : 1.0F;
            damageFlyer(flyer, swordSwing_.damage, knockDir);
            swordSwingHitFlyerIds_.push_back(flyer.id);
        }
    }
    for (auto& worm : worms_) {
        if (!worm.alive()) {
            continue;
        }
        if (physics_.aabbOverlap(swordSwing_.center,
                                 swordSwing_.halfWidth,
                                 swordSwing_.halfHeight * 2.0F,
                                 worm.position,
                                 entities::kWormRadius,
                                 entities::kWormRadius * 2.0F)) {
            const float knockDir = (worm.position.x < swordSwing_.center.x) ? -1.0F : 1.0F;
            damageWorm(worm, swordSwing_.damage, knockDir);
        }
    }
    if (dragon_ && dragon_->alive() && !swordSwingHitDragon_) {
        if (physics_.aabbOverlap(swordSwing_.center,
                                 swordSwing_.halfWidth,
                                 swordSwing_.halfHeight * 2.0F,
                                 dragon_->position,
                                 entities::kDragonHalfWidth,
                                 entities::kDragonHeight)) {
            const float knockDir = (dragon_->position.x < swordSwing_.center.x) ? -1.0F : 1.0F;
            damageDragon(*dragon_, swordSwing_.damage, knockDir);
            swordSwingHitDragon_ = true;
        }
    }
    enemyManager_.removeEnemyProjectilesInBox(swordSwing_.center, swordSwing_.halfWidth, swordSwing_.halfHeight * 2.0F);
    if (swordSwing_.timer >= swordSwing_.duration) {
        swordSwing_.active = false;
    }
}

void CombatSystem::updateProjectiles(float dt) {
    for (auto& projectile : projectiles_) {
        projectile.lifetime -= dt;
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        projectile.velocity.y += kProjectileGravity * projectile.gravityScale * dt;
        projectile.position.x += projectile.velocity.x * dt;
        projectile.position.y += projectile.velocity.y * dt;
        if (projectile.position.x < 0.0F || projectile.position.y < 0.0F
            || projectile.position.x >= static_cast<float>(world_.width())
            || projectile.position.y >= static_cast<float>(world_.height())
            || physics_.solidAtPosition(projectile.position)) {
            projectile.lifetime = 0.0F;
            continue;
        }
        if (enemyManager_.removeEnemyProjectileAt(projectile.position, projectile.radius)) {
            projectile.lifetime = 0.0F;
            continue;
        }
        for (auto& zombie : zombies_) {
            if (!zombie.alive()) {
                continue;
            }
            if (physics_.aabbOverlap(projectile.position,
                                     projectile.radius,
                                     projectile.radius * 2.0F,
                                     zombie.position,
                                     entities::kZombieHalfWidth,
                                     entities::kZombieHeight)) {
                const float knockDir = (projectile.velocity.x < 0.0F) ? -1.0F : 1.0F;
                damageZombie(zombie, projectile.damage, knockDir);
                projectile.lifetime = 0.0F;
                break;
            }
        }
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        for (auto& flyer : flyers_) {
            if (!flyer.alive()) {
                continue;
            }
            if (physics_.aabbOverlap(projectile.position,
                                     projectile.radius,
                                     projectile.radius * 2.0F,
                                     flyer.position,
                                     entities::kFlyingEnemyRadius,
                                     entities::kFlyingEnemyRadius * 2.0F)) {
                const float knockDir = (projectile.velocity.x < 0.0F) ? -1.0F : 1.0F;
                damageFlyer(flyer, projectile.damage, knockDir);
                projectile.lifetime = 0.0F;
                break;
            }
        }
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        for (auto& worm : worms_) {
            if (!worm.alive()) {
                continue;
            }
            if (physics_.aabbOverlap(projectile.position,
                                     projectile.radius,
                                     projectile.radius * 2.0F,
                                     worm.position,
                                     entities::kWormRadius,
                                     entities::kWormRadius * 2.0F)) {
                const float knockDir = (projectile.velocity.x < 0.0F) ? -1.0F : 1.0F;
                damageWorm(worm, projectile.damage, knockDir);
                projectile.lifetime = 0.0F;
                break;
            }
        }
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        if (dragon_ && dragon_->alive()) {
            if (physics_.aabbOverlap(projectile.position,
                                     projectile.radius,
                                     projectile.radius * 2.0F,
                                     dragon_->position,
                                     entities::kDragonHalfWidth,
                                     entities::kDragonHeight)) {
                const float knockDir = (projectile.velocity.x < 0.0F) ? -1.0F : 1.0F;
                damageDragon(*dragon_, projectile.damage, knockDir);
                projectile.lifetime = 0.0F;
            }
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(),
                                      projectiles_.end(),
                                      [](const Projectile& projectile) { return projectile.lifetime <= 0.0F; }),
                       projectiles_.end());
}

void CombatSystem::damageZombie(entities::Zombie& zombie, int amount, float knockbackDir) {
    zombie.health = std::max(0, zombie.health - amount);
    damageNumbers_.addDamage(zombie.position, amount, false);
    zombie.knockbackTimer = 0.18F;
    zombie.knockbackVelocity = knockbackDir * 8.0F;
    zombie.velocity.y = std::min(zombie.velocity.y, -6.0F);
}

void CombatSystem::damageFlyer(entities::FlyingEnemy& flyer, int amount, float knockbackDir) {
    flyer.takeDamage(amount);
    flyer.knockbackTimer = 0.18F;
    flyer.knockbackVelocity = knockbackDir * 6.5F;
    damageNumbers_.addDamage(flyer.position, amount, false);
}

void CombatSystem::damageWorm(entities::Worm& worm, int amount, float knockbackDir) {
    worm.takeDamage(amount);
    worm.knockbackTimer = 0.2F;
    worm.knockbackVelocity = knockbackDir * 7.5F;
    damageNumbers_.addDamage(worm.position, amount, false);
}

void CombatSystem::damageDragon(entities::Dragon& dragon, int amount, float knockbackDir) {
    dragon.takeDamage(amount);
    dragon.knockbackTimer = 0.2F;
    dragon.knockbackVelocity = knockbackDir * 5.5F;
    damageNumbers_.addDamage(dragon.position, amount, false);
}

void CombatSystem::fillHud(rendering::HudState& hud) const {
    hud.swordSwingActive = swordSwing_.active;
    if (swordSwing_.active) {
        hud.swordSwingX = swordSwing_.center.x;
        hud.swordSwingY = swordSwing_.center.y;
        hud.swordSwingHalfWidth = swordSwing_.halfWidth;
        hud.swordSwingHalfHeight = swordSwing_.halfHeight;
    } else {
        hud.swordSwingX = 0.0F;
        hud.swordSwingY = 0.0F;
        hud.swordSwingHalfWidth = 0.0F;
        hud.swordSwingHalfHeight = 0.0F;
    }

    const int projectileCount =
        std::min(static_cast<int>(projectiles_.size()), rendering::kMaxProjectiles);
    hud.projectileCount = projectileCount;
    for (int i = 0; i < projectileCount; ++i) {
        const auto& projectile = projectiles_[static_cast<std::size_t>(i)];
        auto& entry = hud.projectiles[static_cast<std::size_t>(i)];
        entry.active = true;
        entry.x = projectile.position.x;
        entry.y = projectile.position.y;
        entry.radius = projectile.radius;
    }
    for (int i = projectileCount; i < rendering::kMaxProjectiles; ++i) {
        hud.projectiles[static_cast<std::size_t>(i)] = {};
    }
}

} // namespace terraria::game
