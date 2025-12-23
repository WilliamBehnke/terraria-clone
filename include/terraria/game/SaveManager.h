#pragma once

#include "terraria/entities/Player.h"
#include "terraria/world/World.h"

#include <filesystem>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace terraria::game {

struct CharacterInfo {
    std::string id{};
    std::string name{};
    int health{0};
    std::array<entities::ArmorId, static_cast<std::size_t>(entities::ArmorSlot::Count)> armor{};
    std::array<entities::AccessoryId, entities::kAccessorySlotCount> accessories{};
};

struct WorldInfo {
    std::string id{};
    std::string name{};
    int width{0};
    int height{0};
    std::uint32_t seed{0};
    float spawnX{0.0F};
    float spawnY{0.0F};
};

class SaveManager {
public:
    explicit SaveManager(std::filesystem::path basePath = "saves");

    void ensureDirectories() const;

    std::vector<CharacterInfo> listCharacters() const;
    std::vector<WorldInfo> listWorlds() const;

    bool loadCharacter(const std::string& id, entities::Player& player, std::string& outName) const;
    bool saveCharacter(const std::string& id, const std::string& name, const entities::Player& player) const;

    bool loadWorld(const std::string& id,
                   world::World& world,
                   std::string& outName,
                   std::uint32_t& seed,
                   float& spawnX,
                   float& spawnY,
                   float& timeOfDay,
                   bool& isNight) const;
    bool saveWorld(const std::string& id,
                   const std::string& name,
                   const world::World& world,
                   std::uint32_t seed,
                   float spawnX,
                   float spawnY,
                   float timeOfDay,
                   bool isNight) const;

    std::string createCharacterId(const std::vector<CharacterInfo>& existing) const;
    std::string createWorldId(const std::vector<WorldInfo>& existing) const;
    std::string defaultCharacterName(int index) const;
    std::string defaultWorldName(int index) const;

private:
    std::filesystem::path charactersDir() const;
    std::filesystem::path worldsDir() const;

    bool readCharacterHeader(const std::filesystem::path& path, CharacterInfo& info) const;
    bool readWorldHeader(const std::filesystem::path& path, WorldInfo& info) const;
    std::string makeId(const std::string& prefix, int index) const;

    std::filesystem::path basePath_;
};

} // namespace terraria::game
