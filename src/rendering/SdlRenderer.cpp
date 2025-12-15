#include "terraria/rendering/Renderer.h"

#include "terraria/core/Application.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace terraria::rendering {

namespace {

constexpr int kTilePixels = 8;

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

    void render(const world::World& world, const entities::Player& player, const HudState& hud) override {
        SDL_SetRenderDrawColor(renderer_, 12, 20, 34, 255);
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
