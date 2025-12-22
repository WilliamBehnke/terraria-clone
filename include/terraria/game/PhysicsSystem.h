#pragma once

#include "terraria/entities/Vec2.h"
#include "terraria/world/World.h"

namespace terraria::game {

class PhysicsSystem {
public:
    explicit PhysicsSystem(const world::World& world);

    bool isSolidTile(int x, int y) const;
    bool collidesAabb(const entities::Vec2& pos, float halfWidth, float height) const;
    bool aabbOverlap(const entities::Vec2& aPos,
                     float aHalfWidth,
                     float aHeight,
                     const entities::Vec2& bPos,
                     float bHalfWidth,
                     float bHeight) const;
    void resolveHorizontalAabb(entities::Vec2& position,
                               entities::Vec2& velocity,
                               float halfWidth,
                               float height) const;
    void resolveVerticalAabb(entities::Vec2& position,
                             entities::Vec2& velocity,
                             float halfWidth,
                             float height,
                             bool& onGround) const;
    bool solidAtPosition(const entities::Vec2& pos) const;

private:
    const world::World& world_;
};

} // namespace terraria::game
