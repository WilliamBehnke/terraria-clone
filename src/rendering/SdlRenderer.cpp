#include "terraria/rendering/Renderer.h"

#include "terraria/core/Application.h"
#include "terraria/entities/Tools.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
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

const char* TileName(world::TileType type) {
    switch (type) {
    case world::TileType::Dirt: return "DIRT";
    case world::TileType::Stone: return "STONE";
    case world::TileType::Grass: return "GRASS";
    case world::TileType::CopperOre: return "COPPER";
    case world::TileType::IronOre: return "IRON";
    case world::TileType::GoldOre: return "GOLD";
    case world::TileType::Wood: return "WOOD";
    case world::TileType::Leaves: return "LEAVES";
    case world::TileType::WoodPlank: return "PLANKS";
    case world::TileType::StoneBrick: return "BRICK";
    case world::TileType::TreeTrunk: return "TRUNK";
    case world::TileType::TreeLeaves: return "LEAVES";
    case world::TileType::Air:
    default: return "";
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
    case entities::ToolKind::Blaster:
        switch (tier) {
        case entities::ToolTier::Wood: return SDL_Color{160, 110, 170, 255};
        case entities::ToolTier::Stone: return SDL_Color{120, 120, 190, 255};
        case entities::ToolTier::Copper: return SDL_Color{220, 140, 220, 255};
        case entities::ToolTier::Iron: return SDL_Color{170, 200, 230, 255};
        case entities::ToolTier::Gold: return SDL_Color{255, 240, 140, 255};
        default: return SDL_Color{140, 140, 180, 255};
        }
    }
}

const char* ToolKindLabel(entities::ToolKind kind) {
    switch (kind) {
    case entities::ToolKind::Pickaxe: return "PICK";
    case entities::ToolKind::Axe: return "AXE";
    case entities::ToolKind::Sword: return "SWORD";
    case entities::ToolKind::Blaster: return "BLSTR";
    default: return "";
    }
}

const char* ToolTierLabel(entities::ToolTier tier) {
    switch (tier) {
    case entities::ToolTier::Wood: return "WOOD";
    case entities::ToolTier::Stone: return "STONE";
    case entities::ToolTier::Copper: return "COPPER";
    case entities::ToolTier::Iron: return "IRON";
    case entities::ToolTier::Gold: return "GOLD";
    default: return "";
    }
}

std::string ToolLabel(entities::ToolKind kind, entities::ToolTier tier) {
    std::string label;
    if (const char* tierText = ToolTierLabel(tier); tierText && tierText[0] != '\0') {
        label += tierText;
    }
    if (const char* kindText = ToolKindLabel(kind); kindText && kindText[0] != '\0') {
        label += kindText;
    }
    return label;
}

SDL_Color ArmorColor(entities::ArmorId id) {
    switch (id) {
    case entities::ArmorId::WoodHelmet:
    case entities::ArmorId::WoodChest:
    case entities::ArmorId::WoodBoots: return SDL_Color{150, 110, 70, 255};
    case entities::ArmorId::CopperHelmet:
    case entities::ArmorId::CopperChest:
    case entities::ArmorId::CopperBoots: return SDL_Color{220, 140, 80, 255};
    case entities::ArmorId::IronHelmet:
    case entities::ArmorId::IronChest:
    case entities::ArmorId::IronBoots: return SDL_Color{170, 180, 200, 255};
    default: return SDL_Color{90, 90, 90, 255};
    }
}

SDL_Color AccessoryColor(entities::AccessoryId id) {
    switch (id) {
    case entities::AccessoryId::FleetBoots: return SDL_Color{120, 200, 255, 255};
    case entities::AccessoryId::VitalityCharm: return SDL_Color{255, 100, 140, 255};
    case entities::AccessoryId::MinerRing: return SDL_Color{200, 200, 100, 255};
    default: return SDL_Color{100, 100, 120, 255};
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
                const int worldX = startX + x;
                const int worldY = startY + y;
                const auto& tile = world.tile(worldX, worldY);
                if (!tile.active() || tile.type() == world::TileType::Air) {
                    continue;
                }

                SDL_Rect rect{ pixelOffsetX + x * kTilePixels, pixelOffsetY + y * kTilePixels, kTilePixels, kTilePixels };
                if (const TileTexture* textureInfo = tileTexture(tile.type())) {
                    SDL_Rect src = atlasRectFor(world, tile.type(), startX + x, startY + y);
                    SDL_RenderCopy(renderer_, textureInfo->texture, &src, &rect);
                } else {
                    const SDL_Color color = TileColor(tile.type());
                    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
                    SDL_RenderFillRect(renderer_, &rect);
                }

                if (tile.isSolid()) {
                    const auto neighborIsSolid = [&](int nx, int ny) {
                        if (nx < 0 || nx >= world.width() || ny < 0 || ny >= world.height()) {
                            return true;
                        }
                        const auto& neighbor = world.tile(nx, ny);
                        return neighbor.active() && neighbor.isSolid();
                    };

                    constexpr int kFogRadius = 6;
                    bool foundAir = false;
                    int distanceToAir = kFogRadius + 1;
                    for (int r = 1; r <= kFogRadius && !foundAir; ++r) {
                        for (int dx = -r; dx <= r && !foundAir; ++dx) {
                            const int dy = r - std::abs(dx);
                            if (dy == 0) {
                                if (!neighborIsSolid(worldX + dx, worldY)) {
                                    foundAir = true;
                                    distanceToAir = r;
                                }
                            } else {
                                if (!neighborIsSolid(worldX + dx, worldY + dy)
                                    || !neighborIsSolid(worldX + dx, worldY - dy)) {
                                    foundAir = true;
                                    distanceToAir = r;
                                }
                            }
                        }
                    }

                    Uint8 alpha = 0;
                    if (!foundAir) {
                        alpha = 255;
                    } else {
                        constexpr int kMinAlpha = 60;
                        constexpr int kMaxAlpha = 220;
                        const int spread = std::max(1, kFogRadius - 1);
                        alpha = static_cast<Uint8>(kMinAlpha + (distanceToAir - 1) * (kMaxAlpha - kMinAlpha) / spread);
                    }
                    if (alpha > 0) {
                        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, alpha);
                        SDL_RenderFillRect(renderer_, &rect);
                    }
                }
            }
        }

        if (hud.cursorHighlight && hud.cursorTileX >= startX && hud.cursorTileX < startX + tilesWide
            && hud.cursorTileY >= startY
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

        drawProjectiles(hud, startX, startY, tilesWide, tilesTall, pixelOffsetX, pixelOffsetY);
        drawZombies(zombies, startX, startY, tilesWide, tilesTall, pixelOffsetX, pixelOffsetY);
        drawSwordSwing(hud, startX, startY, tilesWide, tilesTall, pixelOffsetX, pixelOffsetY);
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

    void drawSwordSwing(const HudState& hud,
                        int startX,
                        int startY,
                        int tilesWide,
                        int tilesTall,
                        int pixelOffsetX,
                        int pixelOffsetY) {
        if (!hud.swordSwingActive) {
            return;
        }
        const float left = hud.swordSwingX - hud.swordSwingHalfWidth;
        const float right = hud.swordSwingX + hud.swordSwingHalfWidth;
        const float bottom = hud.swordSwingY;
        const float top = hud.swordSwingY - hud.swordSwingHalfHeight * 2.0F;
        if (right < static_cast<float>(startX) || left > static_cast<float>(startX + tilesWide)
            || bottom < static_cast<float>(startY) || top > static_cast<float>(startY + tilesTall)) {
            return;
        }
        const int rectX = pixelOffsetX + static_cast<int>(std::round((left - static_cast<float>(startX)) * kTilePixels));
        const int rectY = pixelOffsetY + static_cast<int>(std::round((top - static_cast<float>(startY)) * kTilePixels));
        const int rectW = std::max(1, static_cast<int>(std::round((right - left) * kTilePixels)));
        const int rectH = std::max(1, static_cast<int>(std::round((bottom - top) * kTilePixels)));
        SDL_Rect rect{rectX, rectY, rectW, rectH};
        SDL_SetRenderDrawColor(renderer_, 255, 220, 120, 70);
        SDL_RenderFillRect(renderer_, &rect);
        SDL_SetRenderDrawColor(renderer_, 255, 180, 60, 220);
        SDL_RenderDrawRect(renderer_, &rect);
    }

    void drawProjectiles(const HudState& hud,
                         int startX,
                         int startY,
                         int tilesWide,
                         int tilesTall,
                         int pixelOffsetX,
                         int pixelOffsetY) {
        if (hud.projectileCount <= 0) {
            return;
        }
        for (int i = 0; i < hud.projectileCount; ++i) {
            const auto& entry = hud.projectiles[static_cast<std::size_t>(i)];
            if (!entry.active) {
                continue;
            }
            const float left = entry.x - entry.radius;
            const float right = entry.x + entry.radius;
            const float bottom = entry.y;
            const float top = entry.y - entry.radius * 2.0F;
            if (right < static_cast<float>(startX) || left > static_cast<float>(startX + tilesWide)
                || bottom < static_cast<float>(startY) || top > static_cast<float>(startY + tilesTall)) {
                continue;
            }
            const int rectX =
                pixelOffsetX + static_cast<int>(std::round((left - static_cast<float>(startX)) * kTilePixels));
            const int rectY =
                pixelOffsetY + static_cast<int>(std::round((top - static_cast<float>(startY)) * kTilePixels));
            const int rectW = std::max(2, static_cast<int>(std::round((right - left) * kTilePixels)));
            const int rectH = std::max(2, static_cast<int>(std::round((bottom - top) * kTilePixels)));
            SDL_Rect rect{rectX, rectY, rectW, rectH};
            SDL_SetRenderDrawColor(renderer_, 255, 200, 120, 220);
            SDL_RenderFillRect(renderer_, &rect);
        }
    }

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
        SDL_Rect defensePanel{zombiePanel.x + zombiePanel.w + 8, zombiePanel.y, 110, 20};
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 200);
        SDL_RenderFillRect(renderer_, &defensePanel);
        SDL_SetRenderDrawColor(renderer_, 70, 70, 70, 255);
        SDL_RenderDrawRect(renderer_, &defensePanel);
        drawNumber("DEF " + std::to_string(std::max(0, hud.playerDefense)), defensePanel.x + 6, defensePanel.y + 4, 2, SDL_Color{200, 220, 255, 255});

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

        const int coordWidth = 170;
        const int coordHeight = 22;
        const int coordX = config_.windowWidth - coordWidth - margin;
        const int coordY = cycleY + cycleHeight + 8;
        SDL_Rect coordPanel{coordX, coordY, coordWidth, coordHeight};
        SDL_SetRenderDrawColor(renderer_, 12, 12, 12, 200);
        SDL_RenderFillRect(renderer_, &coordPanel);
        SDL_SetRenderDrawColor(renderer_, 70, 70, 70, 255);
        SDL_RenderDrawRect(renderer_, &coordPanel);
        SDL_Color coordColor{200, 200, 200, 255};
        drawNumber("X " + std::to_string(hud.playerTileX), coordPanel.x + 6, coordPanel.y + 4, 2, coordColor);
        drawNumber("Y " + std::to_string(hud.playerTileY), coordPanel.x + coordPanel.w / 2, coordPanel.y + 4, 2, coordColor);

        const int perfWidth = 180;
        const int perfHeight = 74;
        const int perfX = coordX;
        const int perfY = coordY + coordHeight + 8;
        SDL_Rect perfPanel{perfX, perfY, perfWidth, perfHeight};
        SDL_SetRenderDrawColor(renderer_, 12, 12, 12, 200);
        SDL_RenderFillRect(renderer_, &perfPanel);
        SDL_SetRenderDrawColor(renderer_, 70, 70, 70, 255);
        SDL_RenderDrawRect(renderer_, &perfPanel);
        const SDL_Color perfColor{200, 200, 200, 255};
        const int fpsValue = std::max(0, static_cast<int>(std::round(hud.perfFps)));
        const int frameMs = std::max(0, static_cast<int>(std::round(hud.perfFrameMs)));
        const int updateMs = std::max(0, static_cast<int>(std::round(hud.perfUpdateMs)));
        const int renderMs = std::max(0, static_cast<int>(std::round(hud.perfRenderMs)));
        drawNumber("FPS " + std::to_string(fpsValue), perfPanel.x + 6, perfPanel.y + 4, 2, perfColor);
        drawNumber("FT " + std::to_string(frameMs) + "MS", perfPanel.x + 6, perfPanel.y + 22, 2, perfColor);
        drawNumber("UP " + std::to_string(updateMs) + "MS", perfPanel.x + 6, perfPanel.y + 40, 2, perfColor);
        drawNumber("RD " + std::to_string(renderMs) + "MS", perfPanel.x + 6, perfPanel.y + 58, 2, perfColor);
    }
    void drawInventoryOverlay(const entities::Player& player, const HudState& hud) {
        (void)player;
        if (hud.inventorySlotCount <= 0) {
            return;
        }

        const int slotWidth = kInventorySlotWidth;
        const int slotHeight = kInventorySlotHeight;
        const int spacing = kInventorySlotSpacing;
        const int margin = spacing;
        const int rows = hud.inventoryOpen ? kInventoryRows : 1;
        const int columns = kInventoryColumns;
        const int baseY = config_.windowHeight - slotHeight - margin;
        SDL_Color textColor{220, 220, 220, 255};

        const int labelHeight = 14;

        auto slotLabel = [&](const HotbarSlotHud& slotData) -> std::string {
            if (slotData.isTool) {
                switch (slotData.toolKind) {
                case entities::ToolKind::Pickaxe: return "PICKAXE";
                case entities::ToolKind::Axe: return "AXE";
                case entities::ToolKind::Sword: return "SWORD";
                case entities::ToolKind::Blaster: return "BLASTER";
                }
            } else if (slotData.isArmor) {
                return entities::ArmorName(slotData.armorId);
            } else if (slotData.isAccessory) {
                return entities::AccessoryName(slotData.accessoryId);
            } else if (slotData.tileType != world::TileType::Air) {
                if (const char* tileName = TileName(slotData.tileType); tileName && tileName[0] != '\0') {
                    return tileName;
                }
            }
            return {};
        };

        constexpr int kStackLimit = entities::kMaxStackCount;
        auto equipmentSlotLabel = [&](const EquipmentSlotHud& slot) -> std::string {
            if (slot.isArmor) {
                const int slotIdx = slot.slotIndex;
                if (slotIdx == static_cast<int>(entities::ArmorSlot::Head)) {
                    return "HEAD";
                }
                if (slotIdx == static_cast<int>(entities::ArmorSlot::Body)) {
                    return "CHEST";
                }
                if (slotIdx == static_cast<int>(entities::ArmorSlot::Legs)) {
                    return "LEGS";
                }
                return "ARMOR";
            }
            return "ACC" + std::to_string(slot.slotIndex + 1);
        };
        auto equipmentItemName = [&](const EquipmentSlotHud& slot) -> std::string {
            if (slot.isArmor && slot.armorId != entities::ArmorId::None) {
                return entities::ArmorName(slot.armorId);
            }
            if (!slot.isArmor && slot.accessoryId != entities::AccessoryId::None) {
                return entities::AccessoryName(slot.accessoryId);
            }
            return {};
        };
        auto drawEquipmentSlot = [&](const EquipmentSlotHud& slot) {
            SDL_Rect panel{slot.x, slot.y, slot.w, slot.h};
            SDL_Color borderColor{60, 60, 60, 255};
            if (slot.hovered) {
                SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 230);
                SDL_RenderFillRect(renderer_, &panel);
                borderColor = SDL_Color{200, 200, 140, 255};
            } else {
                SDL_SetRenderDrawColor(renderer_, 5, 5, 5, 210);
                SDL_RenderFillRect(renderer_, &panel);
            }
            SDL_SetRenderDrawColor(renderer_, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderDrawRect(renderer_, &panel);

            const std::string slotText = equipmentSlotLabel(slot);
            if (!slotText.empty()) {
                drawNumber(slotText, panel.x + 6, panel.y + 4, 2, textColor);
            }

            SDL_Rect swatch{panel.x + 6, panel.y + 22, panel.w - 12, std::max(8, panel.h - 36)};
            const bool hasItem = slot.occupied;
            SDL_Color swatchColor{50, 50, 60, 140};
            if (slot.isArmor) {
                swatchColor = hasItem ? ArmorColor(slot.armorId) : SDL_Color{60, 60, 60, 140};
            } else {
                swatchColor = hasItem ? AccessoryColor(slot.accessoryId) : SDL_Color{70, 70, 90, 140};
            }
            SDL_SetRenderDrawColor(renderer_, swatchColor.r, swatchColor.g, swatchColor.b, swatchColor.a);
            SDL_RenderFillRect(renderer_, &swatch);

            const std::string itemName = equipmentItemName(slot);
            if (!itemName.empty()) {
                SDL_SetRenderDrawColor(renderer_, 5, 5, 5, 170);
                SDL_Rect labelBg{panel.x + 4, panel.y + panel.h - 18, panel.w - 8, 14};
                SDL_RenderFillRect(renderer_, &labelBg);
                drawNumber(itemName, panel.x + 8, panel.y + panel.h - 16, 2, textColor);
            }
        };
        auto drawSlotPanel = [&](const HotbarSlotHud& slotData, const SDL_Rect& panel, bool selected, bool hovered) {
            if (selected) {
                SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 240);
                SDL_RenderFillRect(renderer_, &panel);
                SDL_SetRenderDrawColor(renderer_, 220, 210, 150, 255);
            } else if (hovered) {
                SDL_SetRenderDrawColor(renderer_, 15, 15, 15, 210);
                SDL_RenderFillRect(renderer_, &panel);
                SDL_SetRenderDrawColor(renderer_, 140, 140, 80, 255);
            } else {
                SDL_SetRenderDrawColor(renderer_, 5, 5, 5, 210);
                SDL_RenderFillRect(renderer_, &panel);
                SDL_SetRenderDrawColor(renderer_, 60, 60, 60, 255);
            }
            const std::string name = slotLabel(slotData);
            SDL_RenderDrawRect(renderer_, &panel);

            const int count = std::max(0, slotData.count);
            if (slotData.isTool) {
                const SDL_Color toolColor = ToolColor(slotData.toolKind, slotData.toolTier);
                SDL_SetRenderDrawColor(renderer_, toolColor.r, toolColor.g, toolColor.b, toolColor.a);
                SDL_Rect swatch{panel.x + 6, panel.y + 6, panel.w - 12, panel.h - (12 + labelHeight)};
                SDL_RenderFillRect(renderer_, &swatch);
                char label = 'T';
                switch (slotData.toolKind) {
                case entities::ToolKind::Pickaxe: label = 'P'; break;
                case entities::ToolKind::Axe: label = 'A'; break;
                case entities::ToolKind::Sword: label = 'S'; break;
                case entities::ToolKind::Blaster: label = 'B'; break;
                }
                drawNumber(std::string(1, label), panel.x + panel.w / 2 - 4, panel.y + 8, 3, SDL_Color{20, 20, 20, 230});
            } else if (slotData.isArmor) {
                const SDL_Color armorColor = ArmorColor(slotData.armorId);
                SDL_SetRenderDrawColor(renderer_, armorColor.r, armorColor.g, armorColor.b, armorColor.a);
                SDL_Rect swatch{panel.x + 6, panel.y + 6, panel.w - 12, panel.h - (12 + labelHeight)};
                SDL_RenderFillRect(renderer_, &swatch);
                drawNumber("AR", panel.x + 8, panel.y + 8, 2, SDL_Color{20, 20, 20, 230});
                const int armorValue = std::max(0, entities::ArmorStats(slotData.armorId).defense);
                if (armorValue > 0) {
                    drawNumber("D" + std::to_string(armorValue), panel.x + panel.w - 32, panel.y + 8, 2, SDL_Color{20, 20, 20, 230});
                }
            } else if (slotData.isAccessory) {
                const SDL_Color accessoryColor = AccessoryColor(slotData.accessoryId);
                SDL_SetRenderDrawColor(renderer_, accessoryColor.r, accessoryColor.g, accessoryColor.b, accessoryColor.a);
                SDL_Rect swatch{panel.x + 6, panel.y + 6, panel.w - 12, panel.h - (12 + labelHeight)};
                SDL_RenderFillRect(renderer_, &swatch);
                drawNumber("AC", panel.x + 8, panel.y + 8, 2, SDL_Color{20, 20, 20, 230});
            } else if (slotData.tileType != world::TileType::Air) {
                const SDL_Color tileColor = TileColor(slotData.tileType);
                SDL_SetRenderDrawColor(renderer_, tileColor.r, tileColor.g, tileColor.b, tileColor.a);
                SDL_Rect swatch{panel.x + 6, panel.y + 6, 28, panel.h - (12 + labelHeight)};
                SDL_RenderFillRect(renderer_, &swatch);

                const int barMaxWidth = panel.w - 48;
                const float ratio = std::clamp(static_cast<float>(std::min(count, kStackLimit)) / static_cast<float>(kStackLimit),
                                               0.0F,
                                               1.0F);
                SDL_SetRenderDrawColor(renderer_, tileColor.r, tileColor.g, tileColor.b, 230);
                SDL_Rect bar{
                    panel.x + 40,
                    panel.y + panel.h - (labelHeight + 16),
                    static_cast<int>(barMaxWidth * ratio),
                    6};
                SDL_RenderFillRect(renderer_, &bar);
            }

            if (count > 0 && !slotData.isArmor) {
                drawNumber(std::to_string(count), panel.x + panel.w - 30, panel.y + 10, 2, textColor);
            }

            if (!name.empty()) {
                SDL_SetRenderDrawColor(renderer_, 5, 5, 5, 180);
                SDL_Rect labelBg{panel.x + 2, panel.y + panel.h - labelHeight - 8, panel.w - 4, labelHeight};
                SDL_RenderFillRect(renderer_, &labelBg);
                drawNumber(name, panel.x + 6, panel.y + panel.h - (labelHeight + 6), 2, textColor);
            }
        };

        if (hud.inventoryOpen && hud.equipmentSlotCount > 0) {
            for (int i = 0; i < hud.equipmentSlotCount; ++i) {
                drawEquipmentSlot(hud.equipmentSlots[static_cast<std::size_t>(i)]);
            }
        }

        for (int row = 0; row < rows; ++row) {
            const int rowY = baseY - row * (slotHeight + spacing);
            for (int col = 0; col < columns; ++col) {
                const int index = row * columns + col;
                if (index >= hud.inventorySlotCount) {
                    continue;
                }
                SDL_Rect panel{margin + col * (slotWidth + spacing), rowY, slotWidth, slotHeight};
                const bool selected = (row == 0 && col == hud.selectedSlot);
                const bool hovered = hud.inventoryOpen && hud.hoveredInventorySlot == index;
                drawSlotPanel(hud.inventorySlots[static_cast<std::size_t>(index)], panel, selected, hovered);
                if (row == 0 && col < hud.hotbarCount) {
                    const std::string label = std::to_string(col + 1);
                    drawNumber(label, panel.x + 6, panel.y + 6, 2, SDL_Color{200, 200, 200, 255});
                }
            }
        }

        if (hud.inventoryOpen && hud.carryingItem) {
            SDL_Rect floating{hud.mouseX - slotWidth / 2, hud.mouseY - slotHeight / 2, slotWidth, slotHeight};
            drawSlotPanel(hud.carriedItem, floating, false, false);
        }
    }

    void drawCraftingOverlay(const HudState& hud) {
        if (!hud.inventoryOpen || hud.craftRecipeCount <= 0) {
            return;
        }

        const int panelWidth = (hud.craftPanelWidth > 0) ? hud.craftPanelWidth : 210;
        const int rowHeight = (hud.craftRowHeight > 0) ? hud.craftRowHeight : 26;
        const int rowSpacing = (hud.craftRowSpacing > 0) ? hud.craftRowSpacing : 4;
        const int padding = 6;
        int x = hud.craftPanelX;
        int y = hud.craftPanelY;
        SDL_Color textColor{220, 220, 220, 255};

        const int visibleRows = std::max(1, (hud.craftVisibleRows > 0) ? hud.craftVisibleRows : 1);
        const int totalRows = std::min(hud.craftRecipeCount, kMaxCraftRecipes);
        const int maxStart = std::max(0, totalRows - visibleRows);
        const int startIndex = std::clamp(hud.craftScrollOffset, 0, maxStart);

        for (int visible = 0; visible < visibleRows && (startIndex + visible) < totalRows; ++visible) {
            const int idx = startIndex + visible;
            const auto& entry = hud.craftRecipes[static_cast<std::size_t>(idx)];
            SDL_Rect row{x, y, panelWidth, rowHeight};
            if (idx == hud.craftSelection) {
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

            SDL_Color outputColor{};
            if (entry.outputIsTool) {
                outputColor = ToolColor(entry.toolKind, entry.toolTier);
            } else if (entry.outputIsArmor) {
                outputColor = ArmorColor(entry.armorId);
            } else if (entry.outputIsAccessory) {
                outputColor = AccessoryColor(entry.accessoryId);
            } else {
                outputColor = TileColor(entry.outputType);
            }
            SDL_SetRenderDrawColor(renderer_, outputColor.r, outputColor.g, outputColor.b, outputColor.a);
            SDL_Rect outputRect{x + padding, y + padding, 16, rowHeight - padding * 2};
            SDL_RenderFillRect(renderer_, &outputRect);
            std::string outputLabel;
            const int ingredientSlots = std::min(2, entry.ingredientCount);
            const int ingredientSlotWidth = 70;
            int ingredientStartX = x + panelWidth - ingredientSlots * ingredientSlotWidth - padding;
            const int minIngredientStart = x + padding + 140;
            if (ingredientStartX < minIngredientStart) {
                ingredientStartX = minIngredientStart;
            }
            if (entry.outputIsTool) {
                char label = 'T';
                switch (entry.toolKind) {
                case entities::ToolKind::Pickaxe: label = 'P'; break;
                case entities::ToolKind::Axe: label = 'A'; break;
                case entities::ToolKind::Sword: label = 'S'; break;
                case entities::ToolKind::Blaster: label = 'B'; break;
                }
                drawNumber(std::string(1, label), x + padding + 4, y + 6, 2, SDL_Color{20, 20, 20, 230});
                outputLabel = ToolLabel(entry.toolKind, entry.toolTier);
            } else if (entry.outputIsArmor) {
                outputLabel = entities::ArmorName(entry.armorId);
            } else if (entry.outputIsAccessory) {
                outputLabel = entities::AccessoryName(entry.accessoryId);
            } else if (const char* tileName = TileName(entry.outputType); tileName && tileName[0] != '\0') {
                outputLabel = tileName;
            }
            if (!outputLabel.empty()) {
                drawNumber(outputLabel, x + padding + 24, y + 6, 2, textColor);
            }
            if (!entry.outputIsTool && !entry.outputIsArmor && !entry.outputIsAccessory && entry.outputCount > 0) {
                const int countX = std::max(x + padding + 24, ingredientStartX - 140);
                drawNumber("X" + std::to_string(entry.outputCount), countX, y + rowHeight - 20, 2, textColor);
            }

            int ingredientX = ingredientStartX;
            for (int ing = 0; ing < ingredientSlots; ++ing) {
                const SDL_Color ingredientColor = TileColor(entry.ingredientTypes[static_cast<std::size_t>(ing)]);
                SDL_SetRenderDrawColor(renderer_, ingredientColor.r, ingredientColor.g, ingredientColor.b, ingredientColor.a);
                SDL_Rect ingRect{ingredientX, y + padding, 14, rowHeight - padding * 2};
                SDL_RenderFillRect(renderer_, &ingRect);
                drawNumber(std::to_string(std::max(0, entry.ingredientCounts[static_cast<std::size_t>(ing)])),
                           ingredientX + 18,
                           y + 6,
                           2,
                           textColor);
                ingredientX += ingredientSlotWidth;
            }

            y += rowHeight + rowSpacing;
        }

        if (hud.craftScrollbarVisible && hud.craftScrollbarTrackHeight > 0) {
            SDL_SetRenderDrawColor(renderer_, 30, 30, 30, 200);
            SDL_Rect track{hud.craftScrollbarTrackX,
                           hud.craftScrollbarTrackY,
                           (hud.craftScrollbarWidth > 0) ? hud.craftScrollbarWidth : 6,
                           hud.craftScrollbarTrackHeight};
            SDL_RenderFillRect(renderer_, &track);
            SDL_SetRenderDrawColor(renderer_, 120, 120, 120, 220);
            SDL_Rect thumb{hud.craftScrollbarTrackX,
                           hud.craftScrollbarThumbY,
                           track.w,
                           std::max(8, hud.craftScrollbarThumbHeight)};
            SDL_RenderFillRect(renderer_, &thumb);
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
            if (!neighbor.active()) {
                return '0';
            }
            const bool selfIsDirt = type == world::TileType::Dirt;
            if (selfIsDirt) {
                if (neighbor.type() == world::TileType::Dirt || TileBlendsWithDirt(neighbor.type())) {
                    return 'x';
                }
                return '0';
            }

            const bool neighborIsDirt = neighbor.type() == world::TileType::Dirt;
            if (neighbor.type() == type) {
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

    int drawLetterGlyph(char c, int x, int y, int scale, SDL_Color color) {
        const char* pattern[5]{};
        int patternWidth = 3;
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
        case 'B':
            pattern[0] = "110";
            pattern[1] = "101";
            pattern[2] = "110";
            pattern[3] = "101";
            pattern[4] = "110";
            break;
        case 'F':
            pattern[0] = "111";
            pattern[1] = "100";
            pattern[2] = "110";
            pattern[3] = "100";
            pattern[4] = "100";
            break;
        case 'C':
            pattern[0] = "111";
            pattern[1] = "100";
            pattern[2] = "100";
            pattern[3] = "100";
            pattern[4] = "111";
            break;
        case 'D':
            pattern[0] = "110";
            pattern[1] = "101";
            pattern[2] = "101";
            pattern[3] = "101";
            pattern[4] = "110";
            break;
        case 'E':
            pattern[0] = "111";
            pattern[1] = "100";
            pattern[2] = "111";
            pattern[3] = "100";
            pattern[4] = "111";
            break;
        case 'G':
            pattern[0] = "111";
            pattern[1] = "100";
            pattern[2] = "101";
            pattern[3] = "101";
            pattern[4] = "111";
            break;
        case 'H':
            pattern[0] = "101";
            pattern[1] = "101";
            pattern[2] = "111";
            pattern[3] = "101";
            pattern[4] = "101";
            break;
        case 'I':
            pattern[0] = "111";
            pattern[1] = "010";
            pattern[2] = "010";
            pattern[3] = "010";
            pattern[4] = "111";
            break;
        case 'M':
            patternWidth = 5;
            pattern[0] = "10001";
            pattern[1] = "11011";
            pattern[2] = "10101";
            pattern[3] = "10001";
            pattern[4] = "10001";
            break;
        case 'K':
            patternWidth = 5;
            pattern[0] = "10001";
            pattern[1] = "10010";
            pattern[2] = "11100";
            pattern[3] = "10010";
            pattern[4] = "10001";
            break;
        case 'L':
            pattern[0] = "100";
            pattern[1] = "100";
            pattern[2] = "100";
            pattern[3] = "100";
            pattern[4] = "111";
            break;
        case 'N':
            patternWidth = 5;
            pattern[0] = "10001";
            pattern[1] = "11001";
            pattern[2] = "10101";
            pattern[3] = "10011";
            pattern[4] = "10001";
            break;
        case 'O':
            pattern[0] = "111";
            pattern[1] = "101";
            pattern[2] = "101";
            pattern[3] = "101";
            pattern[4] = "111";
            break;
        case 'R':
            pattern[0] = "110";
            pattern[1] = "101";
            pattern[2] = "110";
            pattern[3] = "101";
            pattern[4] = "101";
            break;
        case 'T':
            pattern[0] = "111";
            pattern[1] = "010";
            pattern[2] = "010";
            pattern[3] = "010";
            pattern[4] = "010";
            break;
        case 'U':
            pattern[0] = "101";
            pattern[1] = "101";
            pattern[2] = "101";
            pattern[3] = "101";
            pattern[4] = "111";
            break;
        case 'V':
            patternWidth = 5;
            pattern[0] = "10001";
            pattern[1] = "10001";
            pattern[2] = "01010";
            pattern[3] = "01010";
            pattern[4] = "00100";
            break;
        case 'W':
            patternWidth = 5;
            pattern[0] = "10001";
            pattern[1] = "10001";
            pattern[2] = "10101";
            pattern[3] = "10101";
            pattern[4] = "01010";
            break;
        case 'X':
            patternWidth = 5;
            pattern[0] = "10001";
            pattern[1] = "01010";
            pattern[2] = "00100";
            pattern[3] = "01010";
            pattern[4] = "10001";
            break;
        case 'Y':
            patternWidth = 5;
            pattern[0] = "10001";
            pattern[1] = "01010";
            pattern[2] = "00100";
            pattern[3] = "00100";
            pattern[4] = "00100";
            break;
        default:
            return 0;
        }

        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < patternWidth; ++col) {
                if (pattern[row][col] == '1') {
                    SDL_Rect rect{x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer_, &rect);
                }
            }
        }
        return patternWidth;
    }

    void drawNumber(const std::string& text, int x, int y, int scale, SDL_Color color) {
        int cursorX = x;
        for (char c : text) {
            if (c == '-') {
                drawDash(cursorX, y + 2 * scale, scale, color);
                cursorX += 4 * scale;
            } else if (c >= 'A' && c <= 'Z') {
                const int width = drawLetterGlyph(c, cursorX, y, scale, color);
                cursorX += (width > 0 ? width + 1 : 4) * scale;
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
