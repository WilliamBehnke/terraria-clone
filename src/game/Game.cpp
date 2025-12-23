#include "terraria/game/Game.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <sstream>

namespace terraria::game {

namespace {
constexpr float kGravity = 60.0F;
constexpr float kJumpVelocity = 25.0F;
constexpr float kMoveSpeed = 14.0F;
constexpr float kGroundAcceleration = 90.0F;
constexpr float kAirAcceleration = 40.0F;
constexpr float kGroundDrag = 80.0F;
constexpr float kJumpBufferTime = 0.18F;
constexpr float kCoyoteTimeWindow = 0.12F;
constexpr float kJumpReleaseGravityBoost = 80.0F;
constexpr float kTilePixels = 16.0F;
constexpr float kBreakRange = 6.0F;
constexpr float kPlaceRange = 6.0F;
constexpr float kPlayerAttackCooldown = 0.35F;
constexpr float kBowDrawTime = 0.4F;
constexpr float kDayLengthSeconds = 180.0F;
constexpr float kNightStart = 0.55F;
constexpr float kNightEnd = 0.95F;
constexpr float kPerfSmoothing = 0.1F;
constexpr float kPi = 3.1415926535F;
constexpr std::uint32_t kSeedSalt = 0x9E3779B9U;

entities::ToolTier RequiredPickaxeTier(world::TileType type) {
    using entities::ToolTier;
    switch (type) {
    case world::TileType::Stone: return ToolTier::Wood;
    case world::TileType::CopperOre: return ToolTier::Stone;
    case world::TileType::IronOre: return ToolTier::Copper;
    case world::TileType::GoldOre: return ToolTier::Iron;
    default: return ToolTier::None;
    }
}

bool RequiredToolKind(world::TileType type, entities::ToolKind& outKind) {
    switch (type) {
    case world::TileType::Dirt:
    case world::TileType::Grass:
        outKind = entities::ToolKind::Shovel;
        return true;
    case world::TileType::Stone:
    case world::TileType::StoneBrick:
    case world::TileType::CopperOre:
    case world::TileType::IronOre:
    case world::TileType::GoldOre:
        outKind = entities::ToolKind::Pickaxe;
        return true;
    case world::TileType::Wood:
    case world::TileType::WoodPlank:
    case world::TileType::TreeTrunk:
        outKind = entities::ToolKind::Axe;
        return true;
    case world::TileType::Leaves:
    case world::TileType::TreeLeaves:
        outKind = entities::ToolKind::Hoe;
        return true;
    default:
        return false;
    }
}

float PickaxeSpeedBonus(entities::ToolTier tier) {
    const int value = entities::ToolTierValue(tier);
    if (value <= 0) {
        return 1.0F;
    }
    return 1.0F + 0.2F * static_cast<float>(value);
}

std::uint32_t generateSeed() {
    const auto now = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    return static_cast<std::uint32_t>((now ^ (now >> 32)) + kSeedSalt);
}

void applyDefaultLoadout(entities::Player& player) {
    player.resetHealth();
    player.addTool(entities::ToolKind::Pickaxe, entities::ToolTier::Wood);
    player.addTool(entities::ToolKind::Axe, entities::ToolTier::Wood);
    player.addTool(entities::ToolKind::Shovel, entities::ToolTier::Wood);
    player.addTool(entities::ToolKind::Hoe, entities::ToolTier::Wood);
    player.addTool(entities::ToolKind::Sword, entities::ToolTier::Wood);
    player.addTool(entities::ToolKind::Bow, entities::ToolTier::Wood);
}

} // namespace

Game::Game(const core::AppConfig& config)
    : config_{config},
      world_{config.worldWidth, config.worldHeight},
      generator_{},
      player_{},
      physics_{world_},
      enemyManager_{config_, world_, player_, physics_, damageNumbers_},
      combatSystem_{world_,
                    player_,
                    physics_,
                    damageNumbers_,
                    enemyManager_.zombies(),
                    enemyManager_.flyers(),
                    enemyManager_.worms(),
                    enemyManager_.dragon(),
                    enemyManager_},
      renderer_{rendering::CreateSdlRenderer(config_)},
      inputSystem_{input::CreateSdlInputSystem()},
      inventorySystem_{config_, player_, *inputSystem_},
      craftingSystem_{config_, player_, *inputSystem_},
      dayLength_{kDayLengthSeconds} {}

void Game::initialize() {
    renderer_->initialize();
    inputSystem_->initialize();
    loadOrCreateSaves();
    timeOfDay_ = 0.0F;
    isNight_ = false;
    cameraPosition_ = clampCameraTarget({static_cast<float>(world_.width()) * 0.5F,
                                         static_cast<float>(world_.height()) * 0.5F});
}

bool Game::tick() {
    const auto frameStart = std::chrono::steady_clock::now();
    const float dt = 1.0F / static_cast<float>(config_.targetFps);
    handleInput();
    const auto updateStart = std::chrono::steady_clock::now();
    update(dt);
    processActions(dt);
    const auto updateEnd = std::chrono::steady_clock::now();
    updateHudState();
    const auto renderStart = std::chrono::steady_clock::now();
    render();
    const auto frameEnd = std::chrono::steady_clock::now();

    const float frameMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
    const float updateMs = std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();
    const float renderMs = std::chrono::duration<float, std::milli>(frameEnd - renderStart).count();
    recordPerformanceMetrics(frameMs, updateMs, renderMs);
    return !inputSystem_->shouldQuit() && !requestQuit_;
}

void Game::shutdown() {
    saveActiveSession();
    inputSystem_->shutdown();
    renderer_->shutdown();
}

void Game::handleInput() {
    inputSystem_->poll();
    const auto& inputState = inputSystem_->state();
    if (menuSystem_.isGameplay() && inputState.menuBack) {
        if (chatConsole_.isOpen()) {
            chatConsole_.close();
        }
        menuSystem_.openPause();
        paused_ = true;
        return;
    }
    if (!menuSystem_.isGameplay()) {
        MenuSystem::MenuAction action{};
        menuSystem_.handleInput(inputState, MenuSystem::MenuData{&characterList_, &worldList_}, action);
        if (action.type == MenuSystem::MenuAction::Type::None) {
            return;
        }
        if (action.type == MenuSystem::MenuAction::Type::Resume) {
            paused_ = false;
            return;
        }
        if (action.type == MenuSystem::MenuAction::Type::Save) {
            saveActiveSession();
            chatConsole_.addMessage("GAME SAVED", true);
            menuSystem_.resumeGameplay();
            paused_ = false;
            return;
        }
        if (action.type == MenuSystem::MenuAction::Type::SaveExit) {
            saveActiveSession();
            requestQuit_ = true;
            return;
        }
        if (action.type == MenuSystem::MenuAction::Type::SaveTitle) {
            saveActiveSession();
            clearActiveSession();
            loadOrCreateSaves();
            menuSystem_.setScreen(Screen::CharacterSelect);
            paused_ = true;
            return;
        }
        if (action.type == MenuSystem::MenuAction::Type::CreateCharacter) {
            entities::Player temp;
            applyDefaultLoadout(temp);
            temp.setPosition({0.0F, 0.0F});
            const std::string charId = saveManager_.createCharacterId(characterList_);
            saveManager_.saveCharacter(charId, action.name, temp);
            characterList_ = saveManager_.listCharacters();
            for (std::size_t i = 0; i < characterList_.size(); ++i) {
                if (characterList_[i].id == charId) {
                    menuSystem_.setCharacterSelection(static_cast<int>(i));
                    break;
                }
            }
            return;
        }
        if (action.type == MenuSystem::MenuAction::Type::CreateWorld) {
            const std::uint32_t seed = action.seed.empty()
                ? generateSeed()
                : static_cast<std::uint32_t>(std::strtoul(action.seed.c_str(), nullptr, 10));
            world_ = world::World(config_.worldWidth, config_.worldHeight);
            generator_.generate(world_, seed, action.genConfig);
            const entities::Vec2 spawn = findSpawnPosition();
            const std::string worldId = saveManager_.createWorldId(worldList_);
            const float defaultTime = 0.0F;
            saveManager_.saveWorld(worldId, action.name, world_, seed, spawn.x, spawn.y, defaultTime, false);
            worldList_ = saveManager_.listWorlds();
            for (std::size_t i = 0; i < worldList_.size(); ++i) {
                if (worldList_[i].id == worldId) {
                    menuSystem_.setWorldSelection(static_cast<int>(i));
                    break;
                }
            }
            return;
        }
        if (action.type == MenuSystem::MenuAction::Type::StartSession) {
            if (action.characterIndex >= 0 && action.characterIndex < static_cast<int>(characterList_.size())
                && action.worldIndex >= 0 && action.worldIndex < static_cast<int>(worldList_.size())) {
                startSession(worldList_[static_cast<std::size_t>(action.worldIndex)],
                             characterList_[static_cast<std::size_t>(action.characterIndex)]);
            }
            return;
        }
        return;
    }
    if (inputState.consoleToggle) {
        if (chatConsole_.isOpen()) {
            if (!inputState.consoleSlash) {
                chatConsole_.close();
            }
        } else {
            if (inventorySystem_.isOpen()) {
                if (!inventorySystem_.placeCarriedStack()) {
                    return;
                }
                inventorySystem_.setOpen(false);
            }
            chatConsole_.toggle();
        }
    }
    if (chatConsole_.isOpen()) {
        chatConsole_.handleInput(
            inputState,
            [this](const std::string& text) { executeConsoleCommand(text); },
            [this](const std::string& text) { chatConsole_.addMessage(text, false); });
        paused_ = true;
        return;
    }
    if (inputState.inventoryToggle) {
        if (cameraMode_) {
            toggleCameraMode();
        }
        if (inventorySystem_.isOpen()) {
            if (inventorySystem_.placeCarriedStack()) {
                inventorySystem_.setOpen(false);
            }
        } else {
            inventorySystem_.setOpen(true);
        }
    }

    if (inputState.toggleCamera) {
        if (cameraMode_) {
            toggleCameraMode();
        } else {
            if (inventorySystem_.isOpen()) {
                if (inventorySystem_.placeCarriedStack()) {
                    inventorySystem_.setOpen(false);
                }
            }
            if (!inventorySystem_.isOpen()) {
                toggleCameraMode();
            }
        }
    }

    const bool inventoryOpen = inventorySystem_.isOpen();
    paused_ = inventoryOpen || cameraMode_ || chatConsole_.isOpen();
    if (cameraMode_) {
        const float moveSpeed = cameraSpeed_;
        const float delta = moveSpeed / static_cast<float>(config_.targetFps);
        cameraPosition_.x += inputState.camMoveX * delta;
        cameraPosition_.y += inputState.camMoveY * delta;
        cameraPosition_ = clampCameraTarget(cameraPosition_);
        player_.setVelocity({0.0F, 0.0F});
        moveInput_ = 0.0F;
        jumpBufferTimer_ = 0.0F;
        jumpHeld_ = false;
        prevJumpInput_ = inputState.jump;
    } else if (!inventoryOpen) {
        moveInput_ = std::clamp(inputState.moveX, -1.0F, 1.0F);
        if (inputState.jump && !prevJumpInput_) {
            jumpBufferTimer_ = kJumpBufferTime;
        }
        jumpHeld_ = inputState.jump;
        prevJumpInput_ = inputState.jump;
    } else {
        entities::Vec2 velocity = player_.velocity();
        velocity.x = 0.0F;
        player_.setVelocity(velocity);
        moveInput_ = 0.0F;
        if (!inputState.jump) {
            jumpHeld_ = false;
        }
        jumpBufferTimer_ = 0.0F;
        prevJumpInput_ = inputState.jump;
    }

    const int selection = inputState.hotbarSelection;
    if (selection >= 0 && selection < entities::kHotbarSlots) {
        selectedHotbar_ = selection;
    }
    selectedHotbar_ = std::clamp(selectedHotbar_, 0, entities::kHotbarSlots - 1);

    craftingSystem_.handleInput(inventoryOpen);
}

void Game::saveActiveSession() {
    if (activeWorldId_.empty() || activeCharacterId_.empty()) {
        return;
    }
    saveManager_.saveWorld(activeWorldId_,
                           activeWorldName_,
                           world_,
                           worldSeed_,
                           worldSpawn_.x,
                           worldSpawn_.y,
                           timeOfDay_,
                           isNight_);
    saveManager_.saveCharacter(activeCharacterId_, activeCharacterName_, player_);
}

void Game::clearActiveSession() {
    world_ = world::World(config_.worldWidth, config_.worldHeight);
    player_ = entities::Player{};
    enemyManager_.reset();
    enemyManager_.setDragonDen({}, 0.0F, 0.0F);
    combatSystem_.reset();
    damageNumbers_.reset();
    breakState_ = {};
    placeCooldown_ = 0.0F;
    playerAttackCooldown_ = 0.0F;
    bowDrawTimer_ = 0.0F;
    moveInput_ = 0.0F;
    jumpBufferTimer_ = 0.0F;
    coyoteTimer_ = 0.0F;
    jumpHeld_ = false;
    prevJumpInput_ = false;
    prevBreakHeld_ = false;
    selectedHotbar_ = 0;
    paused_ = false;
    cameraMode_ = false;
    cameraPosition_ = clampCameraTarget({static_cast<float>(world_.width()) * 0.5F,
                                         static_cast<float>(world_.height()) * 0.5F});
    timeOfDay_ = 0.0F;
    isNight_ = false;
    worldSeed_ = 0;
    worldSpawn_ = {};
    activeWorldId_.clear();
    activeWorldName_.clear();
    activeCharacterId_.clear();
    activeCharacterName_.clear();
    inventorySystem_.setOpen(false);
    chatConsole_.close();
}

void Game::update(float dt) {
    chatConsole_.update(dt);
    if (!menuSystem_.isGameplay()) {
        return;
    }
    if (paused_) {
        return;
    }
    if (!cameraMode_) {
        jumpBufferTimer_ = std::max(0.0F, jumpBufferTimer_ - dt);
        if (player_.onGround()) {
            coyoteTimer_ = kCoyoteTimeWindow;
        } else {
            coyoteTimer_ = std::max(0.0F, coyoteTimer_ - dt);
        }

        entities::Vec2 velocity = player_.velocity();
        const float targetSpeed = moveInput_ * kMoveSpeed * player_.moveSpeedMultiplier();
        const float accel = player_.onGround() ? kGroundAcceleration : kAirAcceleration;
        const float delta = targetSpeed - velocity.x;
        const float maxStep = accel * dt;
        if (std::fabs(delta) <= maxStep) {
            velocity.x = targetSpeed;
        } else {
            velocity.x += std::copysign(maxStep, delta);
        }
        if (std::fabs(moveInput_) < 0.01F && player_.onGround()) {
            const float drag = kGroundDrag * dt;
            if (std::fabs(velocity.x) <= drag) {
                velocity.x = 0.0F;
            } else {
                velocity.x -= std::copysign(drag, velocity.x);
            }
        }

        velocity.y += kGravity * dt;

        const float jumpModifier = 1.0F + player_.jumpBonus();
        if (jumpBufferTimer_ > 0.0F && coyoteTimer_ > 0.0F) {
            velocity.y = -kJumpVelocity * jumpModifier;
            player_.setOnGround(false);
            jumpBufferTimer_ = 0.0F;
            coyoteTimer_ = 0.0F;
        } else if (!jumpHeld_ && velocity.y < 0.0F) {
            velocity.y += kJumpReleaseGravityBoost * jumpModifier * dt;
        }

        entities::Vec2 position = player_.position();
        position.x += velocity.x * dt;
        physics_.resolveHorizontalAabb(position, velocity, entities::kPlayerHalfWidth, entities::kPlayerHeight);

        position.y += velocity.y * dt;
        bool grounded = player_.onGround();
        physics_.resolveVerticalAabb(position, velocity, entities::kPlayerHalfWidth, entities::kPlayerHeight, grounded);
        player_.setOnGround(grounded);

        const float maxX = static_cast<float>(world_.width() - 1);
        const float maxY = static_cast<float>(world_.height() - 1);
        position.x = std::clamp(position.x, 0.0F, maxX);
        position.y = std::clamp(position.y, 0.0F, maxY);

        player_.setPosition(position);
        player_.setVelocity(velocity);
        cameraPosition_ = clampCameraTarget(position);
    }

    updateDayNight(dt);
    enemyManager_.update(dt, isNight_, cameraFocus());
    combatSystem_.update(dt);
    damageNumbers_.update(dt);
    if (player_.health() <= 0) {
        player_.resetHealth();
        player_.setPosition(spawnPosition_);
        player_.setVelocity({0.0F, 0.0F});
        player_.setOnGround(false);
    }
}

void Game::render() {
    renderer_->render(world_, player_, enemyManager_.zombies(), hudState_);
}

void Game::processActions(float dt) {
    if (chatConsole_.isOpen()) {
        return;
    }
    if (!menuSystem_.isGameplay()) {
        return;
    }
    const bool equipmentHandled = inventorySystem_.handleInput();
    if (!paused_ && !inventorySystem_.isOpen()) {
        handleBreaking(dt);
        handlePlacement(dt);
    } else {
        breakState_ = {};
    }
    craftingSystem_.handlePointerInput(inventorySystem_.isOpen(), equipmentHandled);
    craftingSystem_.update(dt);
}

void Game::loadOrCreateSaves() {
    saveManager_.ensureDirectories();
    characterList_ = saveManager_.listCharacters();
    worldList_ = saveManager_.listWorlds();

    if (worldList_.empty()) {
        world_ = world::World(config_.worldWidth, config_.worldHeight);
    }

    if (characterList_.empty()) {
        player_ = entities::Player{};
    }

    activeCharacterId_.clear();
    activeCharacterName_.clear();
    activeWorldId_.clear();
    activeWorldName_.clear();
    menuSystem_.setCharacterSelection(0);
    menuSystem_.setWorldSelection(0);
    menuSystem_.setScreen(Screen::CharacterSelect);
    inventorySystem_.setOpen(false);
    chatConsole_.close();
}

void Game::startSession(const WorldInfo& worldInfo, const CharacterInfo& characterInfo) {
    activeWorldId_ = worldInfo.id;
    activeWorldName_ = worldInfo.name;
    activeCharacterId_ = characterInfo.id;
    activeCharacterName_ = characterInfo.name;

    std::string loadedWorldName{};
    float loadedTime = 0.0F;
    bool loadedNight = false;
    std::uint32_t loadedSeed = 0;
    float loadedSpawnX = 0.0F;
    float loadedSpawnY = 0.0F;
    if (!saveManager_.loadWorld(activeWorldId_, world_, loadedWorldName, loadedSeed, loadedSpawnX, loadedSpawnY, loadedTime, loadedNight)) {
        world_ = world::World(config_.worldWidth, config_.worldHeight);
        loadedSeed = (worldInfo.seed != 0) ? worldInfo.seed : generateSeed();
        generator_.generate(world_, loadedSeed);
        loadedWorldName = activeWorldName_;
        const entities::Vec2 spawn = findSpawnPosition();
        loadedSpawnX = spawn.x;
        loadedSpawnY = spawn.y;
        loadedTime = 0.0F;
        loadedNight = false;
        saveManager_.saveWorld(activeWorldId_,
                               loadedWorldName,
                               world_,
                               loadedSeed,
                               loadedSpawnX,
                               loadedSpawnY,
                               loadedTime,
                               loadedNight);
    }
    activeWorldName_ = loadedWorldName;
    timeOfDay_ = loadedTime;
    isNight_ = loadedNight;
    worldSeed_ = loadedSeed;
    worldSpawn_ = {loadedSpawnX, loadedSpawnY};
    if (worldSpawn_.y <= 0.5F) {
        worldSpawn_ = findSpawnPosition();
    }

    std::string loadedCharName{};
    if (!saveManager_.loadCharacter(activeCharacterId_, player_, loadedCharName)) {
        player_ = entities::Player{};
        applyDefaultLoadout(player_);
        loadedCharName = activeCharacterName_;
    }
    activeCharacterName_ = loadedCharName;

    entities::Vec2 desired = worldSpawn_;
    entities::Vec2 spawnSafe = worldSpawn_;
    if (!findNearestOpenSpot(worldSpawn_, spawnSafe)) {
        spawnSafe = findSpawnPosition();
        findNearestOpenSpot(spawnSafe, spawnSafe);
    }
    worldSpawn_ = spawnSafe;
    entities::Vec2 safeSpot = desired;
    if (!findNearestOpenSpot(desired, safeSpot)) {
        safeSpot = spawnSafe;
    }
    player_.setPosition(safeSpot);
    player_.setVelocity({0.0F, 0.0F});
    player_.setOnGround(false);
    spawnPosition_ = spawnSafe;
    cameraPosition_ = clampCameraTarget(safeSpot);

    enemyManager_.reset();
    const auto denInfo = generator_.dragonDenInfo(world_, worldSeed_);
    const bool denOpen = denInfo.radiusX > 0 && denInfo.radiusY > 0
        && denInfo.centerX >= 0 && denInfo.centerX < world_.width()
        && denInfo.centerY >= 0 && denInfo.centerY < world_.height()
        && !world_.tile(denInfo.centerX, denInfo.centerY).isSolid();
    if (denOpen) {
        const entities::Vec2 denCenter{static_cast<float>(denInfo.centerX) + 0.5F,
                                       static_cast<float>(denInfo.centerY)};
        enemyManager_.setDragonDen(denCenter,
                                   static_cast<float>(denInfo.radiusX),
                                   static_cast<float>(denInfo.radiusY));
    } else {
        enemyManager_.setDragonDen({}, 0.0F, 0.0F);
    }
    combatSystem_.reset();
    damageNumbers_.reset();
    breakState_ = {};
    placeCooldown_ = 0.0F;
    playerAttackCooldown_ = 0.0F;
    bowDrawTimer_ = 0.0F;
    moveInput_ = 0.0F;
    jumpBufferTimer_ = 0.0F;
    coyoteTimer_ = 0.0F;
    jumpHeld_ = false;
    prevJumpInput_ = false;
    prevBreakHeld_ = false;
    selectedHotbar_ = 0;
    paused_ = false;
    cameraMode_ = false;
    inventorySystem_.setOpen(false);
    chatConsole_.close();
    menuSystem_.setScreen(Screen::Gameplay);
}

entities::Vec2 Game::findSpawnPosition() const {
    const int spawnX = world_.width() / 2;
    int spawnY = 0;
    for (int y = 0; y < world_.height(); ++y) {
        if (world_.tile(spawnX, y).active()) {
            spawnY = (y > 0) ? y - 1 : 0;
            break;
        }
    }
    return {static_cast<float>(spawnX) + 0.5F, static_cast<float>(spawnY)};
}

bool Game::findNearestOpenSpot(const entities::Vec2& desired, entities::Vec2& outPos) const {
    const float clampedX = std::clamp(desired.x, 0.0F, static_cast<float>(world_.width() - 1));
    const float clampedY = std::clamp(desired.y, 0.0F, static_cast<float>(world_.height() - 1));
    const int baseX = static_cast<int>(std::round(clampedX));
    const int baseY = static_cast<int>(std::round(clampedY));
    outPos = {clampedX, clampedY};
    constexpr int kSearchRadius = 12;
    for (int r = 0; r <= kSearchRadius; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) {
                    continue;
                }
                const int tx = std::clamp(baseX + dx, 0, world_.width() - 1);
                const int ty = std::clamp(baseY + dy, 0, world_.height() - 1);
                if (world_.tile(tx, ty).active() && world_.tile(tx, ty).isSolid()) {
                    continue;
                }
                entities::Vec2 candidate{static_cast<float>(tx) + 0.5F, static_cast<float>(ty)};
                if (physics_.collidesAabb(candidate, entities::kPlayerHalfWidth, entities::kPlayerHeight)) {
                    continue;
                }
                outPos = candidate;
                return true;
            }
        }
    }
    return false;
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

bool Game::cursorWorldPosition(entities::Vec2& outPos) const {
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
    outPos = {worldX, worldY};
    return true;
}

bool Game::canBreakTile(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= world_.width() || tileY < 0 || tileY >= world_.height()) {
        return false;
    }
    const auto& tile = world_.tile(tileX, tileY);
    if (!tile.active() || tile.type() == world::TileType::Air) {
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
    return !tile.active() || tile.type() == world::TileType::Air;
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
    if (inventorySystem_.isOpen()) {
        breakState_ = {};
        bowDrawTimer_ = 0.0F;
        prevBreakHeld_ = false;
        return;
    }
    const auto& state = inputSystem_->state();
    playerAttackCooldown_ = std::max(0.0F, playerAttackCooldown_ - dt);

    const entities::InventorySlot* activeSlot = nullptr;
    if (selectedHotbar_ >= 0 && selectedHotbar_ < entities::kHotbarSlots) {
        activeSlot = &player_.hotbar()[static_cast<std::size_t>(selectedHotbar_)];
    }

    if (activeSlot && activeSlot->isTool()) {
        if (activeSlot->toolKind == entities::ToolKind::Sword) {
            breakState_ = {};
            if (state.breakHeld && playerAttackCooldown_ <= 0.0F) {
                entities::Vec2 pivot{player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
                entities::Vec2 cursorPos{};
                float dx = 0.0F;
                float dy = 0.0F;
                if (cursorWorldPosition(cursorPos)) {
                    dx = cursorPos.x - pivot.x;
                    dy = cursorPos.y - pivot.y;
                } else {
                    dx = player_.velocity().x;
                    dy = player_.velocity().y;
                }
                const float absDx = std::abs(dx);
                const float absDy = std::abs(dy);
                float angleStart = 0.0F;
                float angleEnd = 0.0F;
                if (absDy > absDx) {
                    if (dy < 0.0F) {
                        angleStart = -kPi * 0.25F;
                        angleEnd = -kPi * 0.75F;
                    } else {
                        angleStart = kPi * 0.25F;
                        angleEnd = kPi * 0.75F;
                    }
                } else {
                    const bool aimLeft = dx < 0.0F;
                    angleStart = aimLeft ? (-kPi * 0.75F) : (-kPi * 0.25F);
                    angleEnd = aimLeft ? (-kPi * 1.25F) : (kPi * 0.25F);
                }
                combatSystem_.startSwordSwing(angleStart, angleEnd, activeSlot->toolTier);
                playerAttackCooldown_ = kPlayerAttackCooldown;
            }
            prevBreakHeld_ = state.breakHeld;
            return;
        }
        if (activeSlot->toolKind == entities::ToolKind::Bow) {
            breakState_ = {};
            if (state.breakHeld) {
                if (player_.inventoryCount(world::TileType::Arrow) <= 0) {
                    bowDrawTimer_ = 0.0F;
                    prevBreakHeld_ = true;
                    return;
                }
                if (playerAttackCooldown_ <= 0.0F) {
                    bowDrawTimer_ = std::min(kBowDrawTime, bowDrawTimer_ + dt);
                } else {
                    bowDrawTimer_ = 0.0F;
                }
                prevBreakHeld_ = true;
                return;
            }
            if (prevBreakHeld_ && bowDrawTimer_ > 0.0F && playerAttackCooldown_ <= 0.0F) {
                entities::Vec2 direction{1.0F, 0.0F};
                entities::Vec2 cursorPos{};
                if (cursorWorldPosition(cursorPos)) {
                    entities::Vec2 origin{player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
                    entities::Vec2 delta{cursorPos.x - origin.x, cursorPos.y - origin.y};
                    const float len = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                    if (len > 0.001F) {
                        direction.x = delta.x / len;
                        direction.y = delta.y / len;
                    } else {
                        direction.x = (cursorPos.x >= origin.x) ? 1.0F : -1.0F;
                        direction.y = 0.0F;
                    }
                } else if (player_.velocity().x < 0.0F) {
                    direction.x = -1.0F;
                }
                if (!player_.consumeAmmo(world::TileType::Arrow, 1)) {
                    bowDrawTimer_ = 0.0F;
                    prevBreakHeld_ = false;
                    return;
                }
                const float drawRatio = std::clamp(bowDrawTimer_ / kBowDrawTime, 0.0F, 1.0F);
                const float damageScale = 0.6F + drawRatio * 0.9F;
                const float speedScale = 0.85F + drawRatio * 0.7F;
                const float gravityScale = 1.0F - drawRatio * 0.45F;
                combatSystem_.spawnProjectile(direction, activeSlot->toolTier, damageScale, speedScale, gravityScale);
                playerAttackCooldown_ = kPlayerAttackCooldown * 0.6F;
            }
            bowDrawTimer_ = 0.0F;
            prevBreakHeld_ = false;
            return;
        }
    }

    if (!state.breakHeld) {
        breakState_ = {};
        bowDrawTimer_ = 0.0F;
        prevBreakHeld_ = false;
        return;
    }
    prevBreakHeld_ = true;

    int tileX = 0;
    int tileY = 0;
    if (!cursorWorldTile(tileX, tileY) || !canBreakTile(tileX, tileY)) {
        breakState_ = {};
        prevBreakHeld_ = state.breakHeld;
        return;
    }

    if (!breakState_.active || breakState_.tileX != tileX || breakState_.tileY != tileY) {
        breakState_ = {true, tileX, tileY, 0.0F};
    }

    auto& target = world_.tile(tileX, tileY);
    const world::TileType targetType = target.type();
    if (!hasRequiredTool(targetType)) {
        breakState_ = {};
        return;
    }

    const float speed = breakSpeedMultiplier(targetType);
    breakState_.progress += dt * 2.5F * speed;
    if (breakState_.progress < 1.0F) {
        return;
    }

    const world::TileType dropType = target.dropType();
    if (dropType != world::TileType::Air) {
        player_.addToInventory(dropType);
    }
    world_.setTile(tileX, tileY, world::TileType::Air, false);
    breakState_ = {};
}

void Game::handlePlacement(float dt) {
    if (inventorySystem_.isOpen()) {
        placeCooldown_ = 0.0F;
        return;
    }
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

    const auto& slots = player_.hotbar();
    if (selectedHotbar_ < 0 || selectedHotbar_ >= entities::kHotbarSlots) {
        return;
    }
    const auto& slot = slots[static_cast<std::size_t>(selectedHotbar_)];
    if (!slot.isBlock()) {
        return;
    }
    if (slot.blockType == world::TileType::Arrow || slot.blockType == world::TileType::Coin) {
        return;
    }
    const world::TileType type = slot.blockType;
    world_.setTile(tileX, tileY, type, true);
    player_.consumeSlot(selectedHotbar_, 1);
    placeCooldown_ = 0.2F;
}

entities::ToolTier Game::selectedToolTier(entities::ToolKind kind) const {
    if (selectedHotbar_ < 0 || selectedHotbar_ >= entities::kHotbarSlots) {
        return entities::ToolTier::None;
    }
    const auto& slot = player_.hotbar()[static_cast<std::size_t>(selectedHotbar_)];
    if (slot.isTool() && slot.toolKind == kind) {
        return slot.toolTier;
    }
    return entities::ToolTier::None;
}

bool Game::hasRequiredTool(world::TileType tileType) const {
    entities::ToolKind requiredKind{};
    if (!RequiredToolKind(tileType, requiredKind)) {
        return true;
    }
    return canMineTileWithTool(tileType, requiredKind, selectedToolTier(requiredKind));
}

bool Game::canMineTileWithTool(world::TileType tileType, entities::ToolKind kind, entities::ToolTier tier) const {
    entities::ToolKind requiredKind{};
    if (!RequiredToolKind(tileType, requiredKind)) {
        return true;
    }
    if (kind != requiredKind) {
        return false;
    }
    if (requiredKind == entities::ToolKind::Pickaxe) {
        const entities::ToolTier requiredTier = RequiredPickaxeTier(tileType);
        if (requiredTier == entities::ToolTier::None) {
            return true;
        }
        return entities::ToolTierValue(tier) >= entities::ToolTierValue(requiredTier);
    }
    return entities::ToolTierValue(tier) > 0;
}

float Game::breakSpeedMultiplier(world::TileType tileType) const {
    entities::ToolKind requiredKind{};
    if (!RequiredToolKind(tileType, requiredKind)) {
        return 1.0F;
    }
    const entities::ToolTier tier = selectedToolTier(requiredKind);
    if (entities::ToolTierValue(tier) <= 0) {
        return 1.0F;
    }
    return PickaxeSpeedBonus(tier);
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

void Game::recordPerformanceMetrics(float frameMs, float updateMs, float renderMs) {
    const auto smooth = [](float current, float target) {
        if (current <= 0.0F) {
            return target;
        }
        return current + (target - current) * kPerfSmoothing;
    };
    perfFrameTimeMs_ = smooth(perfFrameTimeMs_, frameMs);
    perfUpdateTimeMs_ = smooth(perfUpdateTimeMs_, updateMs);
    perfRenderTimeMs_ = smooth(perfRenderTimeMs_, renderMs);
    perfFps_ = (perfFrameTimeMs_ > 0.0001F) ? (1000.0F / perfFrameTimeMs_) : static_cast<float>(config_.targetFps);
}

void Game::updateHudState() {
    const auto& inputState = inputSystem_->state();
    const auto fillSlot = [this](const entities::InventorySlot& src, rendering::HotbarSlotHud& dst) {
        dst = {};
        if (src.isTool()) {
            dst.isTool = true;
            dst.toolKind = src.toolKind;
            dst.toolTier = src.toolTier;
            if (src.toolKind == entities::ToolKind::Bow) {
                dst.count = player_.inventoryCount(world::TileType::Arrow);
            } else {
                dst.count = src.count;
            }
        } else if (src.isArmor()) {
            dst.isArmor = true;
            dst.armorId = src.armorId;
            dst.count = src.count;
        } else if (src.isAccessory()) {
            dst.isAccessory = true;
            dst.accessoryId = src.accessoryId;
            dst.count = src.count;
        } else if (src.isBlock()) {
            dst.isTool = false;
            dst.tileType = src.blockType;
            dst.count = src.count;
        }
    };

    hudState_.selectedSlot = selectedHotbar_;
    const auto hotbarSlots = player_.hotbar();
    hudState_.hotbarCount = std::min(rendering::kMaxHotbarSlots, entities::kHotbarSlots);
    for (int i = 0; i < hudState_.hotbarCount; ++i) {
        fillSlot(hotbarSlots[static_cast<std::size_t>(i)],
                 hudState_.hotbarSlots[static_cast<std::size_t>(i)]);
    }
    for (int i = hudState_.hotbarCount; i < rendering::kMaxHotbarSlots; ++i) {
        hudState_.hotbarSlots[static_cast<std::size_t>(i)] = {};
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
        hudState_.cursorTileX = tileX;
        hudState_.cursorTileY = tileY;
        const entities::InventorySlot* activeSlot = nullptr;
        if (selectedHotbar_ >= 0 && selectedHotbar_ < entities::kHotbarSlots) {
            activeSlot = &player_.hotbar()[static_cast<std::size_t>(selectedHotbar_)];
        }
        bool highlight = false;
        bool canInteract = false;
        if (activeSlot && activeSlot->isTool()) {
            if (activeSlot->toolKind != entities::ToolKind::Sword
                && activeSlot->toolKind != entities::ToolKind::Bow) {
                highlight = canBreakTile(tileX, tileY);
                if (highlight) {
                    const world::TileType targetType = world_.tile(tileX, tileY).type();
                    canInteract = canMineTileWithTool(targetType, activeSlot->toolKind, activeSlot->toolTier);
                }
            }
        } else if (activeSlot && activeSlot->isBlock()) {
            highlight = inRange;
            canInteract = highlight && canPlaceTile(tileX, tileY);
        }
        hudState_.cursorHighlight = highlight;
        hudState_.cursorCanPlace = canInteract;
    } else {
        hudState_.cursorHighlight = false;
        hudState_.cursorInRange = false;
        hudState_.cursorCanPlace = false;
        hudState_.cursorTileX = 0;
        hudState_.cursorTileY = 0;
    }
    const entities::Vec2 playerPos = player_.position();
    const entities::Vec2 coordPos = cameraMode_ ? cameraPosition_ : playerPos;
    hudState_.playerTileX = static_cast<int>(std::floor(coordPos.x));
    hudState_.playerTileY = static_cast<int>(std::floor(coordPos.y));
    hudState_.inventorySlotCount =
        std::min(rendering::kMaxInventorySlots, static_cast<int>(player_.inventory().size()));
    for (int i = 0; i < hudState_.inventorySlotCount; ++i) {
        fillSlot(player_.inventory()[static_cast<std::size_t>(i)],
                 hudState_.inventorySlots[static_cast<std::size_t>(i)]);
    }
    for (int i = hudState_.inventorySlotCount; i < rendering::kMaxInventorySlots; ++i) {
        hudState_.inventorySlots[static_cast<std::size_t>(i)] = {};
    }
    inventorySystem_.fillHud(hudState_);
    hudState_.useCamera = cameraMode_;
    hudState_.cameraX = cameraPosition_.x;
    hudState_.cameraY = cameraPosition_.y;
    hudState_.playerHealth = player_.health();
    hudState_.playerMaxHealth = player_.maxHealth();
    hudState_.playerDefense = player_.defense();
    const entities::Dragon* dragon = enemyManager_.dragon();
    const int dragonCount = (dragon && dragon->alive()) ? 1 : 0;
    hudState_.zombieCount = static_cast<int>(enemyManager_.zombies().size()
                                             + enemyManager_.flyers().size()
                                             + enemyManager_.worms().size()
                                             + dragonCount);
    hudState_.dayProgress = normalizedTimeOfDay();
    hudState_.isNight = isNight_;
    hudState_.bowDrawProgress = std::clamp(bowDrawTimer_ / kBowDrawTime, 0.0F, 1.0F);
    craftingSystem_.fillHud(hudState_);
    combatSystem_.fillHud(hudState_);
    enemyManager_.fillHud(hudState_);
    damageNumbers_.fillHud(hudState_);

    hudState_.perfFrameMs = perfFrameTimeMs_;
    hudState_.perfUpdateMs = perfUpdateTimeMs_;
    hudState_.perfRenderMs = perfRenderTimeMs_;
    hudState_.perfFps = perfFps_;
    hudState_.mouseX = std::clamp(inputState.mouseX, 0, config_.windowWidth);
    hudState_.mouseY = std::clamp(inputState.mouseY, 0, config_.windowHeight);
    menuSystem_.fillHud(hudState_, MenuSystem::MenuData{&characterList_, &worldList_});
    chatConsole_.fillHud(hudState_);
}

void Game::toggleCameraMode() {
    cameraMode_ = !cameraMode_;
    if (cameraMode_) {
        cameraPosition_ = clampCameraTarget(player_.position());
    }
}

void Game::executeConsoleCommand(const std::string& text) {
    std::string command = text;
    command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](unsigned char c) { return !std::isspace(c); }));
    const bool isCommand = !command.empty() && command.front() == '/';
    if (isCommand) {
        command.erase(command.begin());
    } else if (!command.empty()) {
        chatConsole_.addMessage(command, false);
        return;
    }
    if (command.empty()) {
        // Command responses are routed to chat.
        return;
    }
    std::istringstream stream(command);
    std::string verb;
    stream >> verb;
    if (verb == "give") {
        std::string itemId;
        int amount = 1;
        stream >> itemId;
        if (!stream.fail()) {
            stream >> amount;
            if (stream.fail()) {
                amount = 1;
            }
        }
        amount = std::max(1, amount);
        if (itemId.empty()) {
            chatConsole_.addMessage("USAGE: /give ITEM_ID [AMOUNT]", true);
            return;
        }
        auto normalize = [](std::string value) {
            for (char& c : value) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return value;
        };
        itemId = normalize(itemId);
        const auto giveBlock = [&](world::TileType type) {
            player_.addToInventory(type, amount);
            chatConsole_.addMessage("GAVE " + itemId, true);
        };
        if (itemId == "dirt") { giveBlock(world::TileType::Dirt); return; }
        if (itemId == "stone") { giveBlock(world::TileType::Stone); return; }
        if (itemId == "grass") { giveBlock(world::TileType::Grass); return; }
        if (itemId == "copper_ore") { giveBlock(world::TileType::CopperOre); return; }
        if (itemId == "iron_ore") { giveBlock(world::TileType::IronOre); return; }
        if (itemId == "gold_ore") { giveBlock(world::TileType::GoldOre); return; }
        if (itemId == "wood") { giveBlock(world::TileType::Wood); return; }
        if (itemId == "leaves") { giveBlock(world::TileType::Leaves); return; }
        if (itemId == "wood_plank") { giveBlock(world::TileType::WoodPlank); return; }
        if (itemId == "stone_brick") { giveBlock(world::TileType::StoneBrick); return; }
        if (itemId == "tree_trunk") { giveBlock(world::TileType::TreeTrunk); return; }
        if (itemId == "tree_leaves") { giveBlock(world::TileType::TreeLeaves); return; }
        if (itemId == "arrow") { giveBlock(world::TileType::Arrow); return; }
        if (itemId == "coin") { giveBlock(world::TileType::Coin); return; }

        const auto giveTool = [&](entities::ToolKind kind, entities::ToolTier tier) {
            player_.addTool(kind, tier);
            chatConsole_.addMessage("GAVE " + itemId, true);
        };
        const auto tierFrom = [&](const std::string& name, entities::ToolTier& outTier) {
            if (name == "wood") { outTier = entities::ToolTier::Wood; return true; }
            if (name == "stone") { outTier = entities::ToolTier::Stone; return true; }
            if (name == "copper") { outTier = entities::ToolTier::Copper; return true; }
            if (name == "iron") { outTier = entities::ToolTier::Iron; return true; }
            if (name == "gold") { outTier = entities::ToolTier::Gold; return true; }
            return false;
        };
        const auto splitAt = itemId.find('_');
        if (splitAt != std::string::npos) {
            const std::string left = itemId.substr(0, splitAt);
            const std::string right = itemId.substr(splitAt + 1);
            entities::ToolTier tier{};
            if (tierFrom(left, tier)) {
                if (right == "pickaxe") { giveTool(entities::ToolKind::Pickaxe, tier); return; }
                if (right == "axe") { giveTool(entities::ToolKind::Axe, tier); return; }
                if (right == "shovel") { giveTool(entities::ToolKind::Shovel, tier); return; }
                if (right == "hoe") { giveTool(entities::ToolKind::Hoe, tier); return; }
                if (right == "sword") { giveTool(entities::ToolKind::Sword, tier); return; }
                if (right == "bow") { giveTool(entities::ToolKind::Bow, tier); return; }
                if (right == "helmet") {
                    player_.addArmor(left == "copper" ? entities::ArmorId::CopperHelmet
                                      : left == "iron" ? entities::ArmorId::IronHelmet
                                      : entities::ArmorId::GoldHelmet);
                    chatConsole_.addMessage("GAVE " + itemId, true);
                    return;
                }
                if (right == "chest" || right == "chestplate") {
                    player_.addArmor(left == "copper" ? entities::ArmorId::CopperChest
                                      : left == "iron" ? entities::ArmorId::IronChest
                                      : entities::ArmorId::GoldChest);
                    chatConsole_.addMessage("GAVE " + itemId, true);
                    return;
                }
                if (right == "leggings" || right == "legs") {
                    player_.addArmor(left == "copper" ? entities::ArmorId::CopperLeggings
                                      : left == "iron" ? entities::ArmorId::IronLeggings
                                      : entities::ArmorId::GoldLeggings);
                    chatConsole_.addMessage("GAVE " + itemId, true);
                    return;
                }
            }
            if (itemId == "fleet_boots") { player_.addAccessory(entities::AccessoryId::FleetBoots); chatConsole_.addMessage("GAVE " + itemId, true); return; }
            if (itemId == "vitality_charm") { player_.addAccessory(entities::AccessoryId::VitalityCharm); chatConsole_.addMessage("GAVE " + itemId, true); return; }
            if (itemId == "miner_ring") { player_.addAccessory(entities::AccessoryId::MinerRing); chatConsole_.addMessage("GAVE " + itemId, true); return; }
        }
        chatConsole_.addMessage("UNKNOWN ITEM", true);
        return;
    }
    if (verb == "time_set") {
        float value = 0.0F;
        stream >> value;
        if (!stream.fail()) {
            const float clamped = std::clamp(value, 0.0F, dayLength_);
            timeOfDay_ = clamped;
            const float normalized = normalizedTimeOfDay();
            isNight_ = normalized >= kNightStart && normalized < kNightEnd;
            chatConsole_.addMessage("TIME SET", true);
        } else {
            chatConsole_.addMessage("USAGE TIME_SET 0-" + std::to_string(static_cast<int>(dayLength_)), true);
        }
        return;
    }
    if (verb == "locate") {
        std::string target;
        stream >> target;
        if (target == "dragon_den") {
            const auto denInfo = generator_.dragonDenInfo(world_, worldSeed_);
            if (denInfo.radiusX > 0 && denInfo.radiusY > 0) {
                chatConsole_.addMessage("DRAGON DEN " + std::to_string(denInfo.centerX) + " "
                                            + std::to_string(denInfo.centerY),
                                        true);
            } else {
                chatConsole_.addMessage("NO DRAGON DEN", true);
            }
            return;
        }
        chatConsole_.addMessage("USAGE: /locate dragon_den", true);
        return;
    }
    if (verb == "tp") {
        float x = 0.0F;
        float y = 0.0F;
        stream >> x >> y;
        if (!stream.fail()) {
            entities::Vec2 bestPos{};
            const bool found = findNearestOpenSpot({x, y}, bestPos);
            player_.setPosition(bestPos);
            player_.setVelocity({0.0F, 0.0F});
            player_.setOnGround(false);
            cameraPosition_ = clampCameraTarget(bestPos);
            chatConsole_.addMessage(found ? "TELEPORTED" : "TP SAFE SPOT NOT FOUND", true);
        } else {
            chatConsole_.addMessage("USAGE TP X Y", true);
        }
        return;
    }

    chatConsole_.addMessage("UNKNOWN COMMAND", true);
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
