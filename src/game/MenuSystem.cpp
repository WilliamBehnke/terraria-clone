#include "terraria/game/MenuSystem.h"

#include "terraria/entities/Tools.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>

namespace terraria::game {

namespace {

std::string defaultCharacterName(int index) {
    return "PLAYER_" + std::to_string(index);
}

std::string defaultWorldName(int index) {
    return "WORLD_" + std::to_string(index);
}

std::string toUpperLabel(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return value;
}

} // namespace

void MenuSystem::openPause() {
    screen_ = Screen::Pause;
    pauseSelection_ = 0;
}

void MenuSystem::resumeGameplay() {
    screen_ = Screen::Gameplay;
}

void MenuSystem::handleTextEdit(const input::InputState& inputState, bool digitsOnly, std::size_t maxLen) {
    if (!inputState.textInput.empty()) {
        for (char c : inputState.textInput) {
            if (editText_.size() >= maxLen) {
                break;
            }
            if (digitsOnly && (c < '0' || c > '9')) {
                continue;
            }
            if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) {
                continue;
            }
            editText_.insert(editCursor_, 1, c);
            editCursor_ += 1;
        }
    }
    if (inputState.consoleBackspace && editCursor_ > 0) {
        editText_.erase(editCursor_ - 1, 1);
        editCursor_ -= 1;
    }
    if (inputState.consoleLeft && editCursor_ > 0) {
        editCursor_ -= 1;
    }
    if (inputState.consoleRight && editCursor_ < editText_.size()) {
        editCursor_ += 1;
    }
}

void MenuSystem::clampSelection(int& selection, int maxCount) const {
    if (maxCount <= 0) {
        selection = 0;
        return;
    }
    if (selection < 0) {
        selection = maxCount - 1;
    } else if (selection >= maxCount) {
        selection = 0;
    }
}

void MenuSystem::handleInput(const input::InputState& inputState, const MenuData& data, MenuAction& action) {
    action = {};
    static const std::vector<CharacterInfo> emptyCharacters{};
    static const std::vector<WorldInfo> emptyWorlds{};
    const auto& characters = data.characters ? *data.characters : emptyCharacters;
    const auto& worlds = data.worlds ? *data.worlds : emptyWorlds;

    if (editActive_) {
        const bool digitsOnly = editTarget_ == EditTarget::WorldSeed;
        handleTextEdit(inputState, digitsOnly, digitsOnly ? 12U : 16U);
        if (inputState.menuSelect || inputState.menuBack) {
            if (editTarget_ == EditTarget::CharacterName) {
                pendingCharacterName_ = editText_;
            } else if (editTarget_ == EditTarget::WorldName) {
                pendingWorldName_ = editText_;
            } else if (editTarget_ == EditTarget::WorldSeed) {
                pendingWorldSeed_ = editText_;
            }
            editActive_ = false;
            editTarget_ = EditTarget::None;
        }
        return;
    }

    if (screen_ == Screen::Pause) {
        const int entryCount = 4;
        if (inputState.menuUp) {
            pauseSelection_ -= 1;
        }
        if (inputState.menuDown) {
            pauseSelection_ += 1;
        }
        clampSelection(pauseSelection_, entryCount);
        if (inputState.menuBack) {
            action.type = MenuAction::Type::Resume;
            resumeGameplay();
            return;
        }
        if (inputState.menuSelect) {
            switch (pauseSelection_) {
            case 0:
                action.type = MenuAction::Type::Resume;
                resumeGameplay();
                return;
            case 1:
                action.type = MenuAction::Type::Save;
                return;
            case 2:
                action.type = MenuAction::Type::SaveTitle;
                return;
            case 3:
                action.type = MenuAction::Type::SaveExit;
                return;
            default:
                return;
            }
        }
        return;
    }

    if (screen_ == Screen::CharacterSelect) {
        const int totalEntries = static_cast<int>(characters.size()) + 1;
        if (inputState.menuUp) {
            characterSelection_ -= 1;
        }
        if (inputState.menuDown) {
            characterSelection_ += 1;
        }
        clampSelection(characterSelection_, totalEntries);
        if (inputState.menuSelect) {
            if (characterSelection_ == static_cast<int>(characters.size())) {
                pendingCharacterName_ = defaultCharacterName(static_cast<int>(characters.size()) + 1);
                characterCreateSelection_ = 0;
                editActive_ = true;
                editTarget_ = EditTarget::CharacterName;
                editText_ = pendingCharacterName_;
                editCursor_ = editText_.size();
                screen_ = Screen::CharacterCreate;
                return;
            }
            if (!characters.empty() && characterSelection_ >= 0
                && characterSelection_ < static_cast<int>(characters.size())) {
                screen_ = Screen::WorldSelect;
            }
        }
        return;
    }

    if (screen_ == Screen::CharacterCreate) {
        if (inputState.menuUp) {
            characterCreateSelection_ -= 1;
        }
        if (inputState.menuDown) {
            characterCreateSelection_ += 1;
        }
        clampSelection(characterCreateSelection_, 3);
        if (inputState.menuBack) {
            screen_ = Screen::CharacterSelect;
            return;
        }
        if (inputState.menuSelect) {
            if (characterCreateSelection_ == 0) {
                editActive_ = true;
                editTarget_ = EditTarget::CharacterName;
                if (pendingCharacterName_.empty()) {
                    pendingCharacterName_ = defaultCharacterName(static_cast<int>(characters.size()) + 1);
                }
                editText_ = pendingCharacterName_;
                editCursor_ = editText_.size();
                return;
            }
            if (characterCreateSelection_ == 1) {
                action.type = MenuAction::Type::CreateCharacter;
                action.name = pendingCharacterName_.empty()
                    ? defaultCharacterName(static_cast<int>(characters.size()) + 1)
                    : pendingCharacterName_;
                screen_ = Screen::WorldSelect;
                return;
            }
            if (characterCreateSelection_ == 2) {
                screen_ = Screen::CharacterSelect;
                return;
            }
        }
        return;
    }

    if (screen_ == Screen::WorldSelect) {
        const int totalEntries = static_cast<int>(worlds.size()) + 1;
        if (inputState.menuUp) {
            worldSelection_ -= 1;
        }
        if (inputState.menuDown) {
            worldSelection_ += 1;
        }
        clampSelection(worldSelection_, totalEntries);
        if (inputState.menuBack) {
            screen_ = Screen::CharacterSelect;
            return;
        }
        if (inputState.menuSelect) {
            if (worldSelection_ == static_cast<int>(worlds.size())) {
                pendingWorldName_ = defaultWorldName(static_cast<int>(worlds.size()) + 1);
                pendingWorldSeed_.clear();
                pendingTerrainIndex_ = 1;
                pendingSoilIndex_ = 1;
                pendingCavesIndex_ = 1;
                pendingOresIndex_ = 1;
                pendingTreesIndex_ = 1;
                worldCreateSelection_ = 0;
                editActive_ = true;
                editTarget_ = EditTarget::WorldName;
                editText_ = pendingWorldName_;
                editCursor_ = editText_.size();
                screen_ = Screen::WorldCreate;
                return;
            }
            if (!worlds.empty() && worldSelection_ >= 0
                && worldSelection_ < static_cast<int>(worlds.size())) {
                action.type = MenuAction::Type::StartSession;
                action.characterIndex = characterSelection_;
                action.worldIndex = worldSelection_;
            }
        }
        return;
    }

    if (screen_ == Screen::WorldCreate) {
        if (inputState.menuUp) {
            worldCreateSelection_ -= 1;
        }
        if (inputState.menuDown) {
            worldCreateSelection_ += 1;
        }
        clampSelection(worldCreateSelection_, 9);
        if (inputState.menuBack) {
            screen_ = Screen::WorldSelect;
            return;
        }
        auto adjustIndex = [&](int& value, int min, int max, int delta) {
            value = std::clamp(value + delta, min, max);
        };
        const int leftDelta = inputState.consoleLeft ? -1 : 0;
        const int rightDelta = inputState.consoleRight ? 1 : 0;
        const int delta = leftDelta + rightDelta;
        if (delta != 0) {
            switch (worldCreateSelection_) {
            case 2: adjustIndex(pendingTerrainIndex_, 0, 2, delta); break;
            case 3: adjustIndex(pendingSoilIndex_, 0, 2, delta); break;
            case 4: adjustIndex(pendingCavesIndex_, 0, 2, delta); break;
            case 5: adjustIndex(pendingOresIndex_, 0, 2, delta); break;
            case 6: adjustIndex(pendingTreesIndex_, 0, 2, delta); break;
            default: break;
            }
        }
        if (inputState.menuSelect) {
            if (worldCreateSelection_ == 0) {
                editActive_ = true;
                editTarget_ = EditTarget::WorldName;
                if (pendingWorldName_.empty()) {
                    pendingWorldName_ = defaultWorldName(static_cast<int>(worlds.size()) + 1);
                }
                editText_ = pendingWorldName_;
                editCursor_ = editText_.size();
                return;
            }
            if (worldCreateSelection_ == 1) {
                editActive_ = true;
                editTarget_ = EditTarget::WorldSeed;
                editText_ = pendingWorldSeed_;
                editCursor_ = editText_.size();
                return;
            }
            if (worldCreateSelection_ == 7) {
                action.type = MenuAction::Type::CreateWorld;
                action.name = pendingWorldName_.empty()
                    ? defaultWorldName(static_cast<int>(worlds.size()) + 1)
                    : pendingWorldName_;
                action.seed = pendingWorldSeed_;
                static const std::array<float, 3> terrainValues{0.7F, 1.0F, 1.4F};
                static const std::array<float, 3> soilValues{0.8F, 1.0F, 1.3F};
                static const std::array<float, 3> caveValues{0.6F, 1.0F, 1.5F};
                static const std::array<float, 3> oreValues{0.7F, 1.0F, 1.4F};
                static const std::array<float, 3> treeValues{0.7F, 1.0F, 1.4F};
                action.genConfig.terrainAmplitude = terrainValues[static_cast<std::size_t>(pendingTerrainIndex_)];
                action.genConfig.soilDepthScale = soilValues[static_cast<std::size_t>(pendingSoilIndex_)];
                action.genConfig.caveDensity = caveValues[static_cast<std::size_t>(pendingCavesIndex_)];
                action.genConfig.oreDensity = oreValues[static_cast<std::size_t>(pendingOresIndex_)];
                action.genConfig.treeDensity = treeValues[static_cast<std::size_t>(pendingTreesIndex_)];
                screen_ = Screen::WorldSelect;
                return;
            }
            if (worldCreateSelection_ == 8) {
                screen_ = Screen::WorldSelect;
                return;
            }
        }
    }
}

void MenuSystem::fillHud(rendering::HudState& hud, const MenuData& data) const {
    static const std::vector<CharacterInfo> emptyCharacters{};
    static const std::vector<WorldInfo> emptyWorlds{};
    const auto& characters = data.characters ? *data.characters : emptyCharacters;
    const auto& worlds = data.worlds ? *data.worlds : emptyWorlds;

    hud.menuOpen = !isGameplay();
    hud.menuEntries.clear();
    hud.menuTitle.clear();
    hud.menuHint.clear();
    hud.menuDetailTitle.clear();
    hud.menuDetailLines.clear();
    hud.menuEditActive = editActive_;
    hud.menuEditLabel.clear();
    hud.menuEditValue.clear();
    hud.menuEditCursor = 0;
    hud.menuHideGameUi = hideGameUi();
    hud.menuHideWorld = hideWorld();
    hud.menuSelected = 0;

    if (screen_ == Screen::CharacterSelect) {
        hud.menuTitle = "SELECT CHARACTER";
        for (const auto& entry : characters) {
            hud.menuEntries.push_back(toUpperLabel(entry.name));
        }
        hud.menuEntries.push_back("NEW CHARACTER");
        hud.menuSelected = std::clamp(characterSelection_, 0, std::max(0, static_cast<int>(characters.size())));
        hud.menuHint = "ENTER SELECT";
        if (!characters.empty() && characterSelection_ >= 0
            && characterSelection_ < static_cast<int>(characters.size())) {
            const auto& info = characters[static_cast<std::size_t>(characterSelection_)];
            hud.menuDetailTitle = "CHARACTER";
            hud.menuDetailLines.push_back("NAME " + toUpperLabel(info.name));
            hud.menuDetailLines.push_back("HP " + std::to_string(info.health) + "/" + std::to_string(entities::kPlayerMaxHealth));
            const char* helm = entities::ArmorName(info.armor[static_cast<std::size_t>(entities::ArmorSlot::Head)]);
            const char* chest = entities::ArmorName(info.armor[static_cast<std::size_t>(entities::ArmorSlot::Body)]);
            const char* legs = entities::ArmorName(info.armor[static_cast<std::size_t>(entities::ArmorSlot::Legs)]);
            hud.menuDetailLines.push_back(std::string("HEAD ") + (helm && helm[0] ? helm : "NONE"));
            hud.menuDetailLines.push_back(std::string("BODY ") + (chest && chest[0] ? chest : "NONE"));
            hud.menuDetailLines.push_back(std::string("LEGS ") + (legs && legs[0] ? legs : "NONE"));
            const char* acc0 = entities::AccessoryName(info.accessories[0]);
            const char* acc1 = entities::AccessoryName(info.accessories[1]);
            const char* acc2 = entities::AccessoryName(info.accessories[2]);
            hud.menuDetailLines.push_back(std::string("ACC1 ") + (acc0 && acc0[0] ? acc0 : "NONE"));
            hud.menuDetailLines.push_back(std::string("ACC2 ") + (acc1 && acc1[0] ? acc1 : "NONE"));
            hud.menuDetailLines.push_back(std::string("ACC3 ") + (acc2 && acc2[0] ? acc2 : "NONE"));
        }
    } else if (screen_ == Screen::WorldSelect) {
        hud.menuTitle = "SELECT WORLD";
        for (const auto& entry : worlds) {
            hud.menuEntries.push_back(toUpperLabel(entry.name));
        }
        hud.menuEntries.push_back("NEW WORLD");
        hud.menuSelected = std::clamp(worldSelection_, 0, std::max(0, static_cast<int>(worlds.size())));
        hud.menuHint = "ENTER SELECT  ESC BACK";
        if (!worlds.empty() && worldSelection_ >= 0 && worldSelection_ < static_cast<int>(worlds.size())) {
            const auto& info = worlds[static_cast<std::size_t>(worldSelection_)];
            hud.menuDetailTitle = "WORLD";
            hud.menuDetailLines.push_back("NAME " + toUpperLabel(info.name));
            hud.menuDetailLines.push_back("SIZE " + std::to_string(info.width) + "X" + std::to_string(info.height));
            hud.menuDetailLines.push_back("SEED " + std::to_string(info.seed));
            hud.menuDetailLines.push_back("SPAWN " + std::to_string(static_cast<int>(std::round(info.spawnX)))
                                          + " " + std::to_string(static_cast<int>(std::round(info.spawnY))));
        }
    } else if (screen_ == Screen::CharacterCreate) {
        hud.menuTitle = "NEW CHARACTER";
        hud.menuEntries.push_back("NAME " + toUpperLabel(pendingCharacterName_));
        hud.menuEntries.push_back("CREATE");
        hud.menuEntries.push_back("BACK");
        hud.menuSelected = std::clamp(characterCreateSelection_, 0, 2);
        hud.menuHint = "ENTER EDIT/SELECT  ESC BACK";
    } else if (screen_ == Screen::WorldCreate) {
        static const std::array<const char*, 3> terrainLabels{"FLAT", "NORMAL", "HILLY"};
        static const std::array<const char*, 3> soilLabels{"THIN", "NORMAL", "THICK"};
        static const std::array<const char*, 3> cavesLabels{"LOW", "NORMAL", "HIGH"};
        static const std::array<const char*, 3> oresLabels{"LOW", "NORMAL", "RICH"};
        static const std::array<const char*, 3> treesLabels{"SPARSE", "NORMAL", "DENSE"};
        hud.menuTitle = "NEW WORLD";
        hud.menuEntries.push_back("NAME " + toUpperLabel(pendingWorldName_));
        hud.menuEntries.push_back("SEED " + (pendingWorldSeed_.empty() ? "RANDOM" : toUpperLabel(pendingWorldSeed_)));
        hud.menuEntries.push_back(std::string("TERRAIN ") + terrainLabels[static_cast<std::size_t>(pendingTerrainIndex_)]);
        hud.menuEntries.push_back(std::string("SOIL ") + soilLabels[static_cast<std::size_t>(pendingSoilIndex_)]);
        hud.menuEntries.push_back(std::string("CAVES ") + cavesLabels[static_cast<std::size_t>(pendingCavesIndex_)]);
        hud.menuEntries.push_back(std::string("ORES ") + oresLabels[static_cast<std::size_t>(pendingOresIndex_)]);
        hud.menuEntries.push_back(std::string("TREES ") + treesLabels[static_cast<std::size_t>(pendingTreesIndex_)]);
        hud.menuEntries.push_back("CREATE");
        hud.menuEntries.push_back("BACK");
        hud.menuSelected = std::clamp(worldCreateSelection_, 0, 8);
        hud.menuHint = "ENTER EDIT/SELECT  ESC BACK";
    } else if (screen_ == Screen::Pause) {
        hud.menuTitle = "PAUSE";
        hud.menuEntries.push_back("RESUME");
        hud.menuEntries.push_back("SAVE");
        hud.menuEntries.push_back("TITLESCREEN");
        hud.menuEntries.push_back("SAVE AND EXIT");
        hud.menuSelected = std::clamp(pauseSelection_, 0, 3);
        hud.menuHint = "ENTER SELECT  ESC BACK";
    }

    if (editActive_) {
        hud.menuEditActive = true;
        switch (editTarget_) {
        case EditTarget::CharacterName:
            hud.menuEditLabel = "NAME";
            break;
        case EditTarget::WorldName:
            hud.menuEditLabel = "NAME";
            break;
        case EditTarget::WorldSeed:
            hud.menuEditLabel = "SEED";
            break;
        default:
            break;
        }
        hud.menuEditValue = editText_;
        hud.menuEditCursor = editCursor_;
    }
}

} // namespace terraria::game
