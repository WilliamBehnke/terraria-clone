#include "terraria/game/EnemyManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace terraria::game {

namespace {
constexpr float kGravity = 60.0F;
constexpr float kZombieMoveSpeed = 6.0F;
constexpr float kZombieSpawnIntervalMin = 3.0F;
constexpr float kZombieSpawnIntervalMax = 6.0F;
constexpr int kMaxZombies = 12;
constexpr int kZombieDamage = 14;
constexpr float kZombieAttackInterval = 1.2F;
constexpr float kZombieJumpVelocity = 22.0F;
constexpr float kTilePixels = 16.0F;

constexpr float kFlyerMoveSpeed = 7.5F;
constexpr float kFlyerSpawnIntervalMin = 4.0F;
constexpr float kFlyerSpawnIntervalMax = 7.0F;
constexpr int kMaxFlyers = 8;
constexpr int kFlyerDamage = 10;
constexpr float kFlyerAttackInterval = 1.6F;
constexpr float kFlyerProjectileSpeed = 12.0F;
constexpr float kFlyerProjectileLifetime = 3.2F;
constexpr int kZombieCoinMin = 1;
constexpr int kZombieCoinMax = 3;
constexpr int kFlyerCoinMin = 2;
constexpr int kFlyerCoinMax = 5;
constexpr float kWormMoveSpeed = 10.0F;
constexpr float kWormLungeSpeed = 24.0F;
constexpr float kWormAirGravity = 24.0F;
constexpr float kWormAirGravityUp = 12.0F;
constexpr float kWormAirTime = 1.2F;
constexpr float kWormLungeCooldown = 1.6F;
constexpr float kWormSpawnIntervalMin = 5.0F;
constexpr float kWormSpawnIntervalMax = 9.0F;
constexpr int kMaxWorms = 6;
constexpr int kWormDamage = 18;
constexpr float kWormAttackInterval = 1.0F;
constexpr int kWormCoinMin = 3;
constexpr int kWormCoinMax = 7;
constexpr float kWormSpawnDepthRatio = 0.6F;
constexpr float kOffscreenDespawnTime = 6.0F;
constexpr float kWormTurnRate = 3.0F;
constexpr float kWormLungeRange = 8.0F;
constexpr float kWormLungeVerticalBias = 0.85F;
constexpr float kWormRecenterSpeed = 0.45F;
constexpr float kWormSeparationRadius = 3.5F;
constexpr float kWormSeparationStrength = 2.2F;
constexpr float kDragonMoveSpeed = 5.4F;
constexpr float kDragonAttackInterval = 1.1F;
constexpr float kDragonFireInterval = 2.0F;
constexpr float kDragonBreathDuration = 0.9F;
constexpr float kDragonBreathTick = 0.12F;
constexpr float kDragonBreathSpread = 0.25F;
constexpr float kDragonBreathSpeed = 9.5F;
constexpr float kDragonBreathLifetime = 0.9F;
constexpr float kDragonBreathRadius = 0.35F;
constexpr int kDragonSegments = 7;
constexpr float kDragonSegmentGapFactor = 0.455F;
constexpr float kDragonSegmentYFactor = 0.65F;
constexpr float kDragonHeadYOffsetFactor = 0.04F;
constexpr float kDragonChargeSpeed = 16.5F;
constexpr float kDragonChargeDuration = 0.55F;
constexpr float kDragonChargeCooldown = 4.2F;
constexpr float kDragonVolleyInterval = 3.4F;
constexpr float kDragonVolleySpread = 0.35F;
constexpr float kDragonVolleySpeed = 11.5F;
constexpr float kDragonVolleyLifetime = 2.8F;
constexpr float kDragonVolleyRadius = 0.42F;
constexpr int kDragonVolleyDamage = 20;
constexpr int kDragonContactDamage = 26;
constexpr int kDragonProjectileDamage = 22;
constexpr int kDragonCoinMin = 12;
constexpr int kDragonCoinMax = 22;
}

EnemyManager::EnemyManager(const core::AppConfig& config,
                           world::World& world,
                           entities::Player& player,
                           PhysicsSystem& physics,
                           DamageNumberSystem& damageNumbers)
    : config_{config},
      world_{world},
      player_{player},
      physics_{physics},
      damageNumbers_{damageNumbers},
      rng_{static_cast<std::uint32_t>(config.worldWidth * 313 + config.worldHeight * 197)} {
    dragon_.health = 0;
    std::uniform_real_distribution<float> zombieTimerDist(kZombieSpawnIntervalMin, kZombieSpawnIntervalMax);
    std::uniform_real_distribution<float> flyerTimerDist(kFlyerSpawnIntervalMin, kFlyerSpawnIntervalMax);
    std::uniform_real_distribution<float> wormTimerDist(kWormSpawnIntervalMin, kWormSpawnIntervalMax);
    spawnTimerZombies_ = zombieTimerDist(rng_);
    spawnTimerFlyers_ = flyerTimerDist(rng_);
    spawnTimerWorms_ = wormTimerDist(rng_);
}

void EnemyManager::reset() {
    zombies_.clear();
    flyers_.clear();
    worms_.clear();
    dragon_ = {};
    dragon_.health = 0;
    enemyProjectiles_.clear();
    spawnTimerZombies_ = 0.0F;
    spawnTimerFlyers_ = 0.0F;
    spawnTimerWorms_ = 0.0F;
    nextZombieId_ = 1;
    nextFlyerId_ = 1;
    nextWormId_ = 1;
    swoopTimer_ = 0.0F;
    dragonActive_ = false;
    dragonSpawned_ = false;
    dragonDefeated_ = false;
    dragonDenCenter_ = {};
    dragonDenRadiusX_ = 0.0F;
    dragonDenRadiusY_ = 0.0F;
}

EnemyManager::ViewBounds EnemyManager::computeViewBounds(const entities::Vec2& cameraFocus) const {
    const int tilesWide = config_.windowWidth / static_cast<int>(kTilePixels) + 2;
    const int tilesTall = config_.windowHeight / static_cast<int>(kTilePixels) + 2;
    const int maxStartX = std::max(world_.width() - tilesWide, 0);
    const int maxStartY = std::max(world_.height() - tilesTall, 0);
    float viewX = cameraFocus.x - static_cast<float>(tilesWide) / 2.0F;
    float viewY = cameraFocus.y - static_cast<float>(tilesTall) / 2.0F;
    viewX = std::clamp(viewX, 0.0F, static_cast<float>(maxStartX));
    viewY = std::clamp(viewY, 0.0F, static_cast<float>(maxStartY));
    return ViewBounds{static_cast<int>(std::floor(viewX)),
                      static_cast<int>(std::floor(viewY)),
                      tilesWide,
                      tilesTall};
}

void EnemyManager::update(float dt, bool isNight, const entities::Vec2& cameraFocus) {
    const auto view = computeViewBounds(cameraFocus);
    swoopTimer_ += dt;

    updateZombies(dt, isNight, view);
    updateFlyers(dt, isNight, view);
    updateWorms(dt, view);
    updateDragon(dt);
    updateEnemyProjectiles(dt);
}

void EnemyManager::setDragonDen(const entities::Vec2& center, float radiusX, float radiusY) {
    if (radiusX <= 1.0F || radiusY <= 1.0F) {
        dragonActive_ = false;
        dragonDenCenter_ = {};
        dragonDenRadiusX_ = 0.0F;
        dragonDenRadiusY_ = 0.0F;
        dragonSpawned_ = false;
        dragonDefeated_ = false;
        dragon_.health = 0;
        return;
    }
    dragonActive_ = true;
    dragonDenCenter_ = center;
    dragonDenRadiusX_ = radiusX;
    dragonDenRadiusY_ = radiusY;
    dragonSpawned_ = false;
    dragonDefeated_ = false;
    dragon_ = {};
    dragon_.health = 0;
}

bool EnemyManager::removeEnemyProjectilesInBox(const entities::Vec2& center, float halfWidth, float halfHeight) {
    bool removed = false;
    for (auto& projectile : enemyProjectiles_) {
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        if (physics_.aabbOverlap(center,
                                 halfWidth,
                                 halfHeight,
                                 projectile.position,
                                 projectile.radius,
                                 projectile.radius * 2.0F)) {
            projectile.lifetime = 0.0F;
            removed = true;
        }
    }
    return removed;
}

bool EnemyManager::removeEnemyProjectileAt(const entities::Vec2& position, float radius) {
    bool removed = false;
    for (auto& projectile : enemyProjectiles_) {
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        if (projectile.fromDragon) {
            continue;
        }
        if (physics_.aabbOverlap(position,
                                 radius,
                                 radius * 2.0F,
                                 projectile.position,
                                 projectile.radius,
                                 projectile.radius * 2.0F)) {
            projectile.lifetime = 0.0F;
            removed = true;
        }
    }
    return removed;
}

void EnemyManager::updateZombies(float dt, bool isNight, const ViewBounds& view) {
    if (spawnTimerZombies_ > 0.0F) {
        spawnTimerZombies_ -= dt;
    }
    if (isNight && spawnTimerZombies_ <= 0.0F && static_cast<int>(zombies_.size()) < kMaxZombies) {
        spawnZombie(view);
        std::uniform_real_distribution<float> timerDist(kZombieSpawnIntervalMin, kZombieSpawnIntervalMax);
        spawnTimerZombies_ = timerDist(rng_);
    } else if (!isNight) {
        spawnTimerZombies_ = 0.0F;
    }

    for (auto& zombie : zombies_) {
        if (!zombie.alive()) {
            if (!zombie.droppedLoot) {
                const int drop = std::uniform_int_distribution<int>(kZombieCoinMin, kZombieCoinMax)(rng_);
                player_.addToInventory(world::TileType::Coin, drop);
                damageNumbers_.addLoot(zombie.position, drop);
                zombie.droppedLoot = true;
            }
            continue;
        }
        const bool onScreen = zombie.position.x >= static_cast<float>(view.startX)
            && zombie.position.x <= static_cast<float>(view.startX + view.tilesWide)
            && zombie.position.y >= static_cast<float>(view.startY)
            && zombie.position.y <= static_cast<float>(view.startY + view.tilesTall);
        if (!onScreen) {
            zombie.offscreenTimer += dt;
            if (zombie.offscreenTimer >= kOffscreenDespawnTime) {
                zombie.health = 0;
                continue;
            }
        } else {
            zombie.offscreenTimer = 0.0F;
        }
        if (!isNight) {
            const int margin = 8;
            const float viewCenterX = static_cast<float>(view.startX) + static_cast<float>(view.tilesWide) * 0.5F;
            zombie.desiredDir = (zombie.position.x < viewCenterX) ? -1 : 1;
            if (zombie.position.x < static_cast<float>(view.startX - margin)
                || zombie.position.x > static_cast<float>(view.startX + view.tilesWide + margin)) {
                zombie.health = 0;
                continue;
            }
        }
        zombie.attackCooldown = std::max(0.0F, zombie.attackCooldown - dt);
        zombie.lungeCooldown = std::max(0.0F, zombie.lungeCooldown - dt);
        zombie.lungeTimer = std::max(0.0F, zombie.lungeTimer - dt);
        if (isNight) {
            zombie.pathTimer -= dt;
            if (zombie.pathTimer <= 0.0F) {
                zombie.desiredDir = computeZombiePathDirection(zombie);
                zombie.pathTimer = 0.4F;
            }
        }
        applyZombiePhysics(zombie, dt);
        if (isNight && physics_.aabbOverlap(zombie.position,
                                            entities::kZombieHalfWidth,
                                            entities::kZombieHeight,
                                            player_.position(),
                                            entities::kPlayerHalfWidth,
                                            entities::kPlayerHeight)
            && zombie.attackCooldown <= 0.0F) {
            const int dealt = player_.applyDamage(kZombieDamage);
            damageNumbers_.addDamage(player_.position(), dealt, true);
            zombie.attackCooldown = kZombieAttackInterval * 0.85F;
            const float dir = (player_.position().x < zombie.position.x) ? -1.0F : 1.0F;
            entities::Vec2 knock = player_.velocity();
            knock.x += dir * 7.0F;
            knock.y = -8.0F;
            player_.setVelocity(knock);
        }
    }

    zombies_.erase(
        std::remove_if(zombies_.begin(), zombies_.end(), [](const entities::Zombie& zombie) { return !zombie.alive(); }),
        zombies_.end());
}

void EnemyManager::updateFlyers(float dt, bool isNight, const ViewBounds& view) {
    if (spawnTimerFlyers_ > 0.0F) {
        spawnTimerFlyers_ -= dt;
    }
    if (isNight && spawnTimerFlyers_ <= 0.0F && static_cast<int>(flyers_.size()) < kMaxFlyers) {
        spawnFlyer(view);
        std::uniform_real_distribution<float> timerDist(kFlyerSpawnIntervalMin, kFlyerSpawnIntervalMax);
        spawnTimerFlyers_ = timerDist(rng_);
    } else if (!isNight) {
        spawnTimerFlyers_ = 0.0F;
    }

    for (auto& flyer : flyers_) {
        if (!flyer.alive()) {
            if (!flyer.droppedLoot) {
                const int drop = std::uniform_int_distribution<int>(kFlyerCoinMin, kFlyerCoinMax)(rng_);
                player_.addToInventory(world::TileType::Coin, drop);
                damageNumbers_.addLoot(flyer.position, drop);
                flyer.droppedLoot = true;
            }
            continue;
        }
        const bool onScreen = flyer.position.x >= static_cast<float>(view.startX)
            && flyer.position.x <= static_cast<float>(view.startX + view.tilesWide)
            && flyer.position.y >= static_cast<float>(view.startY)
            && flyer.position.y <= static_cast<float>(view.startY + view.tilesTall);
        if (!onScreen) {
            flyer.offscreenTimer += dt;
            if (flyer.offscreenTimer >= kOffscreenDespawnTime) {
                flyer.health = 0;
                continue;
            }
        } else {
            flyer.offscreenTimer = 0.0F;
        }
        flyer.attackCooldown = std::max(0.0F, flyer.attackCooldown - dt);
        flyer.contactCooldown = std::max(0.0F, flyer.contactCooldown - dt);

        if (!isNight) {
            const int margin = 10;
            const float viewCenterX = static_cast<float>(view.startX) + static_cast<float>(view.tilesWide) * 0.5F;
            const float fleeDir = (flyer.position.x < viewCenterX) ? -1.0F : 1.0F;
            flyer.velocity.x = fleeDir * kFlyerMoveSpeed;
            flyer.velocity.y = -kFlyerMoveSpeed * 0.4F;
            if (flyer.position.x < static_cast<float>(view.startX - margin)
                || flyer.position.x > static_cast<float>(view.startX + view.tilesWide + margin)
                || flyer.position.y < static_cast<float>(view.startY - margin)) {
                flyer.health = 0;
            }
            flyer.position.x += flyer.velocity.x * dt;
            flyer.position.y += flyer.velocity.y * dt;
            continue;
        }

        if (flyer.knockbackTimer > 0.0F) {
            flyer.velocity.x = flyer.knockbackVelocity;
            flyer.velocity.y = std::min(flyer.velocity.y, -2.0F);
            flyer.knockbackVelocity *= 0.88F;
            flyer.knockbackTimer = std::max(0.0F, flyer.knockbackTimer - dt);
        } else {
            const float phase = swoopTimer_ * 1.35F + static_cast<float>(flyer.id) * 0.7F;
            const float sweepX = std::cos(phase) * 4.0F;
            const float sweepY = std::sin(phase * 1.4F) * 3.0F;
            const entities::Vec2 target{player_.position().x + sweepX,
                                        player_.position().y - 4.5F + sweepY};
            entities::Vec2 delta{target.x - flyer.position.x, target.y - flyer.position.y};
            const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (distance > 0.01F) {
                delta.x /= distance;
                delta.y /= distance;
                flyer.velocity.x = delta.x * kFlyerMoveSpeed;
                flyer.velocity.y = delta.y * kFlyerMoveSpeed;
            } else {
                flyer.velocity = {0.0F, 0.0F};
            }
        }

        flyer.position.x += flyer.velocity.x * dt;
        flyer.position.y += flyer.velocity.y * dt;
        flyer.position.x = std::clamp(flyer.position.x, 0.5F, static_cast<float>(world_.width() - 1));
        flyer.position.y = std::clamp(flyer.position.y, 1.0F, static_cast<float>(world_.height() - 2));

        if (physics_.solidAtPosition(flyer.position)) {
            flyer.position.y = std::max(1.0F, flyer.position.y - 0.5F);
        }

        if (physics_.aabbOverlap(flyer.position,
                                 entities::kFlyingEnemyRadius,
                                 entities::kFlyingEnemyRadius * 2.0F,
                                 player_.position(),
                                 entities::kPlayerHalfWidth,
                                 entities::kPlayerHeight)
            && flyer.contactCooldown <= 0.0F) {
            const int dealt = player_.applyDamage(kFlyerDamage);
            damageNumbers_.addDamage(player_.position(), dealt, true);
            const float dir = (player_.position().x < flyer.position.x) ? -1.0F : 1.0F;
            entities::Vec2 knock = player_.velocity();
            knock.x += dir * 7.5F;
            knock.y = -8.5F;
            player_.setVelocity(knock);
            flyer.knockbackTimer = 0.16F;
            flyer.knockbackVelocity = -dir * 6.0F;
            flyer.contactCooldown = 0.9F;
        }

        entities::Vec2 toPlayer{player_.position().x - flyer.position.x,
                                player_.position().y - flyer.position.y};
        const float fireDistance = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
        if (fireDistance < 10.0F && flyer.attackCooldown <= 0.0F) {
            entities::Vec2 dir = toPlayer;
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.01F) {
                dir.x /= len;
                dir.y /= len;
            } else {
                dir = {1.0F, 0.0F};
            }
            EnemyProjectile projectile{};
            projectile.position = flyer.position;
            projectile.velocity = {dir.x * kFlyerProjectileSpeed, dir.y * kFlyerProjectileSpeed};
            projectile.lifetime = kFlyerProjectileLifetime;
            projectile.radius = 0.18F;
            projectile.damage = kFlyerDamage;
            projectile.isFlame = false;
            projectile.fromDragon = false;
            enemyProjectiles_.push_back(projectile);
            flyer.attackCooldown = kFlyerAttackInterval;
        }
    }

    flyers_.erase(
        std::remove_if(flyers_.begin(), flyers_.end(), [](const entities::FlyingEnemy& enemy) { return !enemy.alive(); }),
        flyers_.end());
}

void EnemyManager::updateWorms(float dt, const ViewBounds& view) {
    const bool underground = player_.position().y > static_cast<float>(world_.height()) * kWormSpawnDepthRatio;
    if (spawnTimerWorms_ > 0.0F) {
        spawnTimerWorms_ -= dt;
    }
    if (underground && spawnTimerWorms_ <= 0.0F && static_cast<int>(worms_.size()) < kMaxWorms) {
        spawnWorm(view);
        std::uniform_real_distribution<float> timerDist(kWormSpawnIntervalMin, kWormSpawnIntervalMax);
        spawnTimerWorms_ = timerDist(rng_);
    } else if (!underground) {
        spawnTimerWorms_ = 0.0F;
    }

    for (auto& worm : worms_) {
        if (!worm.alive()) {
            if (!worm.droppedLoot) {
                const int drop = std::uniform_int_distribution<int>(kWormCoinMin, kWormCoinMax)(rng_);
                player_.addToInventory(world::TileType::Coin, drop);
                damageNumbers_.addLoot(worm.position, drop);
                worm.droppedLoot = true;
            }
            continue;
        }
        const bool onScreen = worm.position.x >= static_cast<float>(view.startX)
            && worm.position.x <= static_cast<float>(view.startX + view.tilesWide)
            && worm.position.y >= static_cast<float>(view.startY)
            && worm.position.y <= static_cast<float>(view.startY + view.tilesTall);
        if (!onScreen) {
            worm.offscreenTimer += dt;
            if (worm.offscreenTimer >= kOffscreenDespawnTime) {
                worm.health = 0;
                continue;
            }
        } else {
            worm.offscreenTimer = 0.0F;
        }

        worm.attackCooldown = std::max(0.0F, worm.attackCooldown - dt);
        worm.lungeCooldown = std::max(0.0F, worm.lungeCooldown - dt);

        const int solidTileX = static_cast<int>(std::floor(worm.position.x));
        const int solidTileY = static_cast<int>(std::floor(worm.position.y));
        const bool inSolid = physics_.isSolidTile(solidTileX, solidTileY);
        const bool overlappingPlayer = physics_.aabbOverlap(worm.position,
                                                            entities::kWormRadius,
                                                            entities::kWormRadius * 2.0F,
                                                            player_.position(),
                                                            entities::kPlayerHalfWidth,
                                                            entities::kPlayerHeight);

        entities::Vec2 desiredDir{0.0F, 0.0F};
        bool hasDesiredDir = false;
        if (worm.airTimer > 0.0F) {
            worm.airTimer = std::max(0.0F, worm.airTimer - dt);
            const float gravity = (worm.velocity.y < 0.0F) ? kWormAirGravityUp : kWormAirGravity;
            worm.velocity.y += gravity * dt;
        } else if (worm.knockbackTimer > 0.0F) {
            worm.velocity.x = worm.knockbackVelocity;
            worm.knockbackVelocity *= 0.9F;
            worm.knockbackTimer = std::max(0.0F, worm.knockbackTimer - dt);
        } else {
            const float phase = swoopTimer_ * 1.6F + static_cast<float>(worm.id) * 0.6F;
            const float offsetX = std::sin(phase) * 2.0F;
            const float offsetY = std::cos(phase) * 1.5F;
            const entities::Vec2 target{player_.position().x + offsetX, player_.position().y + 2.5F + offsetY};
            entities::Vec2 delta{target.x - worm.position.x, target.y - worm.position.y};
            for (const auto& other : worms_) {
                if (&other == &worm || !other.alive()) {
                    continue;
                }
                const float dx = worm.position.x - other.position.x;
                const float dy = worm.position.y - other.position.y;
                const float distSq = dx * dx + dy * dy;
                if (distSq > 0.001F && distSq < kWormSeparationRadius * kWormSeparationRadius) {
                    const float dist = std::sqrt(distSq);
                    const float push = (1.0F - dist / kWormSeparationRadius) * kWormSeparationStrength;
                    delta.x += (dx / dist) * push;
                    delta.y += (dy / dist) * push;
                }
            }
            const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (distance > 0.01F) {
                delta.x /= distance;
                delta.y /= distance;
                desiredDir = delta;
                hasDesiredDir = true;
                const float targetVX = delta.x * kWormMoveSpeed;
                const float targetVY = delta.y * kWormMoveSpeed;
                const float blend = std::clamp(kWormTurnRate * dt, 0.0F, 1.0F);
                worm.velocity.x = worm.velocity.x + (targetVX - worm.velocity.x) * blend;
                worm.velocity.y = worm.velocity.y + (targetVY - worm.velocity.y) * blend;
            } else if (!overlappingPlayer) {
                worm.velocity = {0.0F, 0.0F};
            }
        }

        bool edgeAhead = false;
        if (inSolid && hasDesiredDir) {
            const int stepX = (desiredDir.x > 0.2F) ? 1 : (desiredDir.x < -0.2F ? -1 : 0);
            const int stepY = (desiredDir.y > 0.2F) ? 1 : (desiredDir.y < -0.2F ? -1 : 0);
            if (stepX != 0 || stepY != 0) {
                const int checkX = solidTileX + stepX;
                const int checkY = solidTileY + stepY;
                if (checkX < 0 || checkY < 0 || checkX >= world_.width() || checkY >= world_.height()
                    || !physics_.isSolidTile(checkX, checkY)) {
                    edgeAhead = true;
                }
            }
        }

        if (worm.airTimer <= 0.0F && worm.lungeCooldown <= 0.0F && inSolid) {
            const entities::Vec2 toPlayer{player_.position().x - worm.position.x,
                                          player_.position().y - worm.position.y};
            const float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
            if (dist < kWormLungeRange || edgeAhead) {
                const float dirX = (dist > 0.01F) ? (toPlayer.x / dist) : (hasDesiredDir ? desiredDir.x : 1.0F);
                float dirY = (dist > 0.01F) ? (toPlayer.y / dist) : (hasDesiredDir ? desiredDir.y : -1.0F);
                dirY = std::clamp(dirY, -kWormLungeVerticalBias, kWormLungeVerticalBias);
                const float len = std::sqrt(dirX * dirX + dirY * dirY);
                const float normX = (len > 0.01F) ? (dirX / len) : dirX;
                const float normY = (len > 0.01F) ? (dirY / len) : dirY;
                worm.velocity.x = normX * kWormLungeSpeed;
                worm.velocity.y = normY * kWormLungeSpeed;
                worm.airTimer = kWormAirTime;
                worm.lungeCooldown = kWormLungeCooldown;
            }
        }

        worm.position.x += worm.velocity.x * dt;
        worm.position.y += worm.velocity.y * dt;
        worm.position.x = std::clamp(worm.position.x, 0.5F, static_cast<float>(world_.width() - 1));
        worm.position.y = std::clamp(worm.position.y, 2.0F, static_cast<float>(world_.height() - 2));

        const int tileX = static_cast<int>(std::floor(worm.position.x));
        int tileY = static_cast<int>(std::floor(worm.position.y));
        const bool inSolidNow = physics_.isSolidTile(tileX, tileY);
        if (worm.airTimer <= 0.0F && !inSolidNow) {
            worm.airTimer = 0.1F;
        } else if (worm.airTimer <= 0.0F) {
            int adjustSteps = 0;
            while (adjustSteps < 6 && !physics_.isSolidTile(tileX, tileY)) {
                worm.position.y = std::min(worm.position.y + kWormRecenterSpeed, static_cast<float>(world_.height() - 2));
                tileY = static_cast<int>(std::floor(worm.position.y));
                adjustSteps += 1;
            }
        }

        if (overlappingPlayer && worm.attackCooldown <= 0.0F) {
            const int dealt = player_.applyDamage(kWormDamage);
            damageNumbers_.addDamage(player_.position(), dealt, true);
            const float dir = (player_.position().x < worm.position.x) ? -1.0F : 1.0F;
            entities::Vec2 knock = player_.velocity();
            knock.x += dir * 8.0F;
            knock.y = -9.0F;
            player_.setVelocity(knock);
            worm.attackCooldown = kWormAttackInterval;
        }
    }

    worms_.erase(
        std::remove_if(worms_.begin(), worms_.end(), [](const entities::Worm& worm) { return !worm.alive(); }),
        worms_.end());
}

void EnemyManager::updateDragon(float dt) {
    if (!dragonActive_ || dragonDefeated_) {
        return;
    }
    if (!dragonSpawned_) {
        spawnDragon();
    }
    if (!dragon_.alive()) {
        if (!dragon_.droppedLoot) {
            const int drop = std::uniform_int_distribution<int>(kDragonCoinMin, kDragonCoinMax)(rng_);
            player_.addToInventory(world::TileType::Coin, drop);
            damageNumbers_.addLoot(dragon_.position, drop);
            dragon_.droppedLoot = true;
        }
        dragonDefeated_ = true;
        return;
    }

    dragon_.attackCooldown = std::max(0.0F, dragon_.attackCooldown - dt);
    dragon_.fireCooldown = std::max(0.0F, dragon_.fireCooldown - dt);
    dragon_.breathTimer = std::max(0.0F, dragon_.breathTimer - dt);
    dragon_.breathTick = std::max(0.0F, dragon_.breathTick - dt);
    dragon_.chargeTimer = std::max(0.0F, dragon_.chargeTimer - dt);
    dragon_.chargeCooldown = std::max(0.0F, dragon_.chargeCooldown - dt);
    dragon_.volleyCooldown = std::max(0.0F, dragon_.volleyCooldown - dt);
    dragon_.facing = (player_.position().x >= dragon_.position.x) ? 1.0F : -1.0F;

    if (dragon_.knockbackTimer > 0.0F) {
        dragon_.velocity.x = dragon_.knockbackVelocity;
        dragon_.knockbackVelocity *= 0.9F;
        dragon_.knockbackTimer = std::max(0.0F, dragon_.knockbackTimer - dt);
    } else if (dragon_.chargeTimer > 0.0F) {
        // Keep charge momentum during the dash.
    } else {
        const float bob = std::sin(swoopTimer_ * 1.05F) * 0.4F;
        entities::Vec2 target = player_.position();
        target.y += bob;
        const float minX = dragonDenCenter_.x - dragonDenRadiusX_ + entities::kDragonHalfWidth;
        const float maxX = dragonDenCenter_.x + dragonDenRadiusX_ - entities::kDragonHalfWidth;
        const float minY = dragonDenCenter_.y - dragonDenRadiusY_ + entities::kDragonHeight;
        const float maxY = dragonDenCenter_.y + dragonDenRadiusY_ - 0.4F;
        target.x = std::clamp(target.x, minX, maxX);
        target.y = std::clamp(target.y, minY, maxY);
        entities::Vec2 delta{target.x - dragon_.position.x, target.y - dragon_.position.y};
        const float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (dist > 0.05F) {
            delta.x /= dist;
            delta.y /= dist;
            const entities::Vec2 desired{delta.x * kDragonMoveSpeed, delta.y * kDragonMoveSpeed};
            const float blend = std::clamp(4.2F * dt, 0.0F, 1.0F);
            dragon_.velocity.x += (desired.x - dragon_.velocity.x) * blend;
            dragon_.velocity.y += (desired.y - dragon_.velocity.y) * blend;
        } else {
            dragon_.velocity.x *= 0.85F;
            dragon_.velocity.y *= 0.85F;
        }
    }

    dragon_.position.x += dragon_.velocity.x * dt;
    dragon_.position.y += dragon_.velocity.y * dt;
    const float minX = dragonDenCenter_.x - dragonDenRadiusX_ + entities::kDragonHalfWidth;
    const float maxX = dragonDenCenter_.x + dragonDenRadiusX_ - entities::kDragonHalfWidth;
    const float minY = dragonDenCenter_.y - dragonDenRadiusY_ + entities::kDragonHeight;
    const float maxY = dragonDenCenter_.y + dragonDenRadiusY_ - 0.4F;
    dragon_.position.x = std::clamp(dragon_.position.x, minX, maxX);
    dragon_.position.y = std::clamp(dragon_.position.y, minY, maxY);

    if (dragon_.chargeCooldown <= 0.0F && dragon_.chargeTimer <= 0.0F) {
        entities::Vec2 dir{player_.position().x - dragon_.position.x,
                           player_.position().y - dragon_.position.y};
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 0.01F) {
            dir.x /= len;
            dir.y /= len;
        } else {
            dir = {1.0F, 0.0F};
        }
        dragon_.velocity = {dir.x * kDragonChargeSpeed, dir.y * kDragonChargeSpeed};
        dragon_.chargeTimer = kDragonChargeDuration;
        dragon_.chargeCooldown = kDragonChargeCooldown;
    }

    if (dragon_.breathTimer <= 0.0F && dragon_.fireCooldown <= 0.0F) {
        dragon_.breathTimer = kDragonBreathDuration;
        dragon_.breathTick = 0.0F;
        dragon_.fireCooldown = kDragonFireInterval;
    }
    if (dragon_.breathTimer > 0.0F && dragon_.breathTick <= 0.0F) {
        const float headOffset = entities::kDragonHeight * kDragonSegmentGapFactor
            * (static_cast<float>(kDragonSegments - 1) * 0.5F);
        const entities::Vec2 headPos{dragon_.position.x + dragon_.facing * headOffset,
                                     dragon_.position.y - entities::kDragonHeight * kDragonSegmentYFactor
                                         + entities::kDragonHeight * kDragonHeadYOffsetFactor};
        entities::Vec2 dir{player_.position().x - headPos.x,
                           player_.position().y - headPos.y};
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 0.01F) {
            dir.x /= len;
            dir.y /= len;
        } else {
            dir = {dragon_.facing, 0.0F};
        }
        std::uniform_real_distribution<float> spreadDist(-kDragonBreathSpread, kDragonBreathSpread);
        const float spread = spreadDist(rng_);
        const float angle = std::atan2(dir.y, dir.x) + spread;
        EnemyProjectile projectile{};
        projectile.position = headPos;
        projectile.velocity = {std::cos(angle) * kDragonBreathSpeed, std::sin(angle) * kDragonBreathSpeed};
        projectile.lifetime = kDragonBreathLifetime;
        projectile.radius = kDragonBreathRadius;
        projectile.damage = kDragonProjectileDamage;
        projectile.isFlame = true;
        projectile.fromDragon = true;
        enemyProjectiles_.push_back(projectile);
        dragon_.breathTick = kDragonBreathTick;
    }

    if (dragon_.volleyCooldown <= 0.0F) {
        const float headOffset = entities::kDragonHeight * kDragonSegmentGapFactor
            * (static_cast<float>(kDragonSegments - 1) * 0.5F);
        const entities::Vec2 headPos{dragon_.position.x + dragon_.facing * headOffset,
                                     dragon_.position.y - entities::kDragonHeight * kDragonSegmentYFactor
                                         + entities::kDragonHeight * kDragonHeadYOffsetFactor};
        entities::Vec2 dir{player_.position().x - headPos.x,
                           player_.position().y - headPos.y};
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 0.01F) {
            dir.x /= len;
            dir.y /= len;
        } else {
            dir = {dragon_.facing, 0.0F};
        }
        const float baseAngle = std::atan2(dir.y, dir.x);
        const std::array<float, 3> offsets{-kDragonVolleySpread, 0.0F, kDragonVolleySpread};
        for (float offset : offsets) {
            const float angle = baseAngle + offset;
            EnemyProjectile projectile{};
            projectile.position = headPos;
            projectile.velocity = {std::cos(angle) * kDragonVolleySpeed, std::sin(angle) * kDragonVolleySpeed};
            projectile.lifetime = kDragonVolleyLifetime;
            projectile.radius = kDragonVolleyRadius;
            projectile.damage = kDragonVolleyDamage;
            projectile.isFlame = false;
            projectile.fromDragon = true;
            enemyProjectiles_.push_back(projectile);
        }
        dragon_.volleyCooldown = kDragonVolleyInterval;
    }

    if (dragon_.attackCooldown <= 0.0F
        && physics_.aabbOverlap(dragon_.position,
                                entities::kDragonHalfWidth,
                                entities::kDragonHeight,
                                player_.position(),
                                entities::kPlayerHalfWidth,
                                entities::kPlayerHeight)) {
        const int dealt = player_.applyDamage(kDragonContactDamage);
        damageNumbers_.addDamage(player_.position(), dealt, true);
        const float dir = (player_.position().x < dragon_.position.x) ? -1.0F : 1.0F;
        entities::Vec2 knock = player_.velocity();
        knock.x += dir * 9.0F;
        knock.y = -10.0F;
        player_.setVelocity(knock);
        dragon_.attackCooldown = kDragonAttackInterval;
    }
}

void EnemyManager::updateEnemyProjectiles(float dt) {
    for (auto& projectile : enemyProjectiles_) {
        projectile.lifetime -= dt;
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        projectile.position.x += projectile.velocity.x * dt;
        projectile.position.y += projectile.velocity.y * dt;
        if (projectile.position.x < 0.0F || projectile.position.y < 0.0F
            || projectile.position.x >= static_cast<float>(world_.width())
            || projectile.position.y >= static_cast<float>(world_.height())
            || physics_.solidAtPosition(projectile.position)) {
            projectile.lifetime = 0.0F;
            continue;
        }
        if (physics_.aabbOverlap(projectile.position,
                                 projectile.radius,
                                 projectile.radius * 2.0F,
                                 player_.position(),
                                 entities::kPlayerHalfWidth,
                                 entities::kPlayerHeight)) {
            const int dealt = player_.applyDamage(projectile.damage);
            damageNumbers_.addDamage(player_.position(), dealt, true);
            const float dir = (projectile.velocity.x < 0.0F) ? -1.0F : 1.0F;
            entities::Vec2 knock = player_.velocity();
            knock.x += dir * 5.5F;
            knock.y = -6.0F;
            player_.setVelocity(knock);
            projectile.lifetime = 0.0F;
        }
    }

    enemyProjectiles_.erase(
        std::remove_if(enemyProjectiles_.begin(), enemyProjectiles_.end(), [](const EnemyProjectile& projectile) {
            return projectile.lifetime <= 0.0F;
        }),
        enemyProjectiles_.end());
}

void EnemyManager::spawnZombie(const ViewBounds& view) {
    if (world_.width() <= 2) {
        return;
    }

    std::uniform_int_distribution<int> sideDist(0, 1);
    const int margin = 6;
    int column = (sideDist(rng_) == 0) ? (view.startX - margin) : (view.startX + view.tilesWide + margin);
    column = std::clamp(column, 1, world_.width() - 2);

    int ground = -1;
    for (int y = 0; y < world_.height(); ++y) {
        if (physics_.isSolidTile(column, y)) {
            ground = y;
            break;
        }
    }
    if (ground <= 0) {
        return;
    }

    const int spawnY = ground - 1;
    entities::Zombie zombie;
    zombie.position = {static_cast<float>(column) + 0.5F, static_cast<float>(spawnY)};
    zombie.velocity = {0.0F, 0.0F};
    zombie.onGround = false;
    zombie.id = nextZombieId_++;
    zombie.lastX = zombie.position.x;
    zombie.health = zombie.maxHealth;
    zombie.attackCooldown = 0.0F;
    zombie.jumpCooldown = 0.0F;
    zombie.stuckTimer = 0.0F;
    zombie.pathTimer = 0.0F;
    zombie.desiredDir = 0;
    zombie.lungeCooldown = 0.0F;
    zombie.lungeTimer = 0.0F;
    zombie.knockbackTimer = 0.0F;
    zombie.knockbackVelocity = 0.0F;

    if (physics_.collidesAabb(zombie.position, entities::kZombieHalfWidth, entities::kZombieHeight)) {
        return;
    }

    zombies_.push_back(zombie);
}

void EnemyManager::spawnFlyer(const ViewBounds& view) {
    if (world_.width() <= 2) {
        return;
    }

    std::uniform_int_distribution<int> sideDist(0, 1);
    const int margin = 6;
    int column = (sideDist(rng_) == 0) ? (view.startX - margin) : (view.startX + view.tilesWide + margin);
    column = std::clamp(column, 1, world_.width() - 2);

    int ground = -1;
    for (int y = 0; y < world_.height(); ++y) {
        if (physics_.isSolidTile(column, y)) {
            ground = y;
            break;
        }
    }
    if (ground <= 0) {
        return;
    }

    const float spawnY = static_cast<float>(std::max(2, ground - 6));
    entities::FlyingEnemy enemy;
    enemy.position = {static_cast<float>(column) + 0.5F, spawnY};
    enemy.velocity = {0.0F, 0.0F};
    enemy.id = nextFlyerId_++;
    enemy.health = enemy.maxHealth;
    enemy.attackCooldown = 0.0F;

    if (physics_.solidAtPosition(enemy.position)) {
        return;
    }

    flyers_.push_back(enemy);
}

void EnemyManager::spawnWorm(const ViewBounds& view) {
    if (world_.width() <= 2) {
        return;
    }

    std::uniform_int_distribution<int> sideDist(0, 1);
    const int margin = 8;
    int column = (sideDist(rng_) == 0) ? (view.startX - margin) : (view.startX + view.tilesWide + margin);
    column = std::clamp(column, 1, world_.width() - 2);

    const float minDepth = static_cast<float>(world_.height()) * kWormSpawnDepthRatio;
    const float baseY = std::clamp(player_.position().y + std::uniform_real_distribution<float>(-4.0F, 6.0F)(rng_),
                                   minDepth,
                                   static_cast<float>(world_.height() - 2));
    entities::Worm worm;
    worm.position = {static_cast<float>(column) + 0.5F, baseY};
    worm.velocity = {0.0F, 0.0F};
    worm.id = nextWormId_++;
    worm.health = worm.maxHealth;
    worm.attackCooldown = 0.0F;

    int tileX = static_cast<int>(std::floor(worm.position.x));
    int tileY = static_cast<int>(std::floor(worm.position.y));
    int search = 0;
    while (search < 8 && !physics_.isSolidTile(tileX, tileY)) {
        worm.position.y = std::min(worm.position.y + 1.0F, static_cast<float>(world_.height() - 2));
        tileY = static_cast<int>(std::floor(worm.position.y));
        search += 1;
    }
    if (!physics_.isSolidTile(tileX, tileY)) {
        return;
    }

    worms_.push_back(worm);
}

void EnemyManager::spawnDragon() {
    dragon_ = {};
    dragon_.position = dragonDenCenter_;
    dragon_.velocity = {0.0F, 0.0F};
    dragon_.health = dragon_.maxHealth;
    dragon_.attackCooldown = 0.0F;
    dragon_.fireCooldown = 1.0F;
    dragon_.breathTimer = 0.0F;
    dragon_.breathTick = 0.0F;
    dragon_.chargeTimer = 0.0F;
    dragon_.chargeCooldown = 1.5F;
    dragon_.volleyCooldown = 1.0F;
    dragon_.facing = 1.0F;
    dragon_.knockbackTimer = 0.0F;
    dragon_.knockbackVelocity = 0.0F;
    dragon_.droppedLoot = false;
    dragonSpawned_ = true;
}

bool EnemyManager::isWalkableSpot(int x, int footY) const {
    if (x < 0 || x >= world_.width()) {
        return false;
    }
    if (footY < 2 || footY >= world_.height()) {
        return false;
    }
    if (!physics_.isSolidTile(x, footY)) {
        return false;
    }
    if (physics_.isSolidTile(x, footY - 1) || physics_.isSolidTile(x, footY - 2)) {
        return false;
    }
    return true;
}

bool EnemyManager::pathClear(int startX, int endX, int footY) const {
    if (startX == endX) {
        return true;
    }
    const int step = (endX < startX) ? -1 : 1;
    for (int x = startX; x != endX; x += step) {
        if (physics_.isSolidTile(x, footY - 1) || physics_.isSolidTile(x, footY - 2)) {
            return false;
        }
    }
    return true;
}

int EnemyManager::computeZombiePathDirection(const entities::Zombie& zombie) const {
    const int startX = static_cast<int>(std::floor(zombie.position.x));
    const int footY = static_cast<int>(std::floor(zombie.position.y));
    const float playerX = player_.position().x;
    const int directDir = (playerX < zombie.position.x) ? -1 : 1;
    const int targetX = static_cast<int>(std::floor(playerX));
    if (pathClear(startX, targetX, footY)) {
        return directDir;
    }

    constexpr int kScanRange = 14;
    int bestDir = directDir;
    float bestScore = std::numeric_limits<float>::infinity();
    for (int offset = 1; offset <= kScanRange; ++offset) {
        const int leftX = startX - offset;
        if (leftX >= 0 && pathClear(startX, leftX, footY) && isWalkableSpot(leftX, footY)) {
            const float score = std::fabs(static_cast<float>(leftX) - playerX);
            if (score < bestScore) {
                bestScore = score;
                bestDir = -1;
            }
        }
        const int rightX = startX + offset;
        if (rightX < world_.width() && pathClear(startX, rightX, footY) && isWalkableSpot(rightX, footY)) {
            const float score = std::fabs(static_cast<float>(rightX) - playerX);
            if (score < bestScore) {
                bestScore = score;
                bestDir = 1;
            }
        }
    }
    return bestDir;
}

void EnemyManager::applyZombiePhysics(entities::Zombie& zombie, float dt) {
    const float horizontalDelta = std::fabs(zombie.position.x - zombie.lastX);
    if (horizontalDelta < 0.02F) {
        zombie.stuckTimer += dt;
    } else {
        zombie.stuckTimer = 0.0F;
    }
    zombie.jumpCooldown = std::max(0.0F, zombie.jumpCooldown - dt);

    int desiredDir = zombie.desiredDir;
    if (desiredDir == 0) {
        desiredDir = (player_.position().x < zombie.position.x) ? -1 : 1;
    }
    float direction = static_cast<float>(desiredDir);

    if (zombie.knockbackTimer > 0.0F) {
        zombie.velocity.x = zombie.knockbackVelocity;
        zombie.knockbackVelocity *= 0.88F;
        zombie.knockbackTimer = std::max(0.0F, zombie.knockbackTimer - dt);
    } else if (zombie.lungeTimer > 0.0F) {
        zombie.velocity.x = direction * kZombieMoveSpeed * 2.2F;
    } else {
        zombie.velocity.x = direction * kZombieMoveSpeed;
    }

    const int aheadX = static_cast<int>(std::floor(zombie.position.x + direction * (entities::kZombieHalfWidth + 0.2F)));
    const int footY = static_cast<int>(std::floor(zombie.position.y));
    bool obstacle = false;
    for (int y = footY - 1; y <= footY; ++y) {
        if (physics_.isSolidTile(aheadX, y)) {
            obstacle = true;
            break;
        }
    }

    if (zombie.onGround && zombie.jumpCooldown <= 0.0F) {
        if (obstacle) {
            zombie.velocity.y = -kZombieJumpVelocity * 0.95F;
            zombie.onGround = false;
            zombie.jumpCooldown = 0.9F;
        }
    }

    if (zombie.stuckTimer > 1.5F && zombie.onGround) {
        zombie.stuckTimer = 0.0F;
    }

    zombie.velocity.y += kGravity * dt;

    entities::Vec2 position = zombie.position;
    position.x += zombie.velocity.x * dt;
    physics_.resolveHorizontalAabb(position, zombie.velocity, entities::kZombieHalfWidth, entities::kZombieHeight);

    position.y += zombie.velocity.y * dt;
    physics_.resolveVerticalAabb(position, zombie.velocity, entities::kZombieHalfWidth, entities::kZombieHeight, zombie.onGround);

    const float maxX = static_cast<float>(world_.width() - 1);
    const float maxY = static_cast<float>(world_.height() - 1);
    position.x = std::clamp(position.x, 0.0F, maxX);
    position.y = std::clamp(position.y, 0.0F, maxY);
    zombie.position = position;
    zombie.lastX = zombie.position.x;
}

void EnemyManager::fillHud(rendering::HudState& hud) const {
    const int enemyCount = std::min(static_cast<int>(flyers_.size()), rendering::kMaxFlyingEnemies);
    hud.flyingEnemyCount = enemyCount;
    for (int i = 0; i < enemyCount; ++i) {
        const auto& enemy = flyers_[static_cast<std::size_t>(i)];
        auto& entry = hud.flyingEnemies[static_cast<std::size_t>(i)];
        entry.active = enemy.alive();
        entry.x = enemy.position.x;
        entry.y = enemy.position.y;
        entry.radius = entities::kFlyingEnemyRadius;
        entry.health = enemy.health;
        entry.maxHealth = enemy.maxHealth;
    }
    for (int i = enemyCount; i < rendering::kMaxFlyingEnemies; ++i) {
        hud.flyingEnemies[static_cast<std::size_t>(i)] = {};
    }

    const int projectileCount = std::min(static_cast<int>(enemyProjectiles_.size()), rendering::kMaxEnemyProjectiles);
    hud.enemyProjectileCount = projectileCount;
    for (int i = 0; i < projectileCount; ++i) {
        const auto& projectile = enemyProjectiles_[static_cast<std::size_t>(i)];
        auto& entry = hud.enemyProjectiles[static_cast<std::size_t>(i)];
        entry.active = projectile.lifetime > 0.0F;
        entry.x = projectile.position.x;
        entry.y = projectile.position.y;
        entry.radius = projectile.radius;
        entry.isFlame = projectile.isFlame;
    }
    for (int i = projectileCount; i < rendering::kMaxEnemyProjectiles; ++i) {
        hud.enemyProjectiles[static_cast<std::size_t>(i)] = {};
    }

    const int wormCount = std::min(static_cast<int>(worms_.size()), rendering::kMaxWorms);
    hud.wormCount = wormCount;
    for (int i = 0; i < wormCount; ++i) {
        const auto& worm = worms_[static_cast<std::size_t>(i)];
        auto& entry = hud.worms[static_cast<std::size_t>(i)];
        entry.active = worm.alive();
        entry.x = worm.position.x;
        entry.y = worm.position.y;
        entry.vx = worm.velocity.x;
        entry.vy = worm.velocity.y;
        entry.radius = entities::kWormRadius;
        entry.health = worm.health;
        entry.maxHealth = worm.maxHealth;
    }
    for (int i = wormCount; i < rendering::kMaxWorms; ++i) {
        hud.worms[static_cast<std::size_t>(i)] = {};
    }

    hud.dragon.active = dragonActive_ && dragon_.alive();
    if (hud.dragon.active) {
        hud.dragon.x = dragon_.position.x;
        hud.dragon.y = dragon_.position.y;
        hud.dragon.halfWidth = entities::kDragonHalfWidth;
        hud.dragon.height = entities::kDragonHeight;
        hud.dragon.wingPhase = swoopTimer_ * 3.0F;
        hud.dragon.facing = dragon_.facing;
        hud.dragon.health = dragon_.health;
        hud.dragon.maxHealth = dragon_.maxHealth;
    } else {
        hud.dragon = {};
    }
}

} // namespace terraria::game
