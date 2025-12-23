#pragma once

#include "terraria/game/SaveManager.h"
#include "terraria/input/InputSystem.h"
#include "terraria/rendering/HudState.h"
#include "terraria/world/WorldGenerator.h"

#include <string>
#include <vector>

namespace terraria::game {

enum class Screen {
    CharacterSelect,
    CharacterCreate,
    WorldSelect,
    WorldCreate,
    Gameplay,
    Pause
};

class MenuSystem {
public:
    struct MenuData {
        const std::vector<CharacterInfo>* characters{nullptr};
        const std::vector<WorldInfo>* worlds{nullptr};
    };

    struct MenuAction {
        enum class Type {
            None,
            Resume,
            Save,
            SaveExit,
            SaveTitle,
            StartSession,
            CreateCharacter,
            CreateWorld
        };

        Type type{Type::None};
        int characterIndex{-1};
        int worldIndex{-1};
        std::string name{};
        std::string seed{};
        world::WorldGenerator::WorldGenConfig genConfig{};
    };

    void handleInput(const input::InputState& inputState, const MenuData& data, MenuAction& action);
    void fillHud(rendering::HudState& hud, const MenuData& data) const;

    Screen screen() const { return screen_; }
    void setScreen(Screen screen) { screen_ = screen; }
    void openPause();
    void resumeGameplay();
    bool isGameplay() const { return screen_ == Screen::Gameplay; }
    bool isPause() const { return screen_ == Screen::Pause; }
    bool isMenu() const { return screen_ != Screen::Gameplay && screen_ != Screen::Pause; }
    bool hideWorld() const { return isMenu(); }
    bool hideGameUi() const { return isMenu(); }

    void setCharacterSelection(int index) { characterSelection_ = index; }
    void setWorldSelection(int index) { worldSelection_ = index; }

private:
    enum class EditTarget {
        None,
        CharacterName,
        WorldName,
        WorldSeed
    };

    void handleTextEdit(const input::InputState& inputState, bool digitsOnly, std::size_t maxLen);
    void clampSelection(int& selection, int maxCount) const;

    Screen screen_{Screen::CharacterSelect};
    int characterSelection_{0};
    int worldSelection_{0};
    int characterCreateSelection_{0};
    int worldCreateSelection_{0};
    int pauseSelection_{0};
    std::string pendingCharacterName_{};
    std::string pendingWorldName_{};
    std::string pendingWorldSeed_{};
    int pendingTerrainIndex_{1};
    int pendingSoilIndex_{1};
    int pendingCavesIndex_{1};
    int pendingOresIndex_{1};
    int pendingTreesIndex_{1};
    bool editActive_{false};
    EditTarget editTarget_{EditTarget::None};
    std::string editText_{};
    std::size_t editCursor_{0};
};

} // namespace terraria::game
