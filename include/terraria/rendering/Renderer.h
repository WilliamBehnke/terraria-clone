#pragma once

#include "terraria/entities/Player.h"
#include "terraria/rendering/HudState.h"
#include "terraria/world/World.h"

#include <memory>

namespace terraria::core {
struct AppConfig;
}

namespace terraria::rendering {

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void initialize() = 0;
    virtual void render(const world::World& world, const entities::Player& player, const HudState& hud) = 0;
    virtual void shutdown() = 0;
};

std::unique_ptr<IRenderer> CreateSdlRenderer(const core::AppConfig& config);

} // namespace terraria::rendering
