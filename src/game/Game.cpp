#include "terraria/game/Game.h"

#include <algorithm>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace terraria::game {

namespace {
constexpr float kGravity = 60.0F;
constexpr float kJumpVelocity = 25.0F;
constexpr float kCollisionEpsilon = 0.001F;
constexpr float kTilePixels = 8.0F;
constexpr float kBreakRange = 6.0F;
constexpr float kPlaceRange = 6.0F;

} // namespace

Game::Game(const core::AppConfig& config)
    : config_{config},
      world_{config.worldWidth, config.worldHeight},
      generator_{},
      player_{},
      renderer_{rendering::CreateSdlRenderer(config_)},
      inputSystem_{input::CreateSdlInputSystem()} {}

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
    if (cameraMode_) {
        return;
    }

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

void Game::render() {
    renderer_->render(world_, player_, hudState_);
}

bool Game::collides(const entities::Vec2& pos) const {
    const float left = pos.x - entities::kPlayerHalfWidth;
    const float right = pos.x + entities::kPlayerHalfWidth;
    const float top = pos.y - entities::kPlayerHeight;
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
    if (!collides(position)) {
        return;
    }
    if (velocity.x > 0.0F) {
        const int tileX = static_cast<int>(std::floor(position.x + entities::kPlayerHalfWidth));
        position.x = static_cast<float>(tileX) - entities::kPlayerHalfWidth - kCollisionEpsilon;
    } else if (velocity.x < 0.0F) {
        const int tileX = static_cast<int>(std::floor(position.x - entities::kPlayerHalfWidth));
        position.x = static_cast<float>(tileX + 1) + entities::kPlayerHalfWidth + kCollisionEpsilon;
    } else {
        // resolve by nudging to the closest free space
        if (collides(position)) {
            position.x = std::round(position.x);
        }
    }
    velocity.x = 0.0F;
}

void Game::resolveVertical(entities::Vec2& position, entities::Vec2& velocity) {
    if (!collides(position)) {
        player_.setOnGround(false);
        return;
    }

    if (velocity.y > 0.0F) {
        const int tileY = static_cast<int>(std::floor(position.y));
        position.y = static_cast<float>(tileY) - kCollisionEpsilon;
        player_.setOnGround(true);
    } else if (velocity.y < 0.0F) {
        const int tileY = static_cast<int>(std::floor(position.y - entities::kPlayerHeight));
        position.y = static_cast<float>(tileY + 1) + entities::kPlayerHeight + kCollisionEpsilon;
    } else {
        const int tileY = static_cast<int>(std::floor(position.y));
        position.y = static_cast<float>(tileY) - kCollisionEpsilon;
        player_.setOnGround(true);
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
    if (!state.breakHeld) {
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
