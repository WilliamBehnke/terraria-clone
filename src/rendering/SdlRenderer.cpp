#include "terraria/rendering/Renderer.h"

#include "terraria/core/Application.h"
#include "terraria/entities/Tools.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace terraria::rendering {

namespace {

constexpr int kTilePixels = 16;
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
    case world::TileType::WoodPlank: return SDL_Color{180, 130, 80, 255};
    case world::TileType::StoneBrick: return SDL_Color{150, 125, 125, 255};
    case world::TileType::TreeTrunk: return SDL_Color{130, 90, 55, 200};
    case world::TileType::TreeLeaves: return SDL_Color{70, 190, 100, 180};
    case world::TileType::Air:
    default: return SDL_Color{0, 0, 0, 0};
    }
}

SDL_Color ToolColor(entities::ToolKind kind, entities::ToolTier tier) {
    switch (kind) {
    case entities::ToolKind::Pickaxe:
        switch (tier) {
        case entities::ToolTier::Wood: return SDL_Color{180, 140, 90, 255};
        case entities::ToolTier::Stone: return SDL_Color{140, 140, 160, 255};
        case entities::ToolTier::Copper: return SDL_Color{220, 140, 80, 255};
        case entities::ToolTier::Iron: return SDL_Color{170, 180, 200, 255};
        case entities::ToolTier::Gold: return SDL_Color{250, 210, 80, 255};
        default: return SDL_Color{120, 120, 120, 255};
        }
    case entities::ToolKind::Axe:
        switch (tier) {
        case entities::ToolTier::Wood: return SDL_Color{160, 110, 70, 255};
        case entities::ToolTier::Stone: return SDL_Color{130, 130, 150, 255};
        case entities::ToolTier::Copper: return SDL_Color{210, 130, 70, 255};
        case entities::ToolTier::Iron: return SDL_Color{160, 170, 190, 255};
        case entities::ToolTier::Gold: return SDL_Color{240, 200, 70, 255};
        default: return SDL_Color{110, 110, 110, 255};
        }
    case entities::ToolKind::Sword:
    default:
        switch (tier) {
        case entities::ToolTier::Wood: return SDL_Color{170, 120, 80, 255};
        case entities::ToolTier::Stone: return SDL_Color{150, 150, 170, 255};
        case entities::ToolTier::Copper: return SDL_Color{230, 150, 90, 255};
        case entities::ToolTier::Iron: return SDL_Color{190, 200, 220, 255};
        case entities::ToolTier::Gold: return SDL_Color{255, 215, 100, 255};
        default: return SDL_Color{130, 130, 130, 255};
        }
    }
}

bool TileBlendsWithDirt(world::TileType type) {
    switch (type) {
    case world::TileType::Grass:
    case world::TileType::Stone:
    case world::TileType::CopperOre:
    case world::TileType::IronOre:
    case world::TileType::GoldOre:
        return true;
    default:
        return false;
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
        loadTileTextures();
    }

    void render(const world::World& world,
                const entities::Player& player,
                const std::vector<entities::Zombie>& zombies,
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

                SDL_Rect rect{ pixelOffsetX + x * kTilePixels, pixelOffsetY + y * kTilePixels, kTilePixels, kTilePixels };
                if (const TileTexture* textureInfo = tileTexture(tile.type)) {
                    SDL_Rect src = atlasRectFor(world, tile.type, startX + x, startY + y);
                    SDL_RenderCopy(renderer_, textureInfo->texture, &src, &rect);
                } else {
                    const SDL_Color color = TileColor(tile.type);
                    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
                    SDL_RenderFillRect(renderer_, &rect);
                }
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
        drawCraftingOverlay(hud);

        SDL_RenderPresent(renderer_);
    }

    void shutdown() override {
        destroyTileTextures();
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
    struct TileTexture {
        SDL_Texture* texture{nullptr};
    };

    core::AppConfig config_;
    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    std::unordered_map<world::TileType, TileTexture> tileTextures_{};
    std::unordered_map<world::TileType, std::unordered_map<std::string, std::vector<SDL_Rect>>> tileMaskRects_{};

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

    void drawZombies(const std::vector<entities::Zombie>& zombies,
                     int startX,
                     int startY,
                     int tilesWide,
                     int tilesTall,
                     int pixelOffsetX,
                     int pixelOffsetY) {
        for (const auto& zombie : zombies) {
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
        (void)player;
        if (hud.hotbarCount <= 0) {
            return;
        }

        const int slotWidth = 90;
        const int slotHeight = 44;
        const int margin = 10;
        int x = margin;
        const int y = config_.windowHeight - slotHeight - margin;
        SDL_Color textColor{220, 220, 220, 255};
        for (int slot = 0; slot < hud.hotbarCount; ++slot) {
            const auto& slotData = hud.hotbarSlots[static_cast<std::size_t>(slot)];
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

            int count = std::max(0, slotData.count);
            if (slotData.isTool) {
                const SDL_Color toolColor = ToolColor(slotData.toolKind, slotData.toolTier);
                SDL_SetRenderDrawColor(renderer_, toolColor.r, toolColor.g, toolColor.b, toolColor.a);
                SDL_Rect swatch{x + 6, y + 6, slotWidth - 12, slotHeight - 12};
                SDL_RenderFillRect(renderer_, &swatch);
                char label = 'T';
                switch (slotData.toolKind) {
                case entities::ToolKind::Pickaxe: label = 'P'; break;
                case entities::ToolKind::Axe: label = 'A'; break;
                case entities::ToolKind::Sword: label = 'S'; break;
                }
                drawNumber(std::string(1, label), x + slotWidth / 2 - 4, y + 8, 3, SDL_Color{20, 20, 20, 230});
            } else {
                const SDL_Color tileColor = TileColor(slotData.tileType);
                SDL_SetRenderDrawColor(renderer_, tileColor.r, tileColor.g, tileColor.b, tileColor.a);
                SDL_Rect swatch{x + 6, y + 6, 28, slotHeight - 12};
                SDL_RenderFillRect(renderer_, &swatch);

                const int barMaxWidth = slotWidth - 48;
                const float ratio = std::clamp(count / 50.0F, 0.0F, 1.0F);
                SDL_SetRenderDrawColor(renderer_, tileColor.r, tileColor.g, tileColor.b, 230);
                SDL_Rect bar{x + 40, y + slotHeight - 12, static_cast<int>(barMaxWidth * ratio), 6};
                SDL_RenderFillRect(renderer_, &bar);
            }

            drawNumber(std::to_string(count), x + slotWidth - 28, y + 10, 2, textColor);
            x += slotWidth + margin;
        }
    }

    void drawCraftingOverlay(const HudState& hud) {
        if (hud.craftRecipeCount <= 0) {
            return;
        }

        const int margin = 10;
        const int panelWidth = 210;
        const int rowHeight = 26;
        const int padding = 6;
        int x = margin;
        int y = margin + 46;
        SDL_Color textColor{220, 220, 220, 255};

        for (int i = 0; i < hud.craftRecipeCount && i < kMaxCraftRecipes; ++i) {
            const auto& entry = hud.craftRecipes[static_cast<std::size_t>(i)];
            SDL_Rect row{x, y, panelWidth, rowHeight};
            if (i == hud.craftSelection) {
                SDL_SetRenderDrawColor(renderer_, 40, 40, 50, 230);
            } else {
                SDL_SetRenderDrawColor(renderer_, 15, 15, 20, 200);
            }
            SDL_RenderFillRect(renderer_, &row);
            if (entry.canCraft) {
                SDL_SetRenderDrawColor(renderer_, 90, 200, 130, 255);
            } else {
                SDL_SetRenderDrawColor(renderer_, 70, 70, 70, 255);
            }
            SDL_RenderDrawRect(renderer_, &row);

            const SDL_Color outputColor =
                entry.outputIsTool ? ToolColor(entry.toolKind, entry.toolTier) : TileColor(entry.outputType);
            SDL_SetRenderDrawColor(renderer_, outputColor.r, outputColor.g, outputColor.b, outputColor.a);
            SDL_Rect outputRect{x + padding, y + padding, 16, rowHeight - padding * 2};
            SDL_RenderFillRect(renderer_, &outputRect);
            if (entry.outputIsTool) {
                char label = 'T';
                switch (entry.toolKind) {
                case entities::ToolKind::Pickaxe: label = 'P'; break;
                case entities::ToolKind::Axe: label = 'A'; break;
                case entities::ToolKind::Sword: label = 'S'; break;
                }
                drawNumber(std::string(1, label), x + padding + 4, y + 6, 2, SDL_Color{20, 20, 20, 230});
            } else {
                drawNumber(std::to_string(std::max(0, entry.outputCount)), x + padding + 20, y + 6, 2, textColor);
            }

            int ingredientX = x + 80;
        for (int ing = 0; ing < entry.ingredientCount && ing < 2; ++ing) {
            const SDL_Color ingredientColor = TileColor(entry.ingredientTypes[static_cast<std::size_t>(ing)]);
            SDL_SetRenderDrawColor(renderer_, ingredientColor.r, ingredientColor.g, ingredientColor.b, ingredientColor.a);
            SDL_Rect ingRect{ingredientX, y + padding, 14, rowHeight - padding * 2};
            SDL_RenderFillRect(renderer_, &ingRect);
                drawNumber(std::to_string(std::max(0, entry.ingredientCounts[static_cast<std::size_t>(ing)])),
                           ingredientX + 18,
                           y + 6,
                           2,
                           textColor);
                ingredientX += 70;
            }

            y += rowHeight + 4;
        }
    }

    const TileTexture* tileTexture(world::TileType type) const {
        const auto it = tileTextures_.find(type);
        return it != tileTextures_.end() ? &it->second : nullptr;
    }

    SDL_Rect atlasRectFor(const world::World& world, world::TileType type, int tileX, int tileY) const {
        auto neighborCode = [&](int x, int y) -> char {
            if (x < 0 || y < 0 || x >= world.width() || y >= world.height()) {
                return '0';
            }
            const auto& neighbor = world.tile(x, y);
            if (!neighbor.active) {
                return '0';
            }
            const bool selfIsDirt = type == world::TileType::Dirt;
            if (selfIsDirt) {
                if (neighbor.type == world::TileType::Dirt || TileBlendsWithDirt(neighbor.type)) {
                    return 'x';
                }
                return '0';
            }

            const bool neighborIsDirt = neighbor.type == world::TileType::Dirt;
            if (neighbor.type == type) {
                return 'x';
            }
            if (neighborIsDirt && TileBlendsWithDirt(type)) {
                return 'd';
            }

            return '0';
        };

        std::string pattern(4, '0');
        pattern[0] = neighborCode(tileX, tileY - 1);
        pattern[1] = neighborCode(tileX + 1, tileY);
        pattern[2] = neighborCode(tileX, tileY + 1);
        pattern[3] = neighborCode(tileX - 1, tileY);

        std::vector<std::string> candidatePatterns;
        candidatePatterns.push_back(pattern);

        const bool containsDirt = pattern.find('d') != std::string::npos;
        const bool treatDirtAsSame = (type == world::TileType::Dirt);
        if (containsDirt && treatDirtAsSame) {
            auto asSame = pattern;
            for (char& c : asSame) {
                if (c == 'd') {
                    c = 'x';
                }
            }
            candidatePatterns.push_back(asSame);
        }
        if (containsDirt) {
            auto asAir = pattern;
            for (char& c : asAir) {
                if (c == 'd') {
                    c = '0';
                }
            }
            candidatePatterns.push_back(asAir);
        }

        const auto typeMaskIt = tileMaskRects_.find(type);
        if (typeMaskIt != tileMaskRects_.end()) {
            const auto& patternMap = typeMaskIt->second;
            const std::size_t base = static_cast<std::size_t>((tileX * 73856093) ^ (tileY * 19349663));
            for (const auto& key : candidatePatterns) {
                const auto rectIt = patternMap.find(key);
                if (rectIt == patternMap.end() || rectIt->second.empty()) {
                    continue;
                }
                const std::size_t idx = base % rectIt->second.size();
                return rectIt->second[idx];
            }
        }

        return SDL_Rect{0, 0, kTilePixels, kTilePixels};
    }

    SDL_Texture* loadTilesetTexture(const std::filesystem::path& path) {
        SDL_Surface* surface = SDL_LoadBMP(path.string().c_str());
        if (!surface) {
            SDL_Log("Failed to load %s: %s", path.string().c_str(), SDL_GetError());
            return nullptr;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_FreeSurface(surface);
        if (!texture) {
            SDL_Log("Failed to create texture for %s: %s", path.string().c_str(), SDL_GetError());
        }
        return texture;
    }

    void loadTileTextures() {
        destroyTileTextures();
        const std::filesystem::path basePath = std::filesystem::path("graphics") / "tiles";
        if (!std::filesystem::exists(basePath)) {
            SDL_Log("Tile directory missing: %s", basePath.string().c_str());
            return;
        }

        struct TileEntry {
            world::TileType type;
            const char* filename;
        };

        const std::array<TileEntry, 6> entries{{
            {world::TileType::Dirt, "dirt.bmp"},
            {world::TileType::Stone, "stone.bmp"},
            {world::TileType::Grass, "grass.bmp"},
            {world::TileType::CopperOre, "copper.bmp"},
            {world::TileType::IronOre, "iron.bmp"},
            {world::TileType::GoldOre, "gold.bmp"},
        }};

        for (const auto& entry : entries) {
            const auto texturePath = basePath / entry.filename;
            if (!std::filesystem::exists(texturePath)) {
                SDL_Log("Missing tile texture %s", texturePath.string().c_str());
                continue;
            }
            if (SDL_Texture* texture = loadTilesetTexture(texturePath)) {
                tileTextures_[entry.type] = TileTexture{texture};
                tileMaskRects_[entry.type] = buildDefaultMaskRects();
            }
        }
    }

    void destroyTileTextures() {
        for (auto& pair : tileTextures_) {
            if (pair.second.texture) {
                SDL_DestroyTexture(pair.second.texture);
            }
        }
        tileTextures_.clear();
        tileMaskRects_.clear();
    }

    std::unordered_map<std::string, std::vector<SDL_Rect>> buildDefaultMaskRects() const {
        static const std::array<std::array<const char*, 16>, 15> layout{{
            {"xxx0", "0xxx", "0xxx", "0xxx", "x0xx", "x0x0", "00x0", "00x0", "00x0", "0x00", "_", "_", "000x", "0xdx", "0xdx", "0xdx"},
            {"xxx0", "xxxx", "xxxx", "xxxx", "x0xx", "x0x0", "_", "_", "_", "0x00", "_", "_", "000x", "dx0x", "dx0x", "dx0x"},
            {"xxx0", "xx0x", "xx0x", "xx0x", "x0xx", "x0x0", "_", "_", "_", "0x00", "_", "_", "000x", "xdx0", "xdx0", "xdx0"},
            {"0xx0", "00xx", "0xx0", "00xx", "0xx0", "00xx", "x000", "x000", "x000", "0000", "0000", "0000", "", "x0xd", "x0xd", "x0xd"},
            {"xx00", "x00x", "xx00", "x00x", "xx00", "x00x", "0x0x", "0x0x", "0x0x", "", "", "", "", "", "", ""},
            {"_", "_", "dxxd", "ddxx", "xxd0", "x0dx", "00d0", "x0d0", "xxdx", "xxdx", "xxdx", "ddxd", "dxdd", "", "", ""},
            {"_", "_", "xxdd", "xddx", "xxd0", "x0dx", "00d0", "x0d0", "dxxx", "dxxx", "dxxx", "ddxd", "dxdd", "", "", ""},
            {"_", "_", "dxxd", "ddxx", "xxd0", "x0dx", "00d0", "x0d0", "xdxx", "xxxd", "xdxd", "ddxd", "dxdd", "", "", ""},
            {"_", "_", "xxdd", "xddx", "dxx0", "d0xx", "d000", "d0x0", "xdxx", "xxxd", "xdxd", "xddd", "dddx", "", "", ""},
            {"_", "_", "dxxd", "ddxx", "dxx0", "d0xx", "d000", "d0x0", "xdxx", "xxxd", "xdxd", "xddd", "dddx", "", "", ""},
            {"_", "_", "xxdd", "xddx", "dxx0", "d0xx", "d000", "d0x0", "dxdx", "dxdx", "dxdx", "xddd", "dddx", "", "", ""},
            {"0xxd", "0xxd", "0xxd", "0dxx", "0dxx", "0dxx", "dddd", "dddd", "dddd", "0d0d", "0d0d", "0d0d", "", "", "", ""},
            {"xx0d", "xx0d", "xx0d", "xd0x", "xd0x", "xd0x", "d0d0", "", "", "", "", "", "", "", "", ""},
            {"000d", "000d", "000d", "0d00", "0d00", "0d00", "d0d0", "", "", "", "", "", "", "", "", ""},
            {"0x0d", "0x0d", "0x0d", "0d0x", "0d0x", "0d0x", "d0d0", "", "", "", "", "", "", "", "", "_"}
        }};

        std::unordered_map<std::string, std::vector<SDL_Rect>> rects;
        for (std::size_t row = 0; row < layout.size(); ++row) {
            for (std::size_t col = 0; col < layout[row].size(); ++col) {
                const char* entry = layout[row][col];
                if (!entry || entry[0] == '_') {
                    continue;
                }
                SDL_Rect rect{
                    static_cast<int>(col) * kTilePixels,
                    static_cast<int>(row) * kTilePixels,
                    kTilePixels,
                    kTilePixels
                };
                rects[entry].push_back(rect);
            }
        }

        return rects;
    }

    void drawLetterGlyph(char c, int x, int y, int scale, SDL_Color color) {
        const char* pattern[5]{};
        switch (c) {
        case 'P':
            pattern[0] = "111";
            pattern[1] = "101";
            pattern[2] = "111";
            pattern[3] = "100";
            pattern[4] = "100";
            break;
        case 'A':
            pattern[0] = "111";
            pattern[1] = "101";
            pattern[2] = "111";
            pattern[3] = "101";
            pattern[4] = "101";
            break;
        case 'S':
            pattern[0] = "111";
            pattern[1] = "100";
            pattern[2] = "111";
            pattern[3] = "001";
            pattern[4] = "111";
            break;
        default:
            return;
        }

        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (pattern[row][col] == '1') {
                    SDL_Rect rect{x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer_, &rect);
                }
            }
        }
    }

    void drawNumber(const std::string& text, int x, int y, int scale, SDL_Color color) {
        int cursorX = x;
        for (char c : text) {
            if (c == '-') {
                drawDash(cursorX, y + 2 * scale, scale, color);
                cursorX += 4 * scale;
            } else if (c == 'P' || c == 'A' || c == 'S') {
                drawLetterGlyph(c, cursorX, y, scale, color);
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
