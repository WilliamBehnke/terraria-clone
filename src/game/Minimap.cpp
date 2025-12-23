#include "terraria/game/Minimap.h"

#include <algorithm>

namespace terraria::game {

void Minimap::handleInput(const input::InputState& inputState,
                          const world::World& world,
                          const entities::Player& player,
                          bool allowZoom) {
    if (inputState.minimapToggle) {
        fullscreen_ = !fullscreen_;
        centerX_ = player.position().x;
        centerY_ = player.position().y;
        zoom_ = std::max(zoom_, fitZoom(world));
    }

    if (!allowZoom) {
        return;
    }

    if (inputState.minimapZoomIn) {
        zoom_ = std::min(8.0F, zoom_ + 0.5F);
    }
    if (inputState.minimapZoomOut) {
        zoom_ = std::max(0.5F, zoom_ - 0.5F);
    }
    zoom_ = std::max(zoom_, fitZoom(world));
}

void Minimap::update(float dt, const input::InputState& inputState, const world::World& world) {
    if (!fullscreen_) {
        return;
    }
    const float panSpeed = 220.0F;
    centerX_ += inputState.camMoveX * panSpeed * dt;
    centerY_ += inputState.camMoveY * panSpeed * dt;
    if (inputState.minimapDrag) {
        const float dragScale = 0.16F;
        centerX_ -= static_cast<float>(inputState.mouseDeltaX) * dragScale;
        centerY_ -= static_cast<float>(inputState.mouseDeltaY) * dragScale;
    }
    clampCenter(world);
}

void Minimap::fillHud(rendering::HudState& hud) const {
    hud.minimapZoom = zoom_;
    hud.minimapFullscreen = fullscreen_;
    hud.minimapCenterX = centerX_;
    hud.minimapCenterY = centerY_;
}

float Minimap::fitZoom(const world::World& world) const {
    const float worldW = static_cast<float>(world.width());
    const float worldH = static_cast<float>(world.height());
    if (worldW <= 0.0F || worldH <= 0.0F) {
        return 1.0F;
    }
    return std::max(worldW, worldH) / std::max(1.0F, std::min(worldW, worldH));
}

void Minimap::clampCenter(const world::World& world) {
    zoom_ = std::max(zoom_, fitZoom(world));
    const float worldW = static_cast<float>(world.width());
    const float worldH = static_cast<float>(world.height());
    const float maxDim = std::max(worldW, worldH);
    const float spanX = maxDim / zoom_;
    const float spanY = maxDim / zoom_;
    const float minX = spanX * 0.5F;
    const float maxX = worldW - spanX * 0.5F;
    const float minY = spanY * 0.5F;
    const float maxY = worldH - spanY * 0.5F;
    centerX_ = (minX <= maxX) ? std::clamp(centerX_, minX, maxX) : worldW * 0.5F;
    centerY_ = (minY <= maxY) ? std::clamp(centerY_, minY, maxY) : worldH * 0.5F;
}

} // namespace terraria::game
