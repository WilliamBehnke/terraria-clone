#include "terraria/game/SaveManager.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <unordered_set>

namespace terraria::game {

namespace {

constexpr std::uint16_t kSaveVersion = 6;

template <typename T>
void writeValue(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool readValue(std::istream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

void writeString(std::ostream& out, const std::string& value) {
    const std::uint16_t len = static_cast<std::uint16_t>(value.size());
    writeValue(out, len);
    out.write(value.data(), len);
}

bool readString(std::istream& in, std::string& value) {
    std::uint16_t len = 0;
    if (!readValue(in, len)) {
        return false;
    }
    value.assign(len, '\0');
    if (len == 0) {
        return true;
    }
    in.read(value.data(), len);
    return static_cast<bool>(in);
}

void writeSlot(std::ostream& out, const entities::InventorySlot& slot) {
    writeValue(out, static_cast<std::uint8_t>(slot.category));
    writeValue(out, static_cast<std::uint8_t>(slot.blockType));
    writeValue(out, static_cast<std::uint8_t>(slot.toolKind));
    writeValue(out, static_cast<std::uint8_t>(slot.toolTier));
    writeValue(out, static_cast<std::uint8_t>(slot.armorId));
    writeValue(out, static_cast<std::uint8_t>(slot.accessoryId));
    writeValue(out, static_cast<std::uint16_t>(std::max(0, slot.count)));
}

bool readSlot(std::istream& in, entities::InventorySlot& slot) {
    std::uint8_t category = 0;
    std::uint8_t blockType = 0;
    std::uint8_t toolKind = 0;
    std::uint8_t toolTier = 0;
    std::uint8_t armorId = 0;
    std::uint8_t accessoryId = 0;
    std::uint16_t count = 0;
    if (!readValue(in, category)
        || !readValue(in, blockType)
        || !readValue(in, toolKind)
        || !readValue(in, toolTier)
        || !readValue(in, armorId)
        || !readValue(in, accessoryId)
        || !readValue(in, count)) {
        return false;
    }
    slot.category = static_cast<entities::ItemCategory>(category);
    slot.blockType = static_cast<world::TileType>(blockType);
    slot.toolKind = static_cast<entities::ToolKind>(toolKind);
    slot.toolTier = static_cast<entities::ToolTier>(toolTier);
    slot.armorId = static_cast<entities::ArmorId>(armorId);
    slot.accessoryId = static_cast<entities::AccessoryId>(accessoryId);
    slot.count = static_cast<int>(count);
    if (slot.count <= 0) {
        slot.clear();
    }
    return true;
}

bool readMagic(std::istream& in, const char* expected) {
    char magic[4]{};
    in.read(magic, 4);
    return static_cast<bool>(in) && std::equal(magic, magic + 4, expected);
}

void writeMagic(std::ostream& out, const char* magic) {
    out.write(magic, 4);
}

} // namespace

SaveManager::SaveManager(std::filesystem::path basePath)
    : basePath_{std::move(basePath)} {}

void SaveManager::ensureDirectories() const {
    std::filesystem::create_directories(charactersDir());
    std::filesystem::create_directories(worldsDir());
}

std::filesystem::path SaveManager::charactersDir() const {
    return basePath_ / "characters";
}

std::filesystem::path SaveManager::worldsDir() const {
    return basePath_ / "worlds";
}

std::vector<CharacterInfo> SaveManager::listCharacters() const {
    std::vector<CharacterInfo> result;
    if (!std::filesystem::exists(charactersDir())) {
        return result;
    }
    for (const auto& entry : std::filesystem::directory_iterator(charactersDir())) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        if (path.extension() != ".char") {
            continue;
        }
        CharacterInfo info{};
        if (readCharacterHeader(path, info)) {
            result.push_back(info);
        }
    }
    std::sort(result.begin(), result.end(), [](const CharacterInfo& a, const CharacterInfo& b) { return a.name < b.name; });
    return result;
}

std::vector<WorldInfo> SaveManager::listWorlds() const {
    std::vector<WorldInfo> result;
    if (!std::filesystem::exists(worldsDir())) {
        return result;
    }
    for (const auto& entry : std::filesystem::directory_iterator(worldsDir())) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        if (path.extension() != ".world") {
            continue;
        }
        WorldInfo info{};
        if (readWorldHeader(path, info)) {
            result.push_back(info);
        }
    }
    std::sort(result.begin(), result.end(), [](const WorldInfo& a, const WorldInfo& b) { return a.name < b.name; });
    return result;
}

bool SaveManager::readCharacterHeader(const std::filesystem::path& path, CharacterInfo& info) const {
    std::ifstream in(path, std::ios::binary);
    if (!in || !readMagic(in, "CHAR")) {
        return false;
    }
    std::uint16_t version = 0;
    if (!readValue(in, version)
        || (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != kSaveVersion)) {
        return false;
    }
    if (!readString(in, info.name)) {
        return false;
    }
    float x = 0.0F;
    float y = 0.0F;
    std::int32_t health = entities::kPlayerMaxHealth;
    if (!readValue(in, x) || !readValue(in, y) || !readValue(in, health)) {
        return false;
    }
    info.health = static_cast<int>(health);
    entities::InventorySlot slot{};
    for (int i = 0; i < entities::kInventorySlots; ++i) {
        if (!readSlot(in, slot)) {
            return false;
        }
    }
    if (!readSlot(in, slot)) {
        return false;
    }
    for (int i = 0; i < static_cast<int>(entities::ArmorSlot::Count); ++i) {
        std::uint8_t armorId = 0;
        if (!readValue(in, armorId)) {
            return false;
        }
        info.armor[static_cast<std::size_t>(i)] = static_cast<entities::ArmorId>(armorId);
    }
    for (int i = 0; i < entities::kAccessorySlotCount; ++i) {
        std::uint8_t accessoryId = 0;
        if (!readValue(in, accessoryId)) {
            return false;
        }
        info.accessories[static_cast<std::size_t>(i)] = static_cast<entities::AccessoryId>(accessoryId);
    }
    info.id = path.stem().string();
    return true;
}

bool SaveManager::readWorldHeader(const std::filesystem::path& path, WorldInfo& info) const {
    std::ifstream in(path, std::ios::binary);
    if (!in || !readMagic(in, "WLD1")) {
        return false;
    }
    std::uint16_t version = 0;
    if (!readValue(in, version)
        || (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != kSaveVersion)) {
        return false;
    }
    if (!readString(in, info.name)) {
        return false;
    }
    if (!readValue(in, info.width) || !readValue(in, info.height)) {
        return false;
    }
    if (version >= 2) {
        if (!readValue(in, info.seed) || !readValue(in, info.spawnX) || !readValue(in, info.spawnY)) {
            return false;
        }
    }
    info.id = path.stem().string();
    return true;
}

bool SaveManager::saveCharacter(const std::string& id, const std::string& name, const entities::Player& player) const {
    ensureDirectories();
    const auto path = charactersDir() / (id + ".char");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    writeMagic(out, "CHAR");
    writeValue(out, kSaveVersion);
    writeString(out, name);
    writeValue(out, player.position().x);
    writeValue(out, player.position().y);
    writeValue(out, static_cast<std::int32_t>(player.health()));
    for (const auto& slot : player.inventory()) {
        writeSlot(out, slot);
    }
    writeSlot(out, player.ammoSlot());
    const auto& armorSlots = player.equippedArmor();
    for (const auto idValue : armorSlots) {
        writeValue(out, static_cast<std::uint8_t>(idValue));
    }
    for (int i = 0; i < entities::kAccessorySlotCount; ++i) {
        writeValue(out, static_cast<std::uint8_t>(player.accessoryAt(i)));
    }
    const auto& exploredMaps = player.exploredMaps();
    const std::uint16_t mapCount = static_cast<std::uint16_t>(std::min<std::size_t>(exploredMaps.size(), 65000));
    writeValue(out, mapCount);
    std::uint16_t written = 0;
    for (const auto& entry : exploredMaps) {
        if (written >= mapCount) {
            break;
        }
        writeString(out, entry.first);
        writeValue(out, static_cast<std::int32_t>(entry.second.width));
        writeValue(out, static_cast<std::int32_t>(entry.second.height));
        const auto& data = entry.second.data;
        for (std::uint8_t value : data) {
            writeValue(out, value);
        }
        ++written;
    }
    return static_cast<bool>(out);
}

bool SaveManager::loadCharacter(const std::string& id, entities::Player& player, std::string& outName) const {
    const auto path = charactersDir() / (id + ".char");
    std::ifstream in(path, std::ios::binary);
    if (!in || !readMagic(in, "CHAR")) {
        return false;
    }
    std::uint16_t version = 0;
    if (!readValue(in, version)
        || (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != kSaveVersion)) {
        return false;
    }
    if (!readString(in, outName)) {
        return false;
    }
    float x = 0.0F;
    float y = 0.0F;
    std::int32_t health = entities::kPlayerMaxHealth;
    if (!readValue(in, x) || !readValue(in, y) || !readValue(in, health)) {
        return false;
    }
    player.setPosition({x, y});
    player.setVelocity({0.0F, 0.0F});
    auto& inventory = player.inventory();
    for (auto& slot : inventory) {
        slot.clear();
        if (!readSlot(in, slot)) {
            return false;
        }
    }
    entities::InventorySlot ammo{};
    if (!readSlot(in, ammo)) {
        return false;
    }
    player.ammoSlot() = ammo;
    for (int i = 0; i < static_cast<int>(entities::ArmorSlot::Count); ++i) {
        std::uint8_t armorId = 0;
        if (!readValue(in, armorId)) {
            return false;
        }
        player.equipArmor(static_cast<entities::ArmorSlot>(i), static_cast<entities::ArmorId>(armorId));
    }
    for (int i = 0; i < entities::kAccessorySlotCount; ++i) {
        std::uint8_t accessoryId = 0;
        if (!readValue(in, accessoryId)) {
            return false;
        }
        player.equipAccessory(i, static_cast<entities::AccessoryId>(accessoryId));
    }
    player.clearExploredMaps();
    if (version == 4) {
        std::int32_t exploredWidth = 0;
        std::int32_t exploredHeight = 0;
        if (!readValue(in, exploredWidth) || !readValue(in, exploredHeight)) {
            return false;
        }
        const std::int64_t total = static_cast<std::int64_t>(exploredWidth) * exploredHeight;
        for (std::int64_t i = 0; i < total; ++i) {
            std::uint8_t exploredValue = 0;
            if (!readValue(in, exploredValue)) {
                return false;
            }
        }
    }
    if (version >= 6) {
        std::uint16_t mapCount = 0;
        if (!readValue(in, mapCount)) {
            return false;
        }
        for (std::uint16_t i = 0; i < mapCount; ++i) {
            std::string worldId{};
            if (!readString(in, worldId)) {
                return false;
            }
            std::int32_t width = 0;
            std::int32_t height = 0;
            if (!readValue(in, width) || !readValue(in, height)) {
                return false;
            }
            if (width <= 0 || height <= 0) {
                continue;
            }
            entities::Player::MapExploration map{};
            map.width = width;
            map.height = height;
            map.data.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
            for (std::size_t idx = 0; idx < map.data.size(); ++idx) {
                std::uint8_t exploredValue = 0;
                if (!readValue(in, exploredValue)) {
                    return false;
                }
                map.data[idx] = exploredValue;
            }
            player.setExploredMap(worldId, std::move(map));
        }
    }
    player.setHealth(static_cast<int>(health));
    return true;
}

bool SaveManager::saveWorld(const std::string& id,
                            const std::string& name,
                            const world::World& world,
                            std::uint32_t seed,
                            float spawnX,
                            float spawnY,
                            float timeOfDay,
                            bool isNight) const {
    ensureDirectories();
    const auto path = worldsDir() / (id + ".world");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    writeMagic(out, "WLD1");
    writeValue(out, kSaveVersion);
    writeString(out, name);
    writeValue(out, world.width());
    writeValue(out, world.height());
    writeValue(out, seed);
    writeValue(out, spawnX);
    writeValue(out, spawnY);
    writeValue(out, timeOfDay);
    writeValue(out, static_cast<std::uint8_t>(isNight ? 1 : 0));
    for (int y = 0; y < world.height(); ++y) {
        for (int x = 0; x < world.width(); ++x) {
            const auto& tile = world.tile(x, y);
            writeValue(out, static_cast<std::uint8_t>(tile.type()));
            writeValue(out, static_cast<std::uint8_t>(tile.active() ? 1 : 0));
        }
    }
    return static_cast<bool>(out);
}

bool SaveManager::loadWorld(const std::string& id,
                            world::World& world,
                            std::string& outName,
                            std::uint32_t& seed,
                            float& spawnX,
                            float& spawnY,
                            float& timeOfDay,
                            bool& isNight) const {
    const auto path = worldsDir() / (id + ".world");
    std::ifstream in(path, std::ios::binary);
    if (!in || !readMagic(in, "WLD1")) {
        return false;
    }
    std::uint16_t version = 0;
    if (!readValue(in, version)
        || (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != kSaveVersion)) {
        return false;
    }
    if (!readString(in, outName)) {
        return false;
    }
    int width = 0;
    int height = 0;
    if (!readValue(in, width) || !readValue(in, height)) {
        return false;
    }
    if (version >= 2) {
        if (!readValue(in, seed) || !readValue(in, spawnX) || !readValue(in, spawnY)) {
            return false;
        }
    } else {
        seed = 0;
        spawnX = static_cast<float>(width) * 0.5F;
        spawnY = 0.0F;
    }
    float loadedTime = 0.0F;
    std::uint8_t nightFlag = 0;
    if (!readValue(in, loadedTime) || !readValue(in, nightFlag)) {
        return false;
    }
    if (width != world.width() || height != world.height()) {
        world = world::World(width, height);
    }
    for (int y = 0; y < world.height(); ++y) {
        for (int x = 0; x < world.width(); ++x) {
            std::uint8_t typeValue = 0;
            std::uint8_t activeValue = 0;
            if (!readValue(in, typeValue) || !readValue(in, activeValue)) {
                return false;
            }
            world.setTile(x, y, static_cast<world::TileType>(typeValue), activeValue != 0);
        }
    }
    if (version == 5) {
        for (int y = 0; y < world.height(); ++y) {
            for (int x = 0; x < world.width(); ++x) {
                std::uint8_t exploredValue = 0;
                if (!readValue(in, exploredValue)) {
                    return false;
                }
            }
        }
    }
    timeOfDay = loadedTime;
    isNight = nightFlag != 0;
    return true;
}

std::string SaveManager::makeId(const std::string& prefix, int index) const {
    return prefix + std::to_string(index);
}

std::string SaveManager::createCharacterId(const std::vector<CharacterInfo>& existing) const {
    std::unordered_set<std::string> ids;
    ids.reserve(existing.size());
    for (const auto& info : existing) {
        ids.insert(info.id);
    }
    for (int i = 1; i < 10000; ++i) {
        const std::string candidate = makeId("char_", i);
        if (ids.find(candidate) == ids.end()) {
            return candidate;
        }
    }
    return makeId("char_", 9999);
}

std::string SaveManager::createWorldId(const std::vector<WorldInfo>& existing) const {
    std::unordered_set<std::string> ids;
    ids.reserve(existing.size());
    for (const auto& info : existing) {
        ids.insert(info.id);
    }
    for (int i = 1; i < 10000; ++i) {
        const std::string candidate = makeId("world_", i);
        if (ids.find(candidate) == ids.end()) {
            return candidate;
        }
    }
    return makeId("world_", 9999);
}

std::string SaveManager::defaultCharacterName(int index) const {
    return "PLAYER_" + std::to_string(index);
}

std::string SaveManager::defaultWorldName(int index) const {
    return "WORLD_" + std::to_string(index);
}

} // namespace terraria::game
