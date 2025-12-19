#include "terraria/game/Game.h"

#include <algorithm>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <random>

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
constexpr float kCollisionEpsilon = 0.001F;
constexpr float kTilePixels = 16.0F;
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
constexpr float kPerfSmoothing = 0.1F;
constexpr float kSwordSwingDuration = 0.32F;
constexpr float kSwordSwingRadius = 1.3F;
constexpr float kSwordSwingHalfWidth = 0.6F;
constexpr float kSwordSwingHalfHeight = 0.5F;
constexpr float kSwordSwingArc = 2.6F;
constexpr float kProjectileSpeed = 22.0F;
constexpr float kProjectileLifetime = 2.5F;
constexpr float kPi = 3.1415926535F;

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

float PickaxeSpeedBonus(entities::ToolTier tier) {
    const int value = entities::ToolTierValue(tier);
    if (value <= 0) {
        return 1.0F;
    }
    return 1.0F + 0.2F * static_cast<float>(value);
}

int SwordDamageForTier(entities::ToolTier tier) {
    const int value = entities::ToolTierValue(tier);
    if (value <= 0) {
        return kPlayerAttackDamage;
    }
    return kPlayerAttackDamage + value * 6;
}

int RangedDamageForTier(entities::ToolTier tier) {
    const int value = entities::ToolTierValue(tier);
    if (value <= 0) {
        return 14;
    }
    return 14 + value * 5;
}

} // namespace

Game::Game(const core::AppConfig& config)
    : config_{config},
      world_{config.worldWidth, config.worldHeight},
      generator_{},
      player_{},
      renderer_{rendering::CreateSdlRenderer(config_)},
      inputSystem_{input::CreateSdlInputSystem()},
      zombieRng_{static_cast<std::uint32_t>(config.worldWidth * 131 + config.worldHeight * 977)},
      dayLength_{kDayLengthSeconds} {
    craftingRecipes_.reserve(32);
    const auto addTileRecipe = [&](world::TileType output, int count, std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.output = output;
        recipe.outputCount = count;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    const auto addToolRecipe = [&](entities::ToolKind kind,
                                   entities::ToolTier tier,
                                   std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.outputIsTool = true;
        recipe.toolKind = kind;
        recipe.toolTier = tier;
        recipe.outputCount = 1;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    const auto addArmorRecipe = [&](entities::ArmorId armorId, std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.outputIsArmor = true;
        recipe.armorId = armorId;
        recipe.outputCount = 1;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    const auto addAccessoryRecipe = [&](entities::AccessoryId accessoryId,
                                        std::initializer_list<CraftIngredient> ingredients) {
        CraftingRecipe recipe{};
        recipe.outputIsAccessory = true;
        recipe.accessoryId = accessoryId;
        recipe.outputCount = 1;
        recipe.ingredientCount = static_cast<int>(ingredients.size());
        int idx = 0;
        for (const auto& ingredient : ingredients) {
            if (idx >= static_cast<int>(recipe.ingredients.size())) {
                break;
            }
            recipe.ingredients[static_cast<std::size_t>(idx++)] = ingredient;
        }
        craftingRecipes_.push_back(recipe);
    };

    addTileRecipe(world::TileType::WoodPlank, 1, {CraftIngredient{world::TileType::Wood, 4}});
    addTileRecipe(world::TileType::StoneBrick, 1, {CraftIngredient{world::TileType::Stone, 4}});

    addToolRecipe(entities::ToolKind::Pickaxe, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 8}});
    addToolRecipe(entities::ToolKind::Axe, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 6}});
    addToolRecipe(entities::ToolKind::Sword, entities::ToolTier::Wood, {CraftIngredient{world::TileType::Wood, 7}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 10}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 8}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Stone,
                  {CraftIngredient{world::TileType::Stone, 9}, CraftIngredient{world::TileType::Wood, 2}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 10}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 8}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 9}, CraftIngredient{world::TileType::Wood, 2}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 12}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 10}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Iron,
                  {CraftIngredient{world::TileType::IronOre, 11}, CraftIngredient{world::TileType::Wood, 2}});

    addToolRecipe(entities::ToolKind::Pickaxe,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 14}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Axe,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 12}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Sword,
                  entities::ToolTier::Gold,
                  {CraftIngredient{world::TileType::GoldOre, 12}, CraftIngredient{world::TileType::Wood, 2}});
    addToolRecipe(entities::ToolKind::Blaster,
                  entities::ToolTier::Copper,
                  {CraftIngredient{world::TileType::CopperOre, 16}, CraftIngredient{world::TileType::Wood, 6}});

    addArmorRecipe(entities::ArmorId::WoodHelmet, {CraftIngredient{world::TileType::Wood, 15}});
    addArmorRecipe(entities::ArmorId::WoodChest, {CraftIngredient{world::TileType::Wood, 25}});
    addArmorRecipe(entities::ArmorId::WoodBoots, {CraftIngredient{world::TileType::Wood, 20}});
    addArmorRecipe(entities::ArmorId::CopperHelmet,
                   {CraftIngredient{world::TileType::CopperOre, 12}, CraftIngredient{world::TileType::Wood, 3}});
    addArmorRecipe(entities::ArmorId::CopperChest,
                   {CraftIngredient{world::TileType::CopperOre, 18}, CraftIngredient{world::TileType::Wood, 4}});
    addArmorRecipe(entities::ArmorId::CopperBoots,
                   {CraftIngredient{world::TileType::CopperOre, 14}, CraftIngredient{world::TileType::Wood, 3}});
    addArmorRecipe(entities::ArmorId::IronHelmet,
                   {CraftIngredient{world::TileType::IronOre, 14}, CraftIngredient{world::TileType::Wood, 3}});
    addArmorRecipe(entities::ArmorId::IronChest,
                   {CraftIngredient{world::TileType::IronOre, 22}, CraftIngredient{world::TileType::Wood, 4}});
    addArmorRecipe(entities::ArmorId::IronBoots,
                   {CraftIngredient{world::TileType::IronOre, 16}, CraftIngredient{world::TileType::Wood, 3}});

    addAccessoryRecipe(entities::AccessoryId::FleetBoots,
                       {CraftIngredient{world::TileType::IronOre, 8}, CraftIngredient{world::TileType::Wood, 4}});
    addAccessoryRecipe(entities::AccessoryId::VitalityCharm,
                       {CraftIngredient{world::TileType::CopperOre, 10}, CraftIngredient{world::TileType::Wood, 3}});
    addAccessoryRecipe(entities::AccessoryId::MinerRing,
                       {CraftIngredient{world::TileType::Stone, 20}, CraftIngredient{world::TileType::CopperOre, 6}});
}

void Game::initialize() {
    generator_.generate(world_);

    const int spawnX = world_.width() / 2;
    int spawnY = 0;
    for (int y = 0; y < world_.height(); ++y) {
        if (world_.tile(spawnX, y).active()) {
            spawnY = (y > 0) ? y - 1 : 0;
            break;
        }
    }
    player_.setPosition({static_cast<float>(spawnX), static_cast<float>(spawnY)});
    spawnPosition_ = player_.position();
    player_.resetHealth();
    player_.addTool(entities::ToolKind::Pickaxe, entities::ToolTier::Copper);
    player_.addTool(entities::ToolKind::Axe, entities::ToolTier::Copper);
    player_.addTool(entities::ToolKind::Sword, entities::ToolTier::Copper);
    player_.addTool(entities::ToolKind::Blaster, entities::ToolTier::Copper);
    std::uniform_real_distribution<float> timerDist(kZombieSpawnIntervalMin, kZombieSpawnIntervalMax);
    zombieSpawnTimer_ = timerDist(zombieRng_);
    timeOfDay_ = dayLength_ * std::clamp(kNightStart + 0.05F, 0.0F, 1.0F);
    isNight_ = true;

    cameraPosition_ = clampCameraTarget(player_.position());
    renderer_->initialize();
    inputSystem_->initialize();
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
    return !inputSystem_->shouldQuit();
}

void Game::shutdown() {
    inputSystem_->shutdown();
    renderer_->shutdown();
}

void Game::handleInput() {
    inputSystem_->poll();
    const auto& inputState = inputSystem_->state();
    if (inputState.inventoryToggle) {
        if (inventoryOpen_) {
            if (placeCarriedStack()) {
                inventoryOpen_ = false;
            }
        } else {
            inventoryOpen_ = true;
        }
    }

    if (inputState.toggleCamera) {
        toggleCameraMode();
    }

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
    } else if (!inventoryOpen_) {
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

    const int recipeCount = totalCraftRecipes();
    if (inventoryOpen_) {
        updateCraftLayoutMetrics(recipeCount);
        if (recipeCount > 0) {
            int delta = 0;
            if (inputState.craftPrev) {
                delta -= 1;
            }
            if (inputState.craftNext) {
                delta += 1;
            }
            if (delta != 0) {
                craftSelection_ = (craftSelection_ + delta + recipeCount) % recipeCount;
                ensureCraftSelectionVisible(recipeCount);
                updateCraftLayoutMetrics(recipeCount);
            }
            if (inputState.craftExecute) {
                craftSelectedRecipe();
            }
        }
    } else {
        craftScrollbarDragging_ = false;
        craftScrollOffset_ = 0;
    }
}

void Game::update(float dt) {
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
    updateProjectiles(dt);
    updateSwordSwing(dt);
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
    return tile.isSolid();
}

bool Game::solidAtPosition(const entities::Vec2& pos) const {
    const int tileX = static_cast<int>(std::floor(pos.x));
    const int tileY = static_cast<int>(std::floor(pos.y));
    return isSolidTile(tileX, tileY);
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
    handleInventoryInput();
    if (!inventoryOpen_) {
        handleBreaking(dt);
        handlePlacement(dt);
    } else {
        breakState_ = {};
    }
    handleCrafting(dt);
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
    if (inventoryOpen_) {
        breakState_ = {};
        return;
    }
    const auto& state = inputSystem_->state();
    playerAttackCooldown_ = std::max(0.0F, playerAttackCooldown_ - dt);

    const entities::InventorySlot* activeSlot = nullptr;
    if (selectedHotbar_ >= 0 && selectedHotbar_ < entities::kHotbarSlots) {
        activeSlot = &player_.hotbar()[static_cast<std::size_t>(selectedHotbar_)];
    }

    if (!state.breakHeld) {
        breakState_ = {};
        return;
    }

    if (activeSlot && activeSlot->isTool()) {
        if (activeSlot->toolKind == entities::ToolKind::Sword) {
            breakState_ = {};
            if (playerAttackCooldown_ <= 0.0F && !swordSwing_.active) {
                int aimX = 0;
                int aimY = 0;
                entities::Vec2 pivot{player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
                float aimAngle = (player_.velocity().x < 0.0F) ? kPi : 0.0F;
                if (cursorWorldTile(aimX, aimY)) {
                    entities::Vec2 target{static_cast<float>(aimX) + 0.5F, static_cast<float>(aimY) + 0.5F};
                    entities::Vec2 delta{target.x - pivot.x, target.y - pivot.y};
                    if (std::fabs(delta.x) > 0.001F || std::fabs(delta.y) > 0.001F) {
                        aimAngle = std::atan2(delta.y, delta.x);
                    }
                }
                startSwordSwing(aimAngle);
                playerAttackCooldown_ = kPlayerAttackCooldown;
            }
            return;
        }
        if (activeSlot->toolKind == entities::ToolKind::Blaster) {
            breakState_ = {};
            if (playerAttackCooldown_ <= 0.0F) {
                entities::Vec2 direction{1.0F, 0.0F};
                int aimX = 0;
                int aimY = 0;
                if (cursorWorldTile(aimX, aimY)) {
                    entities::Vec2 origin{player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
                    entities::Vec2 target{static_cast<float>(aimX) + 0.5F, static_cast<float>(aimY) + 0.5F};
                    entities::Vec2 delta{target.x - origin.x, target.y - origin.y};
                    const float len = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                    if (len > 0.001F) {
                        direction.x = delta.x / len;
                        direction.y = delta.y / len;
                    } else {
                        direction.x = (aimX >= static_cast<int>(std::floor(origin.x))) ? 1.0F : -1.0F;
                        direction.y = 0.0F;
                    }
                } else if (player_.velocity().x < 0.0F) {
                    direction.x = -1.0F;
                }
                spawnPlayerProjectile(direction);
                playerAttackCooldown_ = kPlayerAttackCooldown * 0.6F;
            }
            return;
        }
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

    auto& target = world_.tile(tileX, tileY);
    const world::TileType targetType = target.type();
    if (!hasRequiredPickaxe(targetType)) {
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
    if (inventoryOpen_) {
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
    const world::TileType type = slot.blockType;
    world_.setTile(tileX, tileY, type, true);
    player_.consumeSlot(selectedHotbar_, 1);
    placeCooldown_ = 0.2F;
}

void Game::handleCrafting(float dt) {
    craftCooldown_ = std::max(0.0F, craftCooldown_ - dt);
}

bool Game::canCraft(const CraftingRecipe& recipe) const {
    for (int i = 0; i < recipe.ingredientCount; ++i) {
        const auto& ingredient = recipe.ingredients[static_cast<std::size_t>(i)];
        if (player_.inventoryCount(ingredient.type) < ingredient.count) {
            return false;
        }
    }
    return true;
}

bool Game::tryCraft(const CraftingRecipe& recipe) {
    if (!canCraft(recipe)) {
        return false;
    }
    const auto canStoreTile = [&](world::TileType type) {
        for (const auto& slot : player_.inventory()) {
            if (slot.isBlock() && slot.blockType == type && slot.count < entities::kMaxStackCount) {
                return true;
            }
            if (slot.empty()) {
                return true;
            }
        }
        return false;
    };
    const auto canStoreTool = [&](entities::ToolKind kind, entities::ToolTier tier) {
        for (const auto& slot : player_.inventory()) {
            if (slot.empty()) {
                return true;
            }
            if (slot.isTool() && slot.toolKind == kind && slot.toolTier == tier) {
                return true;
            }
        }
        return false;
    };
    const auto canStoreArmor = [&]() {
        for (const auto& slot : player_.inventory()) {
            if (slot.empty()) {
                return true;
            }
        }
        return false;
    };
    const auto canStoreAccessory = [&]() {
        for (const auto& slot : player_.inventory()) {
            if (slot.empty()) {
                return true;
            }
        }
        return false;
    };

    if (recipe.outputIsTool) {
        if (!canStoreTool(recipe.toolKind, recipe.toolTier)) {
            return false;
        }
    } else if (recipe.outputIsArmor) {
        if (!canStoreArmor()) {
            return false;
        }
    } else if (recipe.outputIsAccessory) {
        if (!canStoreAccessory()) {
            return false;
        }
    } else if (!canStoreTile(recipe.output)) {
        return false;
    }

    for (int i = 0; i < recipe.ingredientCount; ++i) {
        const auto& ingredient = recipe.ingredients[static_cast<std::size_t>(i)];
        player_.consumeFromInventory(ingredient.type, ingredient.count);
    }
    if (recipe.outputIsTool) {
        return player_.addTool(recipe.toolKind, recipe.toolTier);
    }
    if (recipe.outputIsArmor) {
        return player_.addArmor(recipe.armorId);
    }
    if (recipe.outputIsAccessory) {
        return player_.addAccessory(recipe.accessoryId);
    }

    bool success = true;
    for (int i = 0; i < recipe.outputCount; ++i) {
        if (!player_.addToInventory(recipe.output)) {
            success = false;
        }
    }
    return success;
}

void Game::handleInventoryInput() {
    const auto& state = inputSystem_->state();
    if (!inventoryOpen_) {
        hoveredInventorySlot_ = -1;
        hoveredArmorSlot_ = -1;
        hoveredAccessorySlot_ = -1;
        craftScrollbarDragging_ = false;
        return;
    }

    const bool equipmentHandled = handleEquipmentInput();

    hoveredInventorySlot_ = inventorySlotIndexAt(state.mouseX, state.mouseY);
    if (!equipmentHandled && state.inventoryClick && hoveredInventorySlot_ >= 0) {
        handleInventoryClick(hoveredInventorySlot_);
    }

    if (!equipmentHandled) {
        handleCraftingPointerInput();
    }
}

void Game::handleInventoryClick(int slotIndex) {
    auto& slots = player_.inventory();
    if (slotIndex < 0 || slotIndex >= static_cast<int>(slots.size())) {
        return;
    }

    auto& target = slots[static_cast<std::size_t>(slotIndex)];
    if (!carryingItem()) {
        if (!target.empty()) {
            carriedSlot_ = target;
            target.clear();
        }
        return;
    }

    if (target.empty()) {
        target = carriedSlot_;
        if (target.isBlock()) {
            const int moved = std::min(carriedSlot_.count, entities::kMaxStackCount);
            target.count = moved;
            carriedSlot_.count -= moved;
            if (carriedSlot_.count <= 0) {
                carriedSlot_.clear();
            }
        } else {
            carriedSlot_.clear();
        }
        return;
    }

    if (target.canStackWith(carriedSlot_)) {
        const int space = std::max(0, entities::kMaxStackCount - target.count);
        if (space > 0) {
            const int moved = std::min(space, carriedSlot_.count);
            target.count += moved;
            carriedSlot_.count -= moved;
            if (carriedSlot_.count <= 0) {
                carriedSlot_.clear();
            }
        }
        return;
    }

    std::swap(target, carriedSlot_);
}

int Game::inventorySlotIndexAt(int mouseX, int mouseY) const {
    if (!inventoryOpen_) {
        return -1;
    }
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int margin = spacing;
    const int columns = entities::kHotbarSlots;
    const int totalRows = entities::kInventoryRows;
    const int startX = margin;
    const int startY = config_.windowHeight - slotHeight - margin;

    for (int row = 0; row < totalRows; ++row) {
        const int rowY = startY - row * (slotHeight + spacing);
        if (mouseY < rowY || mouseY >= rowY + slotHeight) {
            continue;
        }
        for (int col = 0; col < columns; ++col) {
            const int colX = startX + col * (slotWidth + spacing);
            if (mouseX >= colX && mouseX < colX + slotWidth) {
                return row * columns + col;
            }
        }
    }
    return -1;
}

bool Game::placeCarriedStack() {
    if (!carryingItem()) {
        return true;
    }

    auto& slots = player_.inventory();
    if (carriedSlot_.isBlock()) {
        for (auto& slot : slots) {
            if (slot.canStackWith(carriedSlot_)) {
                const int space = std::max(0, entities::kMaxStackCount - slot.count);
                if (space <= 0) {
                    continue;
                }
                const int moved = std::min(space, carriedSlot_.count);
                slot.count += moved;
                carriedSlot_.count -= moved;
                if (carriedSlot_.count <= 0) {
                    carriedSlot_.clear();
                    return true;
                }
            }
        }
    }

    for (auto& slot : slots) {
        if (!slot.empty()) {
            continue;
        }
        slot = carriedSlot_;
        if (slot.isBlock()) {
            const int moved = std::min(carriedSlot_.count, entities::kMaxStackCount);
            slot.count = moved;
            carriedSlot_.count -= moved;
            if (carriedSlot_.count <= 0) {
                carriedSlot_.clear();
                return true;
            }
        } else {
            carriedSlot_.clear();
            return true;
        }
    }
    return false;
}

bool Game::carryingItem() const {
    return !carriedSlot_.empty();
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

bool Game::hasRequiredPickaxe(world::TileType tileType) const {
    const entities::ToolTier required = RequiredPickaxeTier(tileType);
    if (required == entities::ToolTier::None) {
        return true;
    }
    const int equippedValue = entities::ToolTierValue(selectedToolTier(entities::ToolKind::Pickaxe));
    return equippedValue >= entities::ToolTierValue(required);
}

int Game::activeSwordDamage() const {
    return SwordDamageForTier(selectedToolTier(entities::ToolKind::Sword));
}

int Game::activeRangedDamage() const {
    return RangedDamageForTier(selectedToolTier(entities::ToolKind::Blaster));
}

float Game::breakSpeedMultiplier(world::TileType tileType) const {
    (void)tileType;
    const entities::ToolTier pickTier = selectedToolTier(entities::ToolKind::Pickaxe);
    if (entities::ToolTierValue(pickTier) <= 0) {
        return 1.0F;
    }
    return PickaxeSpeedBonus(pickTier);
}

void Game::startSwordSwing(float aimAngle) {
    swordSwing_.active = true;
    swordSwing_.timer = 0.0F;
    swordSwing_.duration = kSwordSwingDuration;
    swordSwing_.radius = kSwordSwingRadius;
    swordSwing_.halfWidth = kSwordSwingHalfWidth;
    swordSwing_.halfHeight = kSwordSwingHalfHeight;
    const float halfArc = kSwordSwingArc * 0.5F;
    swordSwing_.angleStart = aimAngle - halfArc;
    swordSwing_.angleEnd = aimAngle + halfArc;
    swordSwingHitIds_.clear();
    updateSwordSwing(0.0F);
}

void Game::updateSwordSwing(float dt) {
    if (!swordSwing_.active) {
        return;
    }
    swordSwing_.timer += dt;
    const float progress = std::clamp(swordSwing_.timer / swordSwing_.duration, 0.0F, 1.0F);
    const float angle = swordSwing_.angleStart + (swordSwing_.angleEnd - swordSwing_.angleStart) * progress;
    const entities::Vec2 pivot{player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
    swordSwing_.center = {pivot.x + std::cos(angle) * swordSwing_.radius, pivot.y + std::sin(angle) * swordSwing_.radius};
    const int damage = activeSwordDamage();
    for (auto& zombie : zombies_) {
        if (!zombie.alive()) {
            continue;
        }
        if (std::find(swordSwingHitIds_.begin(), swordSwingHitIds_.end(), zombie.id) != swordSwingHitIds_.end()) {
            continue;
        }
        if (aabbOverlap(swordSwing_.center,
                        swordSwing_.halfWidth,
                        swordSwing_.halfHeight * 2.0F,
                        zombie.position,
                        entities::kZombieHalfWidth,
                        entities::kZombieHeight)) {
            zombie.takeDamage(damage);
            swordSwingHitIds_.push_back(zombie.id);
        }
    }
    if (swordSwing_.timer >= swordSwing_.duration) {
        swordSwing_.active = false;
    }
}

void Game::spawnPlayerProjectile(const entities::Vec2& direction) {
    Projectile projectile{};
    projectile.position = {player_.position().x, player_.position().y - entities::kPlayerHeight * 0.4F};
    projectile.velocity = {direction.x * kProjectileSpeed, direction.y * kProjectileSpeed};
    projectile.lifetime = kProjectileLifetime;
    projectile.radius = 0.2F;
    projectile.damage = activeRangedDamage();
    projectile.fromPlayer = true;
    projectiles_.push_back(projectile);
}

void Game::updateProjectiles(float dt) {
    for (auto& projectile : projectiles_) {
        projectile.lifetime -= dt;
        if (projectile.lifetime <= 0.0F) {
            continue;
        }
        projectile.position.x += projectile.velocity.x * dt;
        projectile.position.y += projectile.velocity.y * dt;
        if (projectile.position.x < 0.0F || projectile.position.y < 0.0F
            || projectile.position.x >= static_cast<float>(world_.width())
            || projectile.position.y >= static_cast<float>(world_.height())
            || solidAtPosition(projectile.position)) {
            projectile.lifetime = 0.0F;
            continue;
        }
        if (projectile.fromPlayer) {
            for (auto& zombie : zombies_) {
                if (!zombie.alive()) {
                    continue;
                }
                if (aabbOverlap(projectile.position,
                                projectile.radius,
                                projectile.radius * 2.0F,
                                zombie.position,
                                entities::kZombieHalfWidth,
                                entities::kZombieHeight)) {
                    zombie.takeDamage(projectile.damage);
                    projectile.lifetime = 0.0F;
                    break;
                }
            }
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(),
                                      projectiles_.end(),
                                      [](const Projectile& projectile) { return projectile.lifetime <= 0.0F; }),
                       projectiles_.end());
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

    for (auto& zombie : zombies_) {
        if (!zombie.alive()) {
            continue;
        }
        zombie.attackCooldown = std::max(0.0F, zombie.attackCooldown - dt);
        applyZombiePhysics(zombie, dt);
        if (zombiesOverlapPlayer(zombie) && zombie.attackCooldown <= 0.0F) {
            player_.applyDamage(kZombieDamage);
            zombie.attackCooldown = kZombieAttackInterval;
        }
    }

    zombies_.erase(
        std::remove_if(zombies_.begin(), zombies_.end(), [](const entities::Zombie& zombie) { return !zombie.alive(); }),
        zombies_.end());
}

void Game::applyZombiePhysics(entities::Zombie& zombie, float dt) {
    const float horizontalDelta = std::fabs(zombie.position.x - zombie.lastX);
    if (horizontalDelta < 0.02F) {
        zombie.stuckTimer += dt;
    } else {
        zombie.stuckTimer = 0.0F;
    }
    zombie.jumpCooldown = std::max(0.0F, zombie.jumpCooldown - dt);

    const float direction = (player_.position().x < zombie.position.x) ? -1.0F : 1.0F;
    zombie.velocity.x = direction * kZombieMoveSpeed;

    const float verticalDelta = player_.position().y - zombie.position.y;
    const bool playerAbove = verticalDelta < -1.2F;
    const bool playerBelow = verticalDelta > 1.2F;

    const int aheadX = static_cast<int>(std::floor(zombie.position.x + direction * (entities::kZombieHalfWidth + 0.2F)));
    const int footY = static_cast<int>(std::floor(zombie.position.y));
    bool obstacle = false;
    for (int y = footY - 1; y <= footY; ++y) {
        if (isSolidTile(aheadX, y)) {
            obstacle = true;
            break;
        }
    }

    const int dropCheckX = static_cast<int>(std::floor(zombie.position.x + direction * (entities::kZombieHalfWidth + 0.9F)));
    const bool dropAhead = !isSolidTile(dropCheckX, footY + 1);

    if (zombie.onGround && zombie.jumpCooldown <= 0.0F) {
        if (obstacle || playerAbove) {
            float jumpScale = playerAbove ? 1.25F : 1.0F;
            zombie.velocity.y = -kZombieJumpVelocity * jumpScale;
            zombie.onGround = false;
            zombie.jumpCooldown = 0.35F;
        } else if (dropAhead && !playerBelow) {
            zombie.velocity.x = direction * kZombieMoveSpeed * 0.2F;
        }
    }

    if (zombie.stuckTimer > 0.5F && zombie.onGround && zombie.jumpCooldown <= 0.0F) {
        zombie.velocity.y = -kZombieJumpVelocity * 1.35F;
        zombie.onGround = false;
        zombie.jumpCooldown = 0.4F;
        zombie.stuckTimer = 0.0F;
    }

    zombie.velocity.y += kGravity * dt;

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
    zombie.lastX = zombie.position.x;
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
    zombie.id = nextZombieId_++;
    zombie.lastX = zombie.position.x;

    if (collidesAabb(zombie.position, entities::kZombieHalfWidth, entities::kZombieHeight)) {
        return;
    }

    zombies_.push_back(zombie);
}

bool Game::zombiesOverlapPlayer(const entities::Zombie& zombie) const {
    return aabbOverlap(zombie.position,
                       entities::kZombieHalfWidth,
                       entities::kZombieHeight,
                       player_.position(),
                       entities::kPlayerHalfWidth,
                       entities::kPlayerHeight);
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

int Game::totalCraftRecipes() const {
    return std::min(static_cast<int>(craftingRecipes_.size()), rendering::kMaxCraftRecipes);
}

void Game::updateCraftLayoutMetrics(int recipeCount) {
    CraftLayout layout{};
    const int margin = rendering::kInventorySlotSpacing;
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int rows = rendering::kInventoryRows;
    const int baseY = config_.windowHeight - slotHeight - margin;
    layout.inventoryTop = baseY - (rows - 1) * (slotHeight + spacing);
    const int minPanelWidth = 260;
    layout.rowHeight = 26;
    layout.rowSpacing = 4;
    const int desiredPanelX = margin + rendering::kInventoryColumns * (slotWidth + spacing) + spacing;
    const int availableWidth = config_.windowWidth - desiredPanelX - margin;
    layout.panelWidth = std::max(minPanelWidth, availableWidth);
    layout.panelX = std::max(margin, config_.windowWidth - layout.panelWidth - margin);
    if (layout.panelX > desiredPanelX) {
        layout.panelX = desiredPanelX;
        layout.panelWidth = std::max(minPanelWidth, config_.windowWidth - layout.panelX - margin);
    }
    layout.panelY = layout.inventoryTop;
    layout.visibleRows =
        std::max(1, (config_.windowHeight - layout.panelY - margin) / (layout.rowHeight + layout.rowSpacing));
    layout.trackWidth = 6;
    layout.trackX = layout.panelX + layout.panelWidth + 4;
    layout.trackY = layout.inventoryTop;
    layout.trackHeight = layout.visibleRows * (layout.rowHeight + layout.rowSpacing) - layout.rowSpacing;

    const int maxStart = std::max(0, recipeCount - layout.visibleRows);
    craftScrollOffset_ = std::clamp(craftScrollOffset_, 0, maxStart);
    layout.scrollbarVisible = recipeCount > layout.visibleRows && layout.trackHeight > 0;
    if (layout.scrollbarVisible) {
        const float visibleRatio = static_cast<float>(layout.visibleRows) / static_cast<float>(recipeCount);
        layout.thumbHeight = std::max(8, static_cast<int>(std::round(layout.trackHeight * visibleRatio)));
        const int trackSpan = std::max(0, layout.trackHeight - layout.thumbHeight);
        const float offsetRatio =
            (maxStart > 0) ? static_cast<float>(craftScrollOffset_) / static_cast<float>(maxStart) : 0.0F;
        layout.thumbY = layout.trackY + static_cast<int>(std::round(offsetRatio * static_cast<float>(trackSpan)));
    } else {
        layout.thumbHeight = layout.trackHeight;
        layout.thumbY = layout.trackY;
    }
    craftLayout_ = layout;
}

void Game::ensureCraftSelectionVisible(int recipeCount) {
    if (recipeCount <= 0) {
        craftScrollOffset_ = 0;
        return;
    }
    craftSelection_ = std::clamp(craftSelection_, 0, recipeCount - 1);
    const int visible = craftLayout_.visibleRows;
    const int maxStart = std::max(0, recipeCount - visible);
    if (craftSelection_ < craftScrollOffset_) {
        craftScrollOffset_ = craftSelection_;
    } else if (craftSelection_ >= craftScrollOffset_ + visible) {
        craftScrollOffset_ = craftSelection_ - visible + 1;
    }
    craftScrollOffset_ = std::clamp(craftScrollOffset_, 0, maxStart);
}

bool Game::craftSelectedRecipe() {
    const int recipeCount = totalCraftRecipes();
    if (!inventoryOpen_ || recipeCount <= 0 || craftCooldown_ > 0.0F) {
        return false;
    }
    craftSelection_ = std::clamp(craftSelection_, 0, recipeCount - 1);
    if (craftSelection_ < 0 || craftSelection_ >= recipeCount) {
        return false;
    }
    if (tryCraft(craftingRecipes_[static_cast<std::size_t>(craftSelection_)])) {
        craftCooldown_ = 0.25F;
        return true;
    }
    return false;
}

void Game::updateCraftScrollbarDrag(int mouseY, int recipeCount) {
    if (!craftScrollbarDragging_ || !craftLayout_.scrollbarVisible || recipeCount <= 0) {
        return;
    }
    const int trackSpan = craftLayout_.trackHeight - craftLayout_.thumbHeight;
    if (trackSpan <= 0) {
        return;
    }
    int thumbOffset = mouseY - craftLayout_.trackY - static_cast<int>(craftScrollbarGrabOffset_);
    thumbOffset = std::clamp(thumbOffset, 0, trackSpan);
    const int maxStart = std::max(0, recipeCount - craftLayout_.visibleRows);
    const float ratio = static_cast<float>(thumbOffset) / static_cast<float>(trackSpan);
    craftScrollOffset_ = std::clamp(static_cast<int>(std::round(ratio * static_cast<float>(maxStart))), 0, maxStart);
    updateCraftLayoutMetrics(recipeCount);
}

int Game::craftRecipeIndexAt(int mouseX, int mouseY) const {
    if (!inventoryOpen_) {
        return -1;
    }
    const int recipeCount = totalCraftRecipes();
    if (recipeCount <= 0) {
        return -1;
    }

    if (mouseX < craftLayout_.panelX || mouseX > craftLayout_.panelX + craftLayout_.panelWidth) {
        return -1;
    }
    int rowY = craftLayout_.panelY;
    for (int row = 0; row < craftLayout_.visibleRows && (craftScrollOffset_ + row) < recipeCount; ++row) {
        const int top = rowY + row * (craftLayout_.rowHeight + craftLayout_.rowSpacing);
        const int bottom = top + craftLayout_.rowHeight;
        if (mouseY >= top && mouseY < bottom) {
            return craftScrollOffset_ + row;
        }
    }
    return -1;
}

void Game::handleCraftingPointerInput() {
    if (!inventoryOpen_) {
        craftScrollbarDragging_ = false;
        return;
    }
    const auto& state = inputSystem_->state();
    const int recipeCount = totalCraftRecipes();
    if (recipeCount <= 0) {
        craftScrollbarDragging_ = false;
        return;
    }

    if (!state.breakHeld) {
        craftScrollbarDragging_ = false;
    }

    if (state.inventoryClick && craftLayout_.scrollbarVisible) {
        const int mouseX = state.mouseX;
        const int mouseY = state.mouseY;
        const bool insideThumb = mouseX >= craftLayout_.trackX
            && mouseX <= craftLayout_.trackX + craftLayout_.trackWidth
            && mouseY >= craftLayout_.thumbY
            && mouseY <= craftLayout_.thumbY + craftLayout_.thumbHeight;
        if (insideThumb) {
            craftScrollbarDragging_ = true;
            craftScrollbarGrabOffset_ = static_cast<float>(mouseY - craftLayout_.thumbY);
            return;
        }
    }

    if (craftScrollbarDragging_ && state.breakHeld) {
        updateCraftScrollbarDrag(state.mouseY, recipeCount);
        return;
    }

    if (state.inventoryClick) {
        const int index = craftRecipeIndexAt(state.mouseX, state.mouseY);
        if (index >= 0) {
            craftSelection_ = index;
            ensureCraftSelectionVisible(recipeCount);
            updateCraftLayoutMetrics(recipeCount);
            craftSelectedRecipe();
        }
    }
}

void Game::updateEquipmentLayout() {
    const int slotWidth = rendering::kInventorySlotWidth;
    const int slotHeight = rendering::kInventorySlotHeight;
    const int spacing = rendering::kInventorySlotSpacing;
    const int topMargin = spacing + 140;
    const int armorX = spacing;
    const int accessoryX = armorX + slotWidth + spacing;

    for (int i = 0; i < rendering::kArmorSlotCount; ++i) {
        const int y = topMargin + i * (slotHeight + spacing);
        equipmentRects_[static_cast<std::size_t>(i)] = EquipmentSlotRect{true, i, armorX, y, slotWidth, slotHeight};
    }
    for (int j = 0; j < rendering::kAccessorySlotCount; ++j) {
        const int y = topMargin + j * (slotHeight + spacing);
        equipmentRects_[rendering::kArmorSlotCount + static_cast<std::size_t>(j)] =
            EquipmentSlotRect{false, j, accessoryX, y, slotWidth, slotHeight};
    }
}

int Game::equipmentSlotAt(int mouseX, int mouseY, bool armor) const {
    const int start = armor ? 0 : rendering::kArmorSlotCount;
    const int end = armor ? rendering::kArmorSlotCount : rendering::kTotalEquipmentSlots;
    for (int i = start; i < end; ++i) {
        const auto& rect = equipmentRects_[static_cast<std::size_t>(i)];
        if (mouseX >= rect.x && mouseX < rect.x + rect.w && mouseY >= rect.y && mouseY < rect.y + rect.h) {
            return rect.slotIndex;
        }
    }
    return -1;
}

bool Game::handleEquipmentInput() {
    hoveredArmorSlot_ = -1;
    hoveredAccessorySlot_ = -1;
    if (!inventoryOpen_) {
        return false;
    }
    updateEquipmentLayout();
    const auto& state = inputSystem_->state();
    hoveredArmorSlot_ = equipmentSlotAt(state.mouseX, state.mouseY, true);
    hoveredAccessorySlot_ = equipmentSlotAt(state.mouseX, state.mouseY, false);
    if (!state.inventoryClick) {
        return false;
    }
    if (hoveredArmorSlot_ >= 0) {
        return handleArmorSlotClick(hoveredArmorSlot_);
    }
    if (hoveredAccessorySlot_ >= 0) {
        return handleAccessorySlotClick(hoveredAccessorySlot_);
    }
    return false;
}

bool Game::handleArmorSlotClick(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= rendering::kArmorSlotCount) {
        return false;
    }
    const entities::ArmorSlot slot = static_cast<entities::ArmorSlot>(slotIndex);
    const entities::ArmorId equipped = player_.armorAt(slot);
    if (!carryingItem()) {
        if (equipped == entities::ArmorId::None) {
            return true;
        }
        carriedSlot_.clear();
        carriedSlot_.category = entities::ItemCategory::Armor;
        carriedSlot_.armorId = equipped;
        carriedSlot_.count = 1;
        player_.equipArmor(slot, entities::ArmorId::None);
        return true;
    }
    if (!carriedSlot_.isArmor()) {
        return false;
    }
    const entities::ArmorSlot carriedSlot = entities::ArmorSlotFor(carriedSlot_.armorId);
    if (carriedSlot != slot) {
        return false;
    }
    const entities::ArmorId carriedId = carriedSlot_.armorId;
    carriedSlot_.clear();
    if (equipped != entities::ArmorId::None) {
        carriedSlot_.category = entities::ItemCategory::Armor;
        carriedSlot_.armorId = equipped;
        carriedSlot_.count = 1;
    }
    player_.equipArmor(slot, carriedId);
    return true;
}

bool Game::handleAccessorySlotClick(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= rendering::kAccessorySlotCount) {
        return false;
    }
    const entities::AccessoryId equipped = player_.accessoryAt(slotIndex);
    if (!carryingItem()) {
        if (equipped == entities::AccessoryId::None) {
            return true;
        }
        carriedSlot_.clear();
        carriedSlot_.category = entities::ItemCategory::Accessory;
        carriedSlot_.accessoryId = equipped;
        carriedSlot_.count = 1;
        player_.equipAccessory(slotIndex, entities::AccessoryId::None);
        return true;
    }
    if (!carriedSlot_.isAccessory()) {
        return false;
    }
    const entities::AccessoryId carriedId = carriedSlot_.accessoryId;
    carriedSlot_.clear();
    if (equipped != entities::AccessoryId::None) {
        carriedSlot_.category = entities::ItemCategory::Accessory;
        carriedSlot_.accessoryId = equipped;
        carriedSlot_.count = 1;
    }
    player_.equipAccessory(slotIndex, carriedId);
    return true;
}

void Game::updateHudState() {
    const auto& inputState = inputSystem_->state();
    const auto fillSlot = [](const entities::InventorySlot& src, rendering::HotbarSlotHud& dst) {
        dst = {};
        if (src.isTool()) {
            dst.isTool = true;
            dst.toolKind = src.toolKind;
            dst.toolTier = src.toolTier;
            dst.count = src.count;
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
    const entities::Vec2 playerPos = player_.position();
    const entities::Vec2 coordPos = cameraMode_ ? cameraPosition_ : playerPos;
    hudState_.playerTileX = static_cast<int>(std::floor(coordPos.x));
    hudState_.playerTileY = static_cast<int>(std::floor(coordPos.y));
    hudState_.inventoryOpen = inventoryOpen_;
    hudState_.inventorySlotCount =
        std::min(rendering::kMaxInventorySlots, static_cast<int>(player_.inventory().size()));
    for (int i = 0; i < hudState_.inventorySlotCount; ++i) {
        fillSlot(player_.inventory()[static_cast<std::size_t>(i)],
                 hudState_.inventorySlots[static_cast<std::size_t>(i)]);
    }
    for (int i = hudState_.inventorySlotCount; i < rendering::kMaxInventorySlots; ++i) {
        hudState_.inventorySlots[static_cast<std::size_t>(i)] = {};
    }
    hudState_.hoveredInventorySlot = inventoryOpen_ ? hoveredInventorySlot_ : -1;
    hudState_.carryingItem = carryingItem();
    if (hudState_.carryingItem) {
        fillSlot(carriedSlot_, hudState_.carriedItem);
    } else {
        hudState_.carriedItem = {};
    }
    hudState_.useCamera = cameraMode_;
    hudState_.cameraX = cameraPosition_.x;
    hudState_.cameraY = cameraPosition_.y;
    hudState_.playerHealth = player_.health();
    hudState_.playerMaxHealth = player_.maxHealth();
    hudState_.playerDefense = player_.defense();
    hudState_.zombieCount = static_cast<int>(zombies_.size());
    hudState_.dayProgress = normalizedTimeOfDay();
    hudState_.isNight = isNight_;
    if (inventoryOpen_) {
        hudState_.craftRecipeCount = totalCraftRecipes();
        if (hudState_.craftRecipeCount > 0) {
            craftSelection_ = std::clamp(craftSelection_, 0, hudState_.craftRecipeCount - 1);
            hudState_.craftSelection = craftSelection_;
            for (int i = 0; i < hudState_.craftRecipeCount; ++i) {
                const auto& recipe = craftingRecipes_[static_cast<std::size_t>(i)];
                auto& entry = hudState_.craftRecipes[static_cast<std::size_t>(i)];
                entry = {};
                entry.outputIsTool = recipe.outputIsTool;
                entry.outputIsArmor = recipe.outputIsArmor;
                entry.outputIsAccessory = recipe.outputIsAccessory;
                entry.outputType = recipe.output;
                entry.outputCount = recipe.outputCount;
                entry.toolKind = recipe.toolKind;
                entry.toolTier = recipe.toolTier;
                entry.armorSlot = ArmorSlotFor(recipe.armorId);
                entry.armorId = recipe.armorId;
                entry.accessoryId = recipe.accessoryId;
                entry.ingredientCount = recipe.ingredientCount;
                for (int ing = 0; ing < recipe.ingredientCount; ++ing) {
                    entry.ingredientTypes[static_cast<std::size_t>(ing)] = recipe.ingredients[static_cast<std::size_t>(ing)].type;
                    entry.ingredientCounts[static_cast<std::size_t>(ing)] = recipe.ingredients[static_cast<std::size_t>(ing)].count;
                }
                entry.canCraft = canCraft(recipe);
            }
        } else {
            hudState_.craftSelection = 0;
        }
        for (int i = hudState_.craftRecipeCount; i < rendering::kMaxCraftRecipes; ++i) {
            hudState_.craftRecipes[static_cast<std::size_t>(i)] = {};
        }
        updateCraftLayoutMetrics(hudState_.craftRecipeCount);
        hudState_.craftScrollOffset = craftScrollOffset_;
        hudState_.craftVisibleRows = craftLayout_.visibleRows;
        hudState_.craftPanelX = craftLayout_.panelX;
        hudState_.craftPanelY = craftLayout_.panelY;
        hudState_.craftPanelWidth = craftLayout_.panelWidth;
        hudState_.craftRowHeight = craftLayout_.rowHeight;
        hudState_.craftRowSpacing = craftLayout_.rowSpacing;
        hudState_.craftScrollbarTrackX = craftLayout_.trackX;
        hudState_.craftScrollbarTrackY = craftLayout_.trackY;
        hudState_.craftScrollbarTrackHeight = craftLayout_.trackHeight;
        hudState_.craftScrollbarThumbY = craftLayout_.thumbY;
        hudState_.craftScrollbarThumbHeight = craftLayout_.thumbHeight;
        hudState_.craftScrollbarWidth = craftLayout_.trackWidth;
        hudState_.craftScrollbarVisible = craftLayout_.scrollbarVisible;
        hudState_.equipmentSlotCount = rendering::kTotalEquipmentSlots;
        updateEquipmentLayout();
        for (std::size_t i = 0; i < hudState_.equipmentSlots.size(); ++i) {
            auto& slotHud = hudState_.equipmentSlots[i];
            const auto& rect = equipmentRects_[i];
            slotHud = {};
            slotHud.isArmor = rect.isArmor;
            slotHud.slotIndex = rect.slotIndex;
            slotHud.x = rect.x;
            slotHud.y = rect.y;
            slotHud.w = rect.w;
            slotHud.h = rect.h;
            if (rect.isArmor) {
                slotHud.armorId = player_.armorAt(static_cast<entities::ArmorSlot>(rect.slotIndex));
                slotHud.occupied = slotHud.armorId != entities::ArmorId::None;
                slotHud.hovered = (hoveredArmorSlot_ == rect.slotIndex);
            } else {
                slotHud.accessoryId = player_.accessoryAt(rect.slotIndex);
                slotHud.occupied = slotHud.accessoryId != entities::AccessoryId::None;
                slotHud.hovered = (hoveredAccessorySlot_ == rect.slotIndex);
            }
        }
    } else {
        hudState_.craftRecipeCount = 0;
        hudState_.craftSelection = 0;
        hudState_.craftScrollOffset = 0;
        hudState_.craftVisibleRows = 0;
        hudState_.craftPanelX = 0;
        hudState_.craftPanelY = 0;
        hudState_.craftPanelWidth = 0;
        hudState_.craftRowHeight = 0;
        hudState_.craftRowSpacing = 0;
        hudState_.craftScrollbarTrackX = 0;
        hudState_.craftScrollbarTrackY = 0;
        hudState_.craftScrollbarTrackHeight = 0;
        hudState_.craftScrollbarThumbY = 0;
        hudState_.craftScrollbarThumbHeight = 0;
        hudState_.craftScrollbarWidth = 0;
        hudState_.craftScrollbarVisible = false;
        hudState_.equipmentSlotCount = 0;
        for (auto& entry : hudState_.craftRecipes) {
            entry = {};
        }
        for (auto& slot : hudState_.equipmentSlots) {
            slot = {};
        }
    }
    hudState_.swordSwingActive = swordSwing_.active;
    if (swordSwing_.active) {
        hudState_.swordSwingX = swordSwing_.center.x;
        hudState_.swordSwingY = swordSwing_.center.y;
        hudState_.swordSwingHalfWidth = swordSwing_.halfWidth;
        hudState_.swordSwingHalfHeight = swordSwing_.halfHeight;
    } else {
        hudState_.swordSwingX = 0.0F;
        hudState_.swordSwingY = 0.0F;
        hudState_.swordSwingHalfWidth = 0.0F;
        hudState_.swordSwingHalfHeight = 0.0F;
    }

    const int projectileCount = std::min(static_cast<int>(projectiles_.size()), rendering::kMaxProjectiles);
    hudState_.projectileCount = projectileCount;
    for (int i = 0; i < projectileCount; ++i) {
        const auto& projectile = projectiles_[static_cast<std::size_t>(i)];
        auto& entry = hudState_.projectiles[static_cast<std::size_t>(i)];
        entry.active = true;
        entry.x = projectile.position.x;
        entry.y = projectile.position.y;
        entry.radius = projectile.radius;
    }
    for (int i = projectileCount; i < rendering::kMaxProjectiles; ++i) {
        hudState_.projectiles[static_cast<std::size_t>(i)] = {};
    }

    hudState_.perfFrameMs = perfFrameTimeMs_;
    hudState_.perfUpdateMs = perfUpdateTimeMs_;
    hudState_.perfRenderMs = perfRenderTimeMs_;
    hudState_.perfFps = perfFps_;
    hudState_.mouseX = std::clamp(inputState.mouseX, 0, config_.windowWidth);
    hudState_.mouseY = std::clamp(inputState.mouseY, 0, config_.windowHeight);
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
