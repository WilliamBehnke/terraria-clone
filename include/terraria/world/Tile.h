#pragma once

#include <cstdint>
#include <memory>

namespace terraria::world {

enum class TileType : std::uint8_t {
    Air,
    Dirt,
    Stone,
    Grass,
    CopperOre,
    IronOre,
    GoldOre,
    Arrow,
    Coin,
    Wood,
    Leaves,
    WoodPlank,
    StoneBrick,
    TreeTrunk,
    TreeLeaves
};

class Tile {
public:
    Tile(TileType type, bool active);
    virtual ~Tile() = default;

    TileType type() const { return type_; }
    bool active() const { return active_; }
    void setActive(bool active) { active_ = active; }

    virtual bool isSolid() const = 0;
    virtual TileType dropType() const { return type_; }

protected:
    TileType type_;
    bool active_;
};

inline Tile::Tile(TileType type, bool active)
    : type_{type},
      active_{active} {}

class SolidTile : public Tile {
public:
    SolidTile(TileType type, bool active)
        : Tile(type, active) {}

    bool isSolid() const override { return active(); }
};

class PassableTile : public Tile {
public:
    PassableTile(TileType type, bool active)
        : Tile(type, active) {}

    bool isSolid() const override { return false; }
};

class TreeTrunkTile : public PassableTile {
public:
    explicit TreeTrunkTile(bool active)
        : PassableTile(TileType::TreeTrunk, active) {}

    TileType dropType() const override { return TileType::Wood; }
};

class TreeLeavesTile : public PassableTile {
public:
    explicit TreeLeavesTile(bool active)
        : PassableTile(TileType::TreeLeaves, active) {}

    TileType dropType() const override { return TileType::Leaves; }
};

std::unique_ptr<Tile> MakeTile(TileType type, bool active);

} // namespace terraria::world
