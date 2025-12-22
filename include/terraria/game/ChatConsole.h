#pragma once

#include "terraria/input/InputSystem.h"
#include "terraria/rendering/HudState.h"

#include <functional>
#include <string>
#include <vector>

namespace terraria::game {

class ChatConsole {
public:
    void toggle();
    void close();
    bool isOpen() const;
    void handleInput(const input::InputState& state,
                     const std::function<void(const std::string&)>& onCommand,
                     const std::function<void(const std::string&)>& onChat);
    void update(float dt);
    void addMessage(const std::string& text, bool isSystem);
    void fillHud(rendering::HudState& hud) const;

private:
    std::string sanitize(const std::string& text) const;

    bool open_{false};
    std::string input_{};
    std::size_t cursor_{0};
    std::vector<rendering::ChatLineHud> log_{};
};

} // namespace terraria::game
