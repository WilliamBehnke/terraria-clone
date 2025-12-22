#include "terraria/game/PhysicsSystem.h"

#include <cmath>

namespace terraria::game {

namespace {
constexpr float kCollisionEpsilon = 0.001F;
}

PhysicsSystem::PhysicsSystem(const world::World& world)
    : world_{world} {}

bool PhysicsSystem::isSolidTile(int x, int y) const {
    if (x < 0 || x >= world_.width() || y >= world_.height()) {
        return true;
    }
    if (y < 0) {
        return false;
    }
    const auto& tile = world_.tile(x, y);
    return tile.isSolid();
}

bool PhysicsSystem::collidesAabb(const entities::Vec2& pos, float halfWidth, float height) const {
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

bool PhysicsSystem::aabbOverlap(const entities::Vec2& aPos,
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

void PhysicsSystem::resolveHorizontalAabb(entities::Vec2& position,
                                          entities::Vec2& velocity,
                                          float halfWidth,
                                          float height) const {
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

void PhysicsSystem::resolveVerticalAabb(entities::Vec2& position,
                                        entities::Vec2& velocity,
                                        float halfWidth,
                                        float height,
                                        bool& onGround) const {
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

bool PhysicsSystem::solidAtPosition(const entities::Vec2& pos) const {
    const int tileX = static_cast<int>(std::floor(pos.x));
    const int tileY = static_cast<int>(std::floor(pos.y));
    return isSolidTile(tileX, tileY);
}

} // namespace terraria::game
