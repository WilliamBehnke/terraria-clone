#include "terraria/rendering/Renderer.h"

#include "terraria/core/Application.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace terraria::rendering {

namespace {

constexpr int kTilePixels = 8;
constexpr float kTwoPi = 6.28318530718F;

SDL_Color TileColor(world::TileType type) {
    switch (type) {
    case world::TileType::Dirt: return SDL_Color{126, 86, 59, 255};
    case world::TileType::Stone: return SDL_Color{100, 100, 100, 255};
    case world::TileType::Grass: return SDL_Color{50, 160, 60, 255};
    case world::TileType::CopperOre: return SDL_Color{220, 140, 80, 255};
    case world::TileType::IronOre: return SDL_Color{150, 165, 185, 255};
    case world::TileType::GoldOre: return SDL_Color{252, 205, 40, 255};
    case world::TileType::Wood: return SDL_Color{120, 82, 50, 255};
    case world::TileType::Leaves: return SDL_Color{60, 180, 90, 220};
    case world::TileType::TreeTrunk: return SDL_Color{130, 90, 55, 200};
    case world::TileType::TreeLeaves: return SDL_Color{70, 190, 100, 180};
    case world::TileType::Air:
    default: return SDL_Color{0, 0, 0, 0};
    }
}

SDL_Color SkyColor(const HudState& hud) {
    const float progress = std::clamp(hud.dayProgress, 0.0F, 1.0F);
    const float daylight = 0.5F - 0.5F * std::cos(progress * kTwoPi);
    const float shade = std::clamp(daylight * 0.85F + (hud.isNight ? 0.05F : 0.15F), 0.05F, 1.0F);
    const int r = static_cast<int>(10.0F + 80.0F * shade);
    const int g = static_cast<int>(18.0F + 110.0F * shade);
    const int b = static_cast<int>(28.0F + 140.0F * shade);
    return SDL_Color{static_cast<Uint8>(std::clamp(r, 0, 255)),
                     static_cast<Uint8>(std::clamp(g, 0, 255)),
                     static_cast<Uint8>(std::clamp(b, 0, 255)),
                     255};
}

class SdlRenderer final : public IRenderer {
public:
    explicit SdlRenderer(core::AppConfig config) : config_{std::move(config)} {}

    void initialize() override {
        if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0) {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
                throw std::runtime_error(std::string("Failed to init SDL: ") + SDL_GetError());
            }
        }

        window_ = SDL_CreateWindow(config_.windowTitle.c_str(),
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   config_.windowWidth,
                                   config_.windowHeight,
                                   SDL_WINDOW_SHOWN);
        if (!window_) {
            throw std::runtime_error(std::string("Failed to create window: ") + SDL_GetError());
        }

        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            throw std::runtime_error(std::string("Failed to create renderer: ") + SDL_GetError());
        }

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    }

    void render(const world::World& world,
                const entities::Player& player,
                const std::vector<std::unique_ptr<entities::Zombie>>& zombies,
                const HudState& hud) override {
        const SDL_Color sky = SkyColor(hud);
        SDL_SetRenderDrawColor(renderer_, sky.r, sky.g, sky.b, sky.a);
        SDL_RenderClear(renderer_);

        const int tilesWide = config_.windowWidth / kTilePixels + 2;
        const int tilesTall = config_.windowHeight / kTilePixels + 2;

        const int maxStartX = std::max(world.width() - tilesWide, 0);
        const int maxStartY = std::max(world.height() - tilesTall, 0);
        float focusX = hud.useCamera ? hud.cameraX : player.position().x;
        float focusY = hud.useCamera ? hud.cameraY : player.position().y;
        float viewX = focusX - static_cast<float>(tilesWide) / 2.0F;
        float viewY = focusY - static_cast<float>(tilesTall) / 2.0F;
        viewX = std::clamp(viewX, 0.0F, static_cast<float>(maxStartX));
        viewY = std::clamp(viewY, 0.0F, static_cast<float>(maxStartY));
        const int startX = static_cast<int>(std::floor(viewX));
        const int startY = static_cast<int>(std::floor(viewY));
        const float subTileOffsetX = viewX - static_cast<float>(startX);
        const float subTileOffsetY = viewY - static_cast<float>(startY);
        const int pixelOffsetX = static_cast<int>(std::floor(-subTileOffsetX * kTilePixels));
        const int pixelOffsetY = static_cast<int>(std::floor(-subTileOffsetY * kTilePixels));

        for (int y = 0; y < tilesTall && (startY + y) < world.height(); ++y) {
            for (int x = 0; x < tilesWide && (startX + x) < world.width(); ++x) {
                const auto& tile = world.tile(startX + x, startY + y);
                if (!tile.active || tile.type == world::TileType::Air) {
                    continue;
                }

                const SDL_Color color = TileColor(tile.type);
                SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
                SDL_Rect rect{ pixelOffsetX + x * kTilePixels, pixelOffsetY + y * kTilePixels, kTilePixels, kTilePixels };
                SDL_RenderFillRect(renderer_, &rect);
            }
        }

        if (hud.cursorHighlight && hud.cursorTileX >= startX && hud.cursorTileX < startX + tilesWide && hud.cursorTileY >= startY
            && hud.cursorTileY < startY + tilesTall) {
            const int screenX = pixelOffsetX + (hud.cursorTileX - startX) * kTilePixels;
            const int screenY = pixelOffsetY + (hud.cursorTileY - startY) * kTilePixels;
            if (hud.cursorCanPlace) {
                SDL_SetRenderDrawColor(renderer_, 80, 200, 120, 120);
            } else {
                SDL_SetRenderDrawColor(renderer_, 220, 80, 80, 120);
            }
            SDL_Rect highlight{screenX, screenY, kTilePixels, kTilePixels};
            SDL_RenderFillRect(renderer_, &highlight);
            SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 200);
            SDL_RenderDrawRect(renderer_, &highlight);
        }

        if (hud.breakActive) {
            if (hud.breakTileX >= startX && hud.breakTileX < startX + tilesWide && hud.breakTileY >= startY
                && hud.breakTileY < startY + tilesTall) {
                const int screenX = pixelOffsetX + (hud.breakTileX - startX) * kTilePixels;
                const int screenY = pixelOffsetY + (hud.breakTileY - startY) * kTilePixels;
                const Uint8 alpha = static_cast<Uint8>(80 + 150 * std::clamp(hud.breakProgress, 0.0F, 1.0F));
                SDL_SetRenderDrawColor(renderer_, 255, 255, 255, alpha);
                SDL_Rect overlay{screenX, screenY, kTilePixels, kTilePixels};
                SDL_RenderFillRect(renderer_, &overlay);
                SDL_SetRenderDrawColor(renderer_, 30, 30, 30, 220);
                const int crackSteps = 3;
                for (int i = 1; i <= crackSteps; ++i) {
                    const float t = std::clamp(hud.breakProgress, 0.0F, 1.0F) * static_cast<float>(i) / static_cast<float>(crackSteps);
                    const int offset = static_cast<int>(t * (kTilePixels / 2));
                    SDL_RenderDrawLine(renderer_, screenX, screenY + offset, screenX + offset, screenY + kTilePixels);
                    SDL_RenderDrawLine(renderer_, screenX + kTilePixels, screenY + offset, screenX + kTilePixels - offset, screenY + kTilePixels);
                }
            }
        }

        drawZombies(zombies, startX, startY, tilesWide, tilesTall, pixelOffsetX, pixelOffsetY);

        drawPlayer(player, startX, startY, pixelOffsetX, pixelOffsetY);
        drawStatusWidgets(hud);
        drawInventoryOverlay(player, hud);

        SDL_RenderPresent(renderer_);
    }

    void shutdown() override {
        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        SDL_Quit();
    }

private:
    core::AppConfig config_;
    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};

    void drawPlayer(const entities::Player& player, int startX, int startY, int pixelOffsetX, int pixelOffsetY) {
        const float playerPixelWidth = entities::kPlayerHalfWidth * 2.0F * static_cast<float>(kTilePixels);
        const float playerPixelHeight = entities::kPlayerHeight * static_cast<float>(kTilePixels);
        const float playerPixelX = pixelOffsetX + (player.position().x - static_cast<float>(startX)) * static_cast<float>(kTilePixels);
        const float playerPixelBottom =
            pixelOffsetY + (player.position().y - static_cast<float>(startY)) * static_cast<float>(kTilePixels);
        const int rectWidth = std::max(2, static_cast<int>(std::round(playerPixelWidth)));
        const int rectHeight = std::max(2, static_cast<int>(std::round(playerPixelHeight)));
        const int rectX = static_cast<int>(std::round(playerPixelX - playerPixelWidth / 2.0F));
        const int rectY = static_cast<int>(std::round(playerPixelBottom - playerPixelHeight));

        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 200);
        SDL_Rect outlineRect{rectX - 1, rectY - 1, rectWidth + 2, rectHeight + 2};
        SDL_RenderFillRect(renderer_, &outlineRect);

        SDL_SetRenderDrawColor(renderer_, 250, 240, 230, 255);
        SDL_Rect playerRect{rectX, rectY, rectWidth, rectHeight};
        SDL_RenderFillRect(renderer_, &playerRect);
    }

    void drawZombies(const std::vector<std::unique_ptr<entities::Zombie>>& zombies,
                     int startX,
                     int startY,
                     int tilesWide,
                     int tilesTall,
                     int pixelOffsetX,
                     int pixelOffsetY) {
        for (const auto& zombiePtr : zombies) {
            if (!zombiePtr) {
                continue;
            }
            const auto& zombie = *zombiePtr;
            const float zombieLeft = zombie.position.x - entities::kZombieHalfWidth;
            const float zombieRight = zombie.position.x + entities::kZombieHalfWidth;
            const float zombieTop = zombie.position.y - entities::kZombieHeight;
            const float zombieBottom = zombie.position.y;
            if (zombieRight < static_cast<float>(startX) || zombieLeft > static_cast<float>(startX + tilesWide)) {
                continue;
            }
            if (zombieBottom < static_cast<float>(startY) || zombieTop > static_cast<float>(startY + tilesTall)) {
                continue;
            }

            const float pixelWidth = entities::kZombieHalfWidth * 2.0F * static_cast<float>(kTilePixels);
            const float pixelHeight = entities::kZombieHeight * static_cast<float>(kTilePixels);
            const float pixelX = pixelOffsetX + (zombie.position.x - static_cast<float>(startX)) * static_cast<float>(kTilePixels);
            const float pixelBottom = pixelOffsetY + (zombie.position.y - static_cast<float>(startY)) * static_cast<float>(kTilePixels);
            const int rectWidth = std::max(2, static_cast<int>(std::round(pixelWidth)));
            const int rectHeight = std::max(2, static_cast<int>(std::round(pixelHeight)));
            const int rectX = static_cast<int>(std::round(pixelX - pixelWidth / 2.0F));
            const int rectY = static_cast<int>(std::round(pixelBottom - pixelHeight));

            SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 180);
            SDL_Rect outline{rectX - 1, rectY - 1, rectWidth + 2, rectHeight + 2};
            SDL_RenderFillRect(renderer_, &outline);

            SDL_SetRenderDrawColor(renderer_, 100, 180, 120, 255);
            SDL_Rect zombieRect{rectX, rectY, rectWidth, rectHeight};
            SDL_RenderFillRect(renderer_, &zombieRect);

            const float ratio = std::clamp(zombie.health / 35.0F, 0.0F, 1.0F);
            SDL_SetRenderDrawColor(renderer_, 50, 180, 90, 220);
            SDL_Rect barBg{rectX, rectY - 4, rectWidth, 3};
            SDL_RenderFillRect(renderer_, &barBg);
            SDL_SetRenderDrawColor(renderer_, 200, 60, 60, 240);
            SDL_Rect hpBar{rectX, rectY - 4, static_cast<int>(rectWidth * ratio), 3};
            SDL_RenderFillRect(renderer_, &hpBar);
        }
    }

    void drawStatusWidgets(const HudState& hud) {
        const int barWidth = 220;
        const int barHeight = 16;
        const int margin = 10;
        SDL_Rect bg{margin, margin, barWidth, barHeight};
        SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 220);
        SDL_RenderFillRect(renderer_, &bg);
        SDL_SetRenderDrawColor(renderer_, 90, 90, 90, 255);
        SDL_RenderDrawRect(renderer_, &bg);

        float ratio = 0.0F;
        if (hud.playerMaxHealth > 0) {
            ratio = std::clamp(static_cast<float>(hud.playerHealth) / static_cast<float>(hud.playerMaxHealth), 0.0F, 1.0F);
        }
        SDL_SetRenderDrawColor(renderer_, 200, 70, 70, 255);
        SDL_Rect fill{margin + 2, margin + 2, static_cast<int>((barWidth - 4) * ratio), barHeight - 4};
        SDL_RenderFillRect(renderer_, &fill);

        SDL_Color textColor{230, 230, 230, 255};
        drawNumber(std::to_string(std::max(0, hud.playerHealth)), margin + barWidth + 8, margin + 2, 3, textColor);

        SDL_Rect zombiePanel{margin, margin + barHeight + 8, 80, 20};
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 200);
        SDL_RenderFillRect(renderer_, &zombiePanel);
        SDL_SetRenderDrawColor(renderer_, 70, 70, 70, 255);
        SDL_RenderDrawRect(renderer_, &zombiePanel);
        drawNumber(std::to_string(std::max(0, hud.zombieCount)), zombiePanel.x + 8, zombiePanel.y + 4, 2, SDL_Color{160, 220, 170, 255});

        const int cycleWidth = 150;
        const int cycleHeight = 12;
        const int cycleX = config_.windowWidth - cycleWidth - margin;
        const int cycleY = margin;
        SDL_Rect cycleBg{cycleX, cycleY, cycleWidth, cycleHeight};
        SDL_SetRenderDrawColor(renderer_, 18, 18, 18, 220);
        SDL_RenderFillRect(renderer_, &cycleBg);
        SDL_SetRenderDrawColor(renderer_, 70, 70, 70, 255);
        SDL_RenderDrawRect(renderer_, &cycleBg);

        const float clampedProgress = std::clamp(hud.dayProgress, 0.0F, 1.0F);
        const int indicatorX = cycleX + static_cast<int>(clampedProgress * static_cast<float>(cycleWidth - 10));
        SDL_Color indicatorColor = hud.isNight ? SDL_Color{150, 190, 255, 255} : SDL_Color{255, 220, 120, 255};
        SDL_SetRenderDrawColor(renderer_, indicatorColor.r, indicatorColor.g, indicatorColor.b, indicatorColor.a);
        SDL_Rect indicator{indicatorX, cycleY + 2, 10, cycleHeight - 4};
        SDL_RenderFillRect(renderer_, &indicator);
        SDL_Rect icon{cycleX - 18, cycleY - 2, 12, 12};
        SDL_SetRenderDrawColor(renderer_, indicatorColor.r, indicatorColor.g, indicatorColor.b, indicatorColor.a);
        SDL_RenderFillRect(renderer_, &icon);
    }
    void drawInventoryOverlay(const entities::Player& player, const HudState& hud) {
        if (hud.hotbarCount <= 0) {
            return;
        }

        const auto& inventory = player.inventory();
        const int slotWidth = 90;
        const int slotHeight = 44;
        const int margin = 10;
        int x = margin;
        const int y = config_.windowHeight - slotHeight - margin;

        SDL_Color textColor{220, 220, 220, 255};
        for (int slot = 0; slot < hud.hotbarCount; ++slot) {
            const world::TileType type = hud.hotbarTypes[static_cast<std::size_t>(slot)];
            SDL_Rect panel{x, y, slotWidth, slotHeight};
            if (slot == hud.selectedSlot) {
                SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 230);
                SDL_RenderFillRect(renderer_, &panel);
                SDL_SetRenderDrawColor(renderer_, 200, 190, 140, 255);
            } else {
                SDL_SetRenderDrawColor(renderer_, 5, 5, 5, 220);
                SDL_RenderFillRect(renderer_, &panel);
                SDL_SetRenderDrawColor(renderer_, 60, 60, 60, 255);
            }
            SDL_RenderDrawRect(renderer_, &panel);

            const SDL_Color tileColor = TileColor(type);
            SDL_SetRenderDrawColor(renderer_, tileColor.r, tileColor.g, tileColor.b, tileColor.a);
            SDL_Rect swatch{x + 6, y + 6, 28, slotHeight - 12};
            SDL_RenderFillRect(renderer_, &swatch);

            const auto index = static_cast<std::size_t>(type);
            const int count = (index < inventory.size()) ? inventory[index] : 0;
            const int barMaxWidth = slotWidth - 48;
            const float ratio = std::clamp(count / 50.0F, 0.0F, 1.0F);
            SDL_SetRenderDrawColor(renderer_, tileColor.r, tileColor.g, tileColor.b, 230);
            SDL_Rect bar{x + 40, y + slotHeight - 12, static_cast<int>(barMaxWidth * ratio), 6};
            SDL_RenderFillRect(renderer_, &bar);

            drawNumber(std::to_string(count), x + 40, y + 10, 3, textColor);
            x += slotWidth + margin;
        }
    }

    void drawNumber(const std::string& text, int x, int y, int scale, SDL_Color color) {
        int cursorX = x;
        for (char c : text) {
            if (c == '-') {
                drawDash(cursorX, y + 2 * scale, scale, color);
                cursorX += 4 * scale;
            } else {
                drawDigit(c, cursorX, y, scale, color);
                cursorX += 4 * scale;
            }
        }
    }

    void drawDash(int x, int y, int scale, SDL_Color color) {
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        SDL_Rect rect{x, y, 3 * scale, scale};
        SDL_RenderFillRect(renderer_, &rect);
    }

    void drawDigit(char c, int x, int y, int scale, SDL_Color color) {
        if (c < '0' || c > '9') {
            return;
        }
        static constexpr const char* glyphs[10][5] = {
            {"111", "101", "101", "101", "111"}, // 0
            {"010", "110", "010", "010", "111"}, // 1
            {"111", "001", "111", "100", "111"}, // 2
            {"111", "001", "111", "001", "111"}, // 3
            {"101", "101", "111", "001", "001"}, // 4
            {"111", "100", "111", "001", "111"}, // 5
            {"111", "100", "111", "101", "111"}, // 6
            {"111", "001", "010", "010", "010"}, // 7
            {"111", "101", "111", "101", "111"}, // 8
            {"111", "101", "111", "001", "111"}  // 9
        };

        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        const int digitIndex = c - '0';
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (glyphs[digitIndex][row][col] == '1') {
                    SDL_Rect rect{
                        x + col * scale,
                        y + row * scale,
                        scale,
                        scale
                    };
                    SDL_RenderFillRect(renderer_, &rect);
                }
            }
        }
    }
};

} // namespace

std::unique_ptr<IRenderer> CreateSdlRenderer(const core::AppConfig& config) {
    return std::make_unique<SdlRenderer>(config);
}

} // namespace terraria::rendering
