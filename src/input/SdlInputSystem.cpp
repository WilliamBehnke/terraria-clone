#include "terraria/input/InputSystem.h"

#include <SDL.h>

#include <stdexcept>
#include <string>

namespace terraria::input {

namespace {

class SdlInputSystem final : public IInputSystem {
public:
    void initialize() override {
        if ((SDL_WasInit(SDL_INIT_EVENTS) & SDL_INIT_EVENTS) == 0) {
            if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
                throw std::runtime_error(std::string("Failed to init SDL events: ") + SDL_GetError());
            }
        }
        SDL_StartTextInput();
    }

    void poll() override {
        state_.moveX = 0.0F;
        state_.hotbarSelection = -1;
        state_.toggleCamera = false;
        state_.camMoveX = 0.0F;
        state_.camMoveY = 0.0F;
        state_.craftPrev = false;
        state_.craftNext = false;
        state_.craftExecute = false;
        state_.inventoryToggle = false;
        state_.inventoryClick = false;
        state_.inventoryRightClick = false;
        state_.consoleToggle = false;
        state_.consoleSlash = false;
        state_.consoleSubmit = false;
        state_.consoleBackspace = false;
        state_.consoleLeft = false;
        state_.consoleRight = false;
        state_.textInput.clear();
        SDL_Event event{};
        const Uint8* keyboard = SDL_GetKeyboardState(nullptr);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit_ = true;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                quit_ = true;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_1: state_.hotbarSelection = 0; break;
                case SDLK_2: state_.hotbarSelection = 1; break;
                case SDLK_3: state_.hotbarSelection = 2; break;
                case SDLK_4: state_.hotbarSelection = 3; break;
                case SDLK_5: state_.hotbarSelection = 4; break;
                case SDLK_6: state_.hotbarSelection = 5; break;
                case SDLK_7: state_.hotbarSelection = 6; break;
                case SDLK_8: state_.hotbarSelection = 7; break;
                case SDLK_9: state_.hotbarSelection = 8; break;
                case SDLK_e: state_.inventoryToggle = true; break;
                case SDLK_LEFTBRACKET:
                case SDLK_UP:
                    state_.craftPrev = true;
                    break;
                case SDLK_RIGHTBRACKET:
                case SDLK_DOWN:
                    state_.craftNext = true;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    state_.craftExecute = true;
                    state_.consoleSubmit = true;
                    break;
                case SDLK_BACKQUOTE:
                    state_.consoleToggle = true;
                    ignoreTextInput_ = true;
                    break;
                case SDLK_SLASH:
                    state_.consoleToggle = true;
                    state_.consoleSlash = true;
                    ignoreTextInput_ = true;
                    break;
                case SDLK_BACKSPACE:
                    state_.consoleBackspace = true;
                    break;
                case SDLK_LEFT:
                    state_.consoleLeft = true;
                    break;
                case SDLK_RIGHT:
                    state_.consoleRight = true;
                    break;
                default: break;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    state_.inventoryClick = true;
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    state_.inventoryRightClick = true;
                }
            } else if (event.type == SDL_TEXTINPUT) {
                if (ignoreTextInput_) {
                    ignoreTextInput_ = false;
                    continue;
                }
                state_.textInput += event.text.text;
            }
        }

        if (keyboard[SDL_SCANCODE_C]) {
            if (!cameraPressed_) {
                state_.toggleCamera = true;
                cameraPressed_ = true;
            }
        } else {
            cameraPressed_ = false;
        }

        if (keyboard[SDL_SCANCODE_A] || keyboard[SDL_SCANCODE_LEFT]) {
            state_.moveX -= 1.0F;
        }
        if (keyboard[SDL_SCANCODE_D] || keyboard[SDL_SCANCODE_RIGHT]) {
            state_.moveX += 1.0F;
        }
        state_.jump = keyboard[SDL_SCANCODE_SPACE] != 0;
        if (keyboard[SDL_SCANCODE_UP]) {
            state_.camMoveY -= 1.0F;
        }
        if (keyboard[SDL_SCANCODE_DOWN]) {
            state_.camMoveY += 1.0F;
        }
        if (keyboard[SDL_SCANCODE_LEFT]) {
            state_.camMoveX -= 1.0F;
        }
        if (keyboard[SDL_SCANCODE_RIGHT]) {
            state_.camMoveX += 1.0F;
        }

        const Uint32 buttons = SDL_GetMouseState(&state_.mouseX, &state_.mouseY);
        state_.breakHeld = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        state_.placeHeld = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    }

    bool shouldQuit() const override { return quit_; }
    const InputState& state() const override { return state_; }

    void shutdown() override {
        SDL_StopTextInput();
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }

private:
    InputState state_{};
    bool quit_{false};
    bool cameraPressed_{false};
    bool ignoreTextInput_{false};
};

} // namespace

std::unique_ptr<IInputSystem> CreateSdlInputSystem() {
    return std::make_unique<SdlInputSystem>();
}

} // namespace terraria::input
