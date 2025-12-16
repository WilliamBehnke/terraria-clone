#include "terraria/game/Game.h"

#include <algorithm>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>

namespace terraria::game {

namespace {
constexpr float kGravity = 60.0F;
constexpr float kJumpVelocity = 25.0F;
constexpr float kCollisionEpsilon = 0.001F;
constexpr float kTilePixels = 8.0F;
constexpr float kBreakRange = 6.0F;
constexpr float kPlaceRange = 6.0F;
constexpr float kZombieMoveSpeed = 6.0F;
constexpr float kZombieSpawnIntervalMin = 3.0F;
constexpr float kZombieSpawnIntervalMax = 6.0F;
constexpr int kMaxZombies = 12;
constexpr float kPlayerAttackCooldown = 0.35F;
constexpr int kPlayerAttackDamage = 18;
constexpr int kZombieDamage = 10;
constexpr float kZombieAttackInterval = 1.2F;
constexpr float kZombieJumpVelocity = 22.0F;
constexpr float kDayLengthSeconds = 180.0F;
constexpr float kNightStart = 0.55F;
constexpr float kNightEnd = 0.95F;

} // namespace

Game::Game(const core::AppConfig& config)
    : config_{config},
      world_{config.worldWidth, config.worldHeight},
      generator_{},
      player_{},
      renderer_{rendering::CreateSdlRenderer(config_)},
      inputSystem_{input::CreateSdlInputSystem()},
      zombieRng_{static_cast<std::uint32_t>(config.worldWidth * 131 + config.worldHeight * 977)},
      dayLength_{kDayLengthSeconds} {}

void Game::initialize() {
    generator_.generate(world_);

    const int spawnX = config_.worldWidth / 2;
    int spawnY = 0;
    for (int y = 0; y < world_.height(); ++y) {
        if (world_.tile(spawnX, y).active) {
            spawnY = (y > 0) ? y - 1 : 0;
            break;
        }
    }
    player_.setPosition({static_cast<float>(spawnX), static_cast<float>(spawnY)});
    spawnPosition_ = player_.position();
    player_.resetHealth();
    std::uniform_real_distribution<float> timerDist(kZombieSpawnIntervalMin, kZombieSpawnIntervalMax);
    zombieSpawnTimer_ = timerDist(zombieRng_);
    const float initialNight = (kNightStart + kNightEnd) * 0.5F;
    timeOfDay_ = dayLength_ * initialNight;
    isNight_ = true;

    renderer_->initialize();
    inputSystem_->initialize();
}

bool Game::tick() {
    const float dt = 1.0F / static_cast<float>(config_.targetFps);
    handleInput();
    update(dt);
    processActions(dt);
    updateHudState();
    render();
    return !inputSystem_->shouldQuit();
}

void Game::shutdown() {
    inputSystem_->shutdown();
    renderer_->shutdown();
}

void Game::handleInput() {
    inputSystem_->poll();
    if (inputSystem_->state().toggleCamera) {
        toggleCameraMode();
    }

    if (cameraMode_) {
        const float moveSpeed = cameraSpeed_;
        const float delta = moveSpeed / static_cast<float>(config_.targetFps);
        cameraPosition_.x += inputSystem_->state().camMoveX * delta;
        cameraPosition_.y += inputSystem_->state().camMoveY * delta;
        cameraPosition_ = clampCameraTarget(cameraPosition_);
        player_.setVelocity({0.0F, 0.0F});
        jumpRequested_ = false;
    } else {
        constexpr float moveSpeed = 12.0F;
        entities::Vec2 velocity = player_.velocity();
        velocity.x = inputSystem_->state().moveX * moveSpeed;
        player_.setVelocity(velocity);
        jumpRequested_ = inputSystem_->state().jump;
    }

    const int selection = inputSystem_->state().hotbarSelection;
    if (selection >= 0 && selection < static_cast<int>(hotbarTypes_.size())) {
        selectedHotbar_ = selection;
    }
    selectedHotbar_ = std::clamp(selectedHotbar_, 0, static_cast<int>(hotbarTypes_.size()) - 1);
}

void Game::update(float dt) {
    if (!cameraMode_) {
        entities::Vec2 velocity = player_.velocity();
        velocity.y += kGravity * dt;

        if (jumpRequested_ && player_.onGround()) {
            velocity.y = -kJumpVelocity;
            player_.setOnGround(false);
        }
        jumpRequested_ = false;

        entities::Vec2 position = player_.position();
        position.x += velocity.x * dt;
        resolveHorizontal(position, velocity);

        position.y += velocity.y * dt;
        resolveVertical(position, velocity);

        const float maxX = static_cast<float>(world_.width() - 1);
        const float maxY = static_cast<float>(world_.height() - 1);
        position.x = std::clamp(position.x, 0.0F, maxX);
        position.y = std::clamp(position.y, 0.0F, maxY);

        player_.setPosition(position);
        player_.setVelocity(velocity);
        cameraPosition_ = clampCameraTarget(position);
    }

    updateDayNight(dt);
    updateZombies(dt);
    if (player_.health() <= 0) {
        player_.resetHealth();
        player_.setPosition(spawnPosition_);
        player_.setVelocity({0.0F, 0.0F});
        player_.setOnGround(false);
    }
}

void Game::render() {
    renderer_->render(world_, player_, zombies_, hudState_);
}

bool Game::collides(const entities::Vec2& pos) const {
    return collidesAabb(pos, entities::kPlayerHalfWidth, entities::kPlayerHeight);
}

bool Game::collidesAabb(const entities::Vec2& pos, float halfWidth, float height) const {
    const float left = pos.x - halfWidth;
    const float right = pos.x + halfWidth;
    const float top = pos.y - height;
    const float bottom = pos.y;

    const int startX = static_cast<int>(std::floor(left));
    const int endX = static_cast<int>(std::floor(right));
    const int startY = static_cast<int>(std::floor(top));
    const int endY = static_cast<int>(std::floor(bottom));

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            if (isSolidTile(x, y)) {
                return true;
            }
        }
    }
    return false;
}

bool Game::isSolidTile(int x, int y) const {
    if (x < 0 || x >= world_.width() || y >= world_.height()) {
        return true;
    }
    if (y < 0) {
        return false;
    }
    const auto& tile = world_.tile(x, y);
    if (!tile.active) {
        return false;
    }
    switch (tile.type) {
    case world::TileType::Air:
    case world::TileType::TreeTrunk:
    case world::TileType::TreeLeaves:
        return false;
    default:
        return true;
    }
}

void Game::resolveHorizontal(entities::Vec2& position, entities::Vec2& velocity) {
    resolveHorizontalAabb(position, velocity, entities::kPlayerHalfWidth, entities::kPlayerHeight);
}

void Game::resolveVertical(entities::Vec2& position, entities::Vec2& velocity) {
    bool grounded = player_.onGround();
    resolveVerticalAabb(position, velocity, entities::kPlayerHalfWidth, entities::kPlayerHeight, grounded);
    player_.setOnGround(grounded);
}

void Game::resolveHorizontalAabb(entities::Vec2& position,
                                 entities::Vec2& velocity,
                                 float halfWidth,
                                 float height) {
    if (!collidesAabb(position, halfWidth, height)) {
        return;
    }

    if (velocity.x > 0.0F) {
        const int tileX = static_cast<int>(std::floor(position.x + halfWidth));
        position.x = static_cast<float>(tileX) - halfWidth - kCollisionEpsilon;
    } else if (velocity.x < 0.0F) {
        const int tileX = static_cast<int>(std::floor(position.x - halfWidth));
        position.x = static_cast<float>(tileX + 1) + halfWidth + kCollisionEpsilon;
    } else {
        position.x = std::round(position.x);
    }
    velocity.x = 0.0F;
}

void Game::resolveVerticalAabb(entities::Vec2& position,
                               entities::Vec2& velocity,
                               float halfWidth,
                               float height,
                               bool& onGround) {
    if (!collidesAabb(position, halfWidth, height)) {
        onGround = false;
        return;
    }

    if (velocity.y > 0.0F) {
        const int tileY = static_cast<int>(std::floor(position.y));
        position.y = static_cast<float>(tileY) - kCollisionEpsilon;
        onGround = true;
    } else if (velocity.y < 0.0F) {
        const int tileY = static_cast<int>(std::floor(position.y - height));
        position.y = static_cast<float>(tileY + 1) + height + kCollisionEpsilon;
    } else {
        const int tileY = static_cast<int>(std::floor(position.y));
        position.y = static_cast<float>(tileY) - kCollisionEpsilon;
        onGround = true;
    }
    velocity.y = 0.0F;
}

void Game::processActions(float dt) {
    handleBreaking(dt);
    handlePlacement(dt);
}

bool Game::cursorWorldTile(int& outX, int& outY) const {
    const auto& state = inputSystem_->state();
    const int tilesWide = config_.windowWidth / static_cast<int>(kTilePixels) + 2;
    const int tilesTall = config_.windowHeight / static_cast<int>(kTilePixels) + 2;
    const int maxStartX = std::max(world_.width() - tilesWide, 0);
    const int maxStartY = std::max(world_.height() - tilesTall, 0);
    const entities::Vec2 focus = cameraFocus();
    float viewX = focus.x - static_cast<float>(tilesWide) / 2.0F;
    float viewY = focus.y - static_cast<float>(tilesTall) / 2.0F;
    viewX = std::clamp(viewX, 0.0F, static_cast<float>(maxStartX));
    viewY = std::clamp(viewY, 0.0F, static_cast<float>(maxStartY));

    const int clampedMouseX = std::clamp(state.mouseX, 0, config_.windowWidth);
    const int clampedMouseY = std::clamp(state.mouseY, 0, config_.windowHeight);
    float worldX = viewX + static_cast<float>(clampedMouseX) / kTilePixels;
    float worldY = viewY + static_cast<float>(clampedMouseY) / kTilePixels;

    const float widthF = static_cast<float>(world_.width());
    const float heightF = static_cast<float>(world_.height());
    worldX = std::clamp(worldX, 0.0F, widthF - 0.0001F);
    worldY = std::clamp(worldY, 0.0F, heightF - 0.0001F);

    const float dx = worldX - player_.position().x;
    const float dy = worldY - player_.position().y;
    const float distSq = dx * dx + dy * dy;
    if (distSq <= kPlaceRange * kPlaceRange) {
        outX = static_cast<int>(worldX);
        outY = static_cast<int>(worldY);
        return true;
    }

    const float distance = std::sqrt(distSq);
    if (distance <= 0.0001F) {
        outX = static_cast<int>(worldX);
        outY = static_cast<int>(worldY);
        return true;
    }

    const float dirX = dx / distance;
    const float dirY = dy / distance;
    const float maxDistance = kPlaceRange - 0.1F;
    const float step = 0.25F;
    float traveled = 0.0F;
    while (traveled + step < maxDistance) {
        traveled += step;
    }
    traveled = std::min(traveled, maxDistance);

    float posX = player_.position().x + dirX * traveled;
    float posY = player_.position().y + dirY * traveled;
    posX = std::clamp(posX, 0.0F, widthF - 0.0001F);
    posY = std::clamp(posY, 0.0F, heightF - 0.0001F);

    outX = static_cast<int>(posX);
    outY = static_cast<int>(posY);
    return true;
}

bool Game::canBreakTile(int tileX, int tileY) const {
    const auto& tile = world_.tile(tileX, tileY);
    if (!tile.active || tile.type == world::TileType::Air) {
        return false;
    }
    const float centerX = static_cast<float>(tileX) + 0.5F;
    const float centerY = static_cast<float>(tileY) + 0.5F;
    const float dx = centerX - player_.position().x;
    const float dy = centerY - player_.position().y;
    const float distanceSquared = dx * dx + dy * dy;
    return distanceSquared <= kBreakRange * kBreakRange;
}

bool Game::canPlaceTile(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= world_.width() || tileY < 0 || tileY >= world_.height()) {
        return false;
    }
    if (tileInsidePlayer(tileX, tileY)) {
        return false;
    }
    const auto& tile = world_.tile(tileX, tileY);
    return !tile.active || tile.type == world::TileType::Air;
}

bool Game::withinPlacementRange(int tileX, int tileY) const {
    const float centerX = static_cast<float>(tileX) + 0.5F;
    const float centerY = static_cast<float>(tileY) + 0.5F;
    const float dx = centerX - player_.position().x;
    const float dy = centerY - player_.position().y;
    const float distanceSquared = dx * dx + dy * dy;
    return distanceSquared <= kPlaceRange * kPlaceRange;
}

bool Game::tileInsidePlayer(int tileX, int tileY) const {
    const float left = player_.position().x - entities::kPlayerHalfWidth;
    const float right = player_.position().x + entities::kPlayerHalfWidth;
    const float bottom = player_.position().y;
    const float top = bottom - entities::kPlayerHeight;

    return tileX + 1 > left && tileX < right && tileY + 1 > top && tileY < bottom;
}

void Game::handleBreaking(float dt) {
    const auto& state = inputSystem_->state();
    if (playerAttackCooldown_ > 0.0F) {
        playerAttackCooldown_ = std::max(0.0F, playerAttackCooldown_ - dt);
    }
    if (!state.breakHeld) {
        breakState_ = {};
        return;
    }

    if (playerAttackCooldown_ <= 0.0F && attackZombiesAtCursor()) {
        playerAttackCooldown_ = kPlayerAttackCooldown;
        breakState_ = {};
        return;
    }

    int tileX = 0;
    int tileY = 0;
    if (!cursorWorldTile(tileX, tileY) || !canBreakTile(tileX, tileY)) {
        breakState_ = {};
        return;
    }

    if (!breakState_.active || breakState_.tileX != tileX || breakState_.tileY != tileY) {
        breakState_ = {true, tileX, tileY, 0.0F};
    }

    breakState_.progress += dt * 2.5F;
    if (breakState_.progress < 1.0F) {
        return;
    }

    auto& target = world_.tile(tileX, tileY);
    const world::TileType type = target.type;
    world::TileType dropType = type;
    switch (type) {
    case world::TileType::TreeTrunk: dropType = world::TileType::Wood; break;
    case world::TileType::TreeLeaves: dropType = world::TileType::Leaves; break;
    default: break;
    }
    if (dropType != world::TileType::Air) {
        player_.addToInventory(dropType);
    }
    target = {world::TileType::Air, false};
    breakState_ = {};
}

void Game::handlePlacement(float dt) {
    const auto& state = inputSystem_->state();
    if (!state.placeHeld) {
        placeCooldown_ = 0.0F;
        return;
    }

    if (placeCooldown_ > 0.0F) {
        placeCooldown_ -= dt;
        return;
    }

    int tileX = 0;
    int tileY = 0;
    if (!cursorWorldTile(tileX, tileY) || !withinPlacementRange(tileX, tileY) || !canPlaceTile(tileX, tileY)) {
        return;
    }

    const world::TileType type = hotbarTypes_[static_cast<std::size_t>(selectedHotbar_)];
    if (!player_.removeFromInventory(type)) {
        return;
    }
    world_.tile(tileX, tileY) = {type, true};
    placeCooldown_ = 0.2F;
}

void Game::updateZombies(float dt) {
    if (zombieSpawnTimer_ > 0.0F) {
        zombieSpawnTimer_ -= dt;
    }

    if (isNight_ && zombieSpawnTimer_ <= 0.0F && static_cast<int>(zombies_.size()) < kMaxZombies) {
        spawnZombie();
        std::uniform_real_distribution<float> timerDist(kZombieSpawnIntervalMin, kZombieSpawnIntervalMax);
        zombieSpawnTimer_ = timerDist(zombieRng_);
    } else if (!isNight_) {
        zombieSpawnTimer_ = 0.0F;
    }

    for (auto& ptr : zombies_) {
        if (!ptr) {
            continue;
        }
        auto& zombie = *ptr;
        zombie.attackCooldown = std::max(0.0F, zombie.attackCooldown - dt);
        applyZombiePhysics(zombie, dt);
        if (zombiesOverlapPlayer(zombie) && zombie.attackCooldown <= 0.0F) {
            player_.applyDamage(kZombieDamage);
            zombie.attackCooldown = kZombieAttackInterval;
        }
    }

    zombies_.erase(
        std::remove_if(zombies_.begin(),
                       zombies_.end(),
                       [](const std::unique_ptr<entities::Zombie>& zombie) {
                           return !zombie || !zombie->alive();
                       }),
        zombies_.end());
}

void Game::applyZombiePhysics(entities::Zombie& zombie, float dt) {
    const float direction = (player_.position().x < zombie.position.x) ? -1.0F : 1.0F;
    zombie.velocity.x = direction * kZombieMoveSpeed;
    zombie.velocity.y += kGravity * dt;
    zombie.jumpCooldown = std::max(0.0F, zombie.jumpCooldown - dt);

    if (zombie.onGround && zombie.jumpCooldown <= 0.0F) {
        const int aheadX = static_cast<int>(std::floor(zombie.position.x + direction * (entities::kZombieHalfWidth + 0.1F)));
        const int footY = static_cast<int>(std::floor(zombie.position.y));
        bool obstacle = false;
        for (int y = footY - 1; y <= footY; ++y) {
            if (isSolidTile(aheadX, y)) {
                obstacle = true;
                break;
            }
        }
        if (obstacle) {
            zombie.velocity.y = -kZombieJumpVelocity;
            zombie.onGround = false;
            zombie.jumpCooldown = 0.45F;
        }
    }

    entities::Vec2 position = zombie.position;
    position.x += zombie.velocity.x * dt;
    resolveHorizontalAabb(position, zombie.velocity, entities::kZombieHalfWidth, entities::kZombieHeight);

    position.y += zombie.velocity.y * dt;
    resolveVerticalAabb(position, zombie.velocity, entities::kZombieHalfWidth, entities::kZombieHeight, zombie.onGround);

    const float maxX = static_cast<float>(world_.width() - 1);
    const float maxY = static_cast<float>(world_.height() - 1);
    position.x = std::clamp(position.x, 0.0F, maxX);
    position.y = std::clamp(position.y, 0.0F, maxY);
    zombie.position = position;
}

void Game::spawnZombie() {
    if (world_.width() <= 2) {
        return;
    }

    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> offsetDist(20, 60);
    const float direction = (sideDist(zombieRng_) == 0) ? -1.0F : 1.0F;
    float spawnX = player_.position().x + direction * static_cast<float>(offsetDist(zombieRng_));
    spawnX = std::clamp(spawnX, 1.0F, static_cast<float>(world_.width() - 2));
    const int column = static_cast<int>(spawnX);

    int ground = -1;
    for (int y = 0; y < world_.height(); ++y) {
        if (isSolidTile(column, y)) {
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

    if (collidesAabb(zombie.position, entities::kZombieHalfWidth, entities::kZombieHeight)) {
        return;
    }

    zombies_.push_back(std::make_unique<entities::Zombie>(zombie));
}

bool Game::attackZombiesAtCursor() {
    int tileX = 0;
    int tileY = 0;
    if (!cursorWorldTile(tileX, tileY)) {
        return false;
    }

    for (auto& zombiePtr : zombies_) {
        if (!zombiePtr) {
            continue;
        }
        if (!zombiePtr->alive()) {
            continue;
        }
        if (cursorHitsZombie(*zombiePtr, tileX, tileY)) {
            zombiePtr->takeDamage(kPlayerAttackDamage);
            return true;
        }
    }
    return false;
}

bool Game::zombiesOverlapPlayer(const entities::Zombie& zombie) const {
    return aabbOverlap(zombie.position,
                       entities::kZombieHalfWidth,
                       entities::kZombieHeight,
                       player_.position(),
                       entities::kPlayerHalfWidth,
                       entities::kPlayerHeight);
}

bool Game::cursorHitsZombie(const entities::Zombie& zombie, int tileX, int tileY) const {
    const entities::Vec2 cursorPos{static_cast<float>(tileX) + 0.5F, static_cast<float>(tileY + 1)};
    return aabbOverlap(cursorPos, 0.5F, 1.0F, zombie.position, entities::kZombieHalfWidth, entities::kZombieHeight);
}

bool Game::aabbOverlap(const entities::Vec2& aPos,
                       float aHalfWidth,
                       float aHeight,
                       const entities::Vec2& bPos,
                       float bHalfWidth,
                       float bHeight) const {
    const float aLeft = aPos.x - aHalfWidth;
    const float aRight = aPos.x + aHalfWidth;
    const float aTop = aPos.y - aHeight;
    const float aBottom = aPos.y;

    const float bLeft = bPos.x - bHalfWidth;
    const float bRight = bPos.x + bHalfWidth;
    const float bTop = bPos.y - bHeight;
    const float bBottom = bPos.y;

    return aRight > bLeft && aLeft < bRight && aBottom > bTop && aTop < bBottom;
}

void Game::updateDayNight(float dt) {
    if (dayLength_ <= 0.001F) {
        isNight_ = false;
        return;
    }
    timeOfDay_ += dt;
    while (timeOfDay_ >= dayLength_) {
        timeOfDay_ -= dayLength_;
    }

    const float normalized = normalizedTimeOfDay();
    isNight_ = normalized >= kNightStart && normalized < kNightEnd;
}

float Game::normalizedTimeOfDay() const {
    if (dayLength_ <= 0.001F) {
        return 0.0F;
    }
    return std::clamp(timeOfDay_ / dayLength_, 0.0F, 1.0F);
}

void Game::updateHudState() {
    hudState_.selectedSlot = selectedHotbar_;
    hudState_.hotbarCount = static_cast<int>(hotbarTypes_.size());
    for (int i = 0; i < hudState_.hotbarCount; ++i) {
        hudState_.hotbarTypes[static_cast<std::size_t>(i)] = hotbarTypes_[static_cast<std::size_t>(i)];
    }
    if (breakState_.active) {
        hudState_.breakActive = true;
        hudState_.breakTileX = breakState_.tileX;
        hudState_.breakTileY = breakState_.tileY;
        hudState_.breakProgress = std::clamp(breakState_.progress, 0.0F, 1.0F);
    } else {
        hudState_.breakActive = false;
        hudState_.breakTileX = 0;
        hudState_.breakTileY = 0;
        hudState_.breakProgress = 0.0F;
    }

    int tileX = 0;
    int tileY = 0;
    if (cursorWorldTile(tileX, tileY)) {
        const bool inRange = withinPlacementRange(tileX, tileY);
        hudState_.cursorInRange = inRange;
        hudState_.cursorHighlight = inRange;
        hudState_.cursorTileX = tileX;
        hudState_.cursorTileY = tileY;
        hudState_.cursorCanPlace = inRange && canPlaceTile(tileX, tileY);
    } else {
        hudState_.cursorHighlight = false;
        hudState_.cursorInRange = false;
        hudState_.cursorCanPlace = false;
        hudState_.cursorTileX = 0;
        hudState_.cursorTileY = 0;
    }
    hudState_.useCamera = cameraMode_;
    hudState_.cameraX = cameraPosition_.x;
    hudState_.cameraY = cameraPosition_.y;
    hudState_.playerHealth = player_.health();
    hudState_.playerMaxHealth = entities::kPlayerMaxHealth;
    hudState_.zombieCount = static_cast<int>(zombies_.size());
    hudState_.dayProgress = normalizedTimeOfDay();
    hudState_.isNight = isNight_;
}

void Game::toggleCameraMode() {
    cameraMode_ = !cameraMode_;
    if (cameraMode_) {
        cameraPosition_ = clampCameraTarget(player_.position());
    }
}

entities::Vec2 Game::cameraFocus() const {
    const entities::Vec2 source = cameraMode_ ? cameraPosition_ : player_.position();
    return clampCameraTarget(source);
}

entities::Vec2 Game::clampCameraTarget(const entities::Vec2& desired) const {
    entities::Vec2 result = desired;
    const float viewWidth = static_cast<float>(config_.windowWidth) / kTilePixels + 2.0F;
    const float viewHeight = static_cast<float>(config_.windowHeight) / kTilePixels + 2.0F;
    const float halfWidth = viewWidth * 0.5F;
    const float halfHeight = viewHeight * 0.5F;
    const float worldWidth = static_cast<float>(world_.width());
    const float worldHeight = static_cast<float>(world_.height());

    if (worldWidth <= viewWidth) {
        result.x = worldWidth * 0.5F;
    } else {
        result.x = std::clamp(result.x, halfWidth, worldWidth - halfWidth);
    }

    if (worldHeight <= viewHeight) {
        result.y = worldHeight * 0.5F;
    } else {
        result.y = std::clamp(result.y, halfHeight, worldHeight - halfHeight);
    }

    return result;
}

bool Game::isTreeTile(world::TileType type) {
    return type == world::TileType::TreeTrunk || type == world::TileType::TreeLeaves;
}

} // namespace terraria::game
