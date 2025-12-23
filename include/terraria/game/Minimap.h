#pragma once

#include "terraria/entities/Player.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/HudState.h"
#include "terraria/world/World.h"

namespace terraria::game {

class Minimap {
public:
    void handleInput(const input::InputState& inputState,
                     const world::World& world,
                     const entities::Player& player,
                     bool allowZoom);
    void update(float dt, const input::InputState& inputState, const world::World& world);
    void fillHud(rendering::HudState& hud) const;
    bool isFullscreen() const { return fullscreen_; }

private:
    void clampCenter(const world::World& world);
    float fitZoom(const world::World& world) const;

    float zoom_{1.0F};
    bool fullscreen_{false};
    float centerX_{0.0F};
    float centerY_{0.0F};
};

} // namespace terraria::game
