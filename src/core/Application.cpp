#include "terraria/core/Application.h"

#include "terraria/game/Game.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace terraria::core {

Application::Application() = default;
Application::~Application() = default;

void Application::init() {
    running_ = true;
}

void Application::shutdown() {
    running_ = false;
}

void Application::run() {
    init();

    game::Game game{config_};
    game.initialize();

    while (running_ && game.tick()) {
        std::this_thread::sleep_for(16ms); // placeholder frame limiter
    }

    game.shutdown();
    shutdown();
}

} // namespace terraria::core
