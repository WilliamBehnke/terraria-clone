#include "terraria/game/ChatConsole.h"

#include <algorithm>
#include <cctype>

namespace terraria::game {

void ChatConsole::toggle() {
    if (open_) {
        open_ = false;
        return;
    }
    open_ = true;
    input_.clear();
    cursor_ = 0;
}

void ChatConsole::close() {
    open_ = false;
}

bool ChatConsole::isOpen() const {
    return open_;
}

void ChatConsole::handleInput(const input::InputState& state,
                              const std::function<void(const std::string&)>& onCommand,
                              const std::function<void(const std::string&)>& onChat) {
    if (!open_) {
        return;
    }
    if (state.consoleSlash) {
        input_.insert(cursor_, "/");
        cursor_ += 1;
    }
    if (!state.textInput.empty()) {
        input_.insert(cursor_, state.textInput);
        cursor_ += state.textInput.size();
    }
    if (state.consoleBackspace && cursor_ > 0 && !input_.empty()) {
        input_.erase(cursor_ - 1, 1);
        cursor_ -= 1;
    }
    if (state.consoleLeft && cursor_ > 0) {
        cursor_ -= 1;
    }
    if (state.consoleRight && cursor_ < input_.size()) {
        cursor_ += 1;
    }
    if (state.consoleSubmit) {
        const std::string trimmed = sanitize(input_);
        if (!trimmed.empty()) {
            if (!trimmed.empty() && trimmed.front() == '/') {
                onCommand(trimmed);
            } else {
                onChat(trimmed);
            }
        }
        input_.clear();
        cursor_ = 0;
    }
}

void ChatConsole::update(float dt) {
    if (open_) {
        return;
    }
    for (auto& line : log_) {
        line.age += dt;
    }
}

void ChatConsole::addMessage(const std::string& text, bool isSystem) {
    const std::string sanitized = sanitize(text);
    if (sanitized.empty()) {
        return;
    }
    if (log_.size() >= 200) {
        log_.erase(log_.begin());
    }
    log_.push_back(rendering::ChatLineHud{sanitized, isSystem, 0.0F});
}

void ChatConsole::fillHud(rendering::HudState& hud) const {
    hud.consoleOpen = open_;
    hud.consoleInput = input_;
    hud.consoleCursor = cursor_;
    hud.chatLines = log_;
}

std::string ChatConsole::sanitize(const std::string& text) const {
    std::string sanitized;
    sanitized.reserve(text.size());
    for (char c : text) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 32 && uc <= 126) {
            sanitized.push_back(static_cast<char>(c));
        }
    }
    return sanitized;
}

} // namespace terraria::game
