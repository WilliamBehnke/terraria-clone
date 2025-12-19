#pragma once

#include <memory>
#include <string>

namespace terraria::core {

struct AppConfig {
    std::string windowTitle{"Terra Clone"};
    int windowWidth{1280};
    int windowHeight{720};
    int targetFps{60};
    int worldWidth{4096};
    int worldHeight{768};
};

class Application {
public:
    Application();
    ~Application();

    void run();

private:
    void init();
    void shutdown();

    AppConfig config_{};
    bool running_{false};
};

} // namespace terraria::core
