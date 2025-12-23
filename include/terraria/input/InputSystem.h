#pragma once

#include <memory>
#include <string>

namespace terraria::input {

struct InputState {
    float moveX{0.0F};
    bool jump{false};
    bool breakHeld{false};
    bool placeHeld{false};
    bool toggleCamera{false};
    float camMoveX{0.0F};
    float camMoveY{0.0F};
    int mouseX{0};
    int mouseY{0};
    int hotbarSelection{-1};
    bool craftPrev{false};
    bool craftNext{false};
    bool craftExecute{false};
    bool inventoryToggle{false};
    bool inventoryClick{false};
    bool inventoryRightClick{false};
    bool consoleToggle{false};
    bool consoleSlash{false};
    bool consoleSubmit{false};
    bool consoleBackspace{false};
    bool consoleLeft{false};
    bool consoleRight{false};
    bool menuUp{false};
    bool menuDown{false};
    bool menuSelect{false};
    bool menuBack{false};
    bool minimapZoomIn{false};
    bool minimapZoomOut{false};
    bool minimapToggle{false};
    bool minimapDrag{false};
    int mouseDeltaX{0};
    int mouseDeltaY{0};
    std::string textInput{};
};

class IInputSystem {
public:
    virtual ~IInputSystem() = default;
    virtual void initialize() = 0;
    virtual void poll() = 0;
    virtual bool shouldQuit() const = 0;
    virtual const InputState& state() const = 0;
    virtual void shutdown() = 0;
};

std::unique_ptr<IInputSystem> CreateSdlInputSystem();

} // namespace terraria::input
