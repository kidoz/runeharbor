// SPDX-License-Identifier: MIT
#include "hud.hpp"

#include <algorithm>
#include <string>

#include <cmath>

#include "../engine/map_scene.hpp"
#include "../game/game_world.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"

namespace runeharbor::ui
{

// MM7 HUD layout constants (in 640x480 game coordinates)
namespace
{
// Party bar at bottom of screen: 4 portrait slots
constexpr int kPartyBarY = 402;
constexpr int kPartyBarH = 78;
constexpr int kPortraitW = 104;
constexpr int kPortraitH = 48;
constexpr int kPortraitStartX = 8;
constexpr int kPortraitSpacing = 156;

// HP/SP bar dimensions (within each portrait slot)
constexpr int kBarW = 100;
constexpr int kBarH = 8;
constexpr int kHPBarOffsetY = 52;
constexpr int kSPBarOffsetY = 62;

// Resource bar (gold/food) at bottom-right
constexpr int kResourceX = 500;
constexpr int kResourceY = 456;

// Minimap placeholder at top-right
constexpr int kMinimapX = 520;
constexpr int kMinimapY = 8;
constexpr int kMinimapW = 112;
constexpr int kMinimapH = 112;

// Time display below minimap
constexpr int kTimeX = 520;
constexpr int kTimeY = 124;

const char* kMonthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char* kDayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

const char* kClassName[] = {"Knight", "Paladin", "Archer", "Cleric", "Sorcerer",
                            "Thief",  "Monk",    "Ranger", "Druid"};

struct MinimapBounds
{
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    bool valid = false;
};

MinimapBounds buildMinimapBounds(const engine::MapScene* mapScene)
{
    MinimapBounds result;
    if (!mapScene)
    {
        return result;
    }

    auto extend = [&](float x, float y)
    {
        if (!result.valid)
        {
            result.minX = x;
            result.maxX = x;
            result.minY = y;
            result.maxY = y;
            result.valid = true;
            return;
        }
        result.minX = std::min(result.minX, x);
        result.maxX = std::max(result.maxX, x);
        result.minY = std::min(result.minY, y);
        result.maxY = std::max(result.maxY, y);
    };

    const auto& blv = mapScene->getBLVData();
    for (const auto& vertex : blv.vertices)
    {
        extend(static_cast<float>(vertex.x), static_cast<float>(vertex.y));
    }

    const auto& odm = mapScene->getODMData();
    if (!odm.heightmap.empty())
    {
        constexpr float kCell = 512.0f;
        const float half = static_cast<float>(runeharbor::formats::ODMMapData::TERRAIN_SIZE) * 0.5f;
        extend((-half) * kCell, (-half) * kCell);
        extend((half - 1.0f) * kCell, (half - 1.0f) * kCell);
    }

    for (const auto& building : odm.buildings)
    {
        const float minX = static_cast<float>(building.worldX + building.minX);
        const float maxX = static_cast<float>(building.worldX + building.maxX);
        const float minY = static_cast<float>(building.worldY + building.minY);
        const float maxY = static_cast<float>(building.worldY + building.maxY);
        extend(minX, minY);
        extend(maxX, maxY);
    }

    if (result.valid)
    {
        if (std::abs(result.maxX - result.minX) < 1.0f)
        {
            result.maxX = result.minX + 1.0f;
        }
        if (std::abs(result.maxY - result.minY) < 1.0f)
        {
            result.maxY = result.minY + 1.0f;
        }
    }

    return result;
}

float normalizeToUnit(float value, float min, float max)
{
    if (max <= min)
    {
        return 0.5f;
    }
    return std::clamp((value - min) / (max - min), 0.0f, 1.0f);
}
} // namespace

int HUD::sx(int gameX, float scale, float offsetX) const
{
    return static_cast<int>(offsetX + gameX * scale);
}

int HUD::sy(int gameY, float scale, float offsetY) const
{
    return static_cast<int>(offsetY + gameY * scale);
}

int HUD::sw(int gameW, float scale) const
{
    return static_cast<int>(gameW * scale);
}

int HUD::sh(int gameH, float scale) const
{
    return static_cast<int>(gameH * scale);
}

void HUD::render(graphics::IRenderer& renderer, const graphics::DebugText& debugText, float scale,
                 float offsetX, float offsetY)
{
    if (!gameWorld_)
        return;

    renderPartyBar(renderer, debugText, scale, offsetX, offsetY);
    renderResourceBar(renderer, debugText, scale, offsetX, offsetY);
    renderMinimap(renderer, debugText, scale, offsetX, offsetY);
    renderTimeDisplay(renderer, debugText, scale, offsetX, offsetY);
}

void HUD::renderPartyBar(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                         float scale, float offsetX, float offsetY)
{
    const auto& party = gameWorld_->party();

    // Background panel
    renderer.drawFilledRect(sx(0, scale, offsetX), sy(kPartyBarY, scale, offsetY), sw(640, scale),
                            sh(kPartyBarH, scale), 10, 10, 15, 200);

    for (int i = 0; i < game::kPartySize; i++)
    {
        const auto& ch = party.member(i);
        int baseX = kPortraitStartX + i * kPortraitSpacing;

        // Portrait placeholder (colored rectangle based on class)
        uint8_t classHue =
            static_cast<uint8_t>((static_cast<int>(ch.charClass) * 37 + 60) % 200 + 55);
        renderer.drawFilledRect(sx(baseX, scale, offsetX), sy(kPartyBarY + 4, scale, offsetY),
                                sw(kPortraitW, scale), sh(kPortraitH, scale), classHue,
                                static_cast<uint8_t>(classHue / 2), 40, 180);

        // Name and class
        SDL_Renderer* sdl = renderer.getSDLRenderer();
        if (sdl)
        {
            int nameY = sy(kPartyBarY + 6, scale, offsetY);
            debugText.drawText(sdl, sx(baseX + 2, scale, offsetX), nameY, 1, 255, 255, 255,
                               ch.name);

            int classIdx = static_cast<int>(ch.charClass);
            if (classIdx >= 0 && classIdx < 9)
            {
                debugText.drawText(sdl, sx(baseX + 2, scale, offsetX),
                                   nameY + debugText.lineHeight(1), 1, 180, 180, 200,
                                   kClassName[classIdx]);
            }

            // Level
            std::string lvlStr = "Lv" + std::to_string(ch.level);
            debugText.drawText(sdl, sx(baseX + kPortraitW - 24, scale, offsetX), nameY, 1, 200, 200,
                               100, lvlStr);
        }

        // HP bar
        int hpBarX = sx(baseX, scale, offsetX);
        int hpBarY = sy(kPartyBarY + kHPBarOffsetY, scale, offsetY);
        int hpBarW = sw(kBarW, scale);
        int hpBarH = sh(kBarH, scale);

        renderer.drawFilledRect(hpBarX, hpBarY, hpBarW, hpBarH, 40, 10, 10, 200);

        if (ch.maxHitPoints > 0)
        {
            int hpFill = std::clamp(ch.hitPoints * hpBarW / ch.maxHitPoints, 0, hpBarW);
            uint8_t hpR = 200, hpG = 40, hpB = 40;
            if (ch.hitPoints > ch.maxHitPoints / 2)
            {
                hpG = 160;
                hpB = 40;
            }
            else if (ch.hitPoints > ch.maxHitPoints / 4)
            {
                hpR = 220;
                hpG = 160;
                hpB = 20;
            }
            renderer.drawFilledRect(hpBarX, hpBarY, hpFill, hpBarH, hpR, hpG, hpB, 220);
        }
        renderer.drawRect(hpBarX, hpBarY, hpBarW, hpBarH, 80, 80, 80, 255);

        // SP bar
        int spBarY = sy(kPartyBarY + kSPBarOffsetY, scale, offsetY);
        renderer.drawFilledRect(hpBarX, spBarY, hpBarW, hpBarH, 10, 10, 40, 200);

        if (ch.maxSpellPoints > 0)
        {
            int spFill = std::clamp(ch.spellPoints * hpBarW / ch.maxSpellPoints, 0, hpBarW);
            renderer.drawFilledRect(hpBarX, spBarY, spFill, hpBarH, 40, 80, 220, 220);
        }
        renderer.drawRect(hpBarX, spBarY, hpBarW, hpBarH, 80, 80, 80, 255);

        // HP/SP numbers
        if (sdl)
        {
            std::string hpStr =
                std::to_string(ch.hitPoints) + "/" + std::to_string(ch.maxHitPoints);
            std::string spStr =
                std::to_string(ch.spellPoints) + "/" + std::to_string(ch.maxSpellPoints);
            debugText.drawText(sdl, hpBarX + 2, hpBarY + 1, 1, 255, 255, 255, hpStr);
            debugText.drawText(sdl, hpBarX + 2, spBarY + 1, 1, 200, 200, 255, spStr);
        }

        // Condition indicator
        if (!ch.isConscious())
        {
            renderer.drawFilledRect(sx(baseX, scale, offsetX), sy(kPartyBarY + 4, scale, offsetY),
                                    sw(kPortraitW, scale), sh(kPortraitH, scale), 0, 0, 0, 150);
            if (sdl)
            {
                const char* status = ch.isAlive() ? "UNCONSCIOUS" : "DEAD";
                int txtY = sy(kPartyBarY + 24, scale, offsetY);
                debugText.drawText(sdl, sx(baseX + 10, scale, offsetX), txtY, 1, 255, 60, 60,
                                   status);
            }
        }
    }
}

void HUD::renderResourceBar(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                            float scale, float offsetX, float offsetY)
{
    const auto& party = gameWorld_->party();
    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    // Gold
    std::string goldStr = "Gold: " + std::to_string(party.gold());
    debugText.drawText(sdl, sx(kResourceX, scale, offsetX), sy(kResourceY, scale, offsetY), 1, 255,
                       215, 0, goldStr);

    // Food
    std::string foodStr = "Food: " + std::to_string(party.food());
    debugText.drawText(sdl, sx(kResourceX + 70, scale, offsetX), sy(kResourceY, scale, offsetY), 1,
                       180, 220, 140, foodStr);
}

void HUD::renderMinimap(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                        float scale, float offsetX, float offsetY)
{
    renderer.drawFilledRect(sx(kMinimapX, scale, offsetX), sy(kMinimapY, scale, offsetY),
                            sw(kMinimapW, scale), sh(kMinimapH, scale), 15, 20, 15, 180);
    renderer.drawRect(sx(kMinimapX, scale, offsetX), sy(kMinimapY, scale, offsetY),
                      sw(kMinimapW, scale), sh(kMinimapH, scale), 80, 100, 80, 255);

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (sdl)
    {
        // Map name
        const auto& mapName = gameWorld_->currentMap();
        if (!mapName.empty())
        {
            debugText.drawText(sdl, sx(kMinimapX + 4, scale, offsetX),
                               sy(kMinimapY + 4, scale, offsetY), 1, 200, 200, 200, mapName);
        }
        else
        {
            debugText.drawText(sdl, sx(kMinimapX + 20, scale, offsetX),
                               sy(kMinimapY + 48, scale, offsetY), 1, 100, 100, 100, "MINIMAP");
        }
    }

    constexpr float kPad = 4.0f;
    const float innerW = static_cast<float>(kMinimapW) - 2.0f * kPad;
    const float innerH = static_cast<float>(kMinimapH) - 2.0f * kPad;
    const MinimapBounds bounds = buildMinimapBounds(mapScene_);

    float partyU = 0.5f;
    float partyV = 0.5f;
    if (bounds.valid)
    {
        partyU = normalizeToUnit(gameWorld_->party().worldX(), bounds.minX, bounds.maxX);
        // Minimap Y grows downward; world Y grows upward in map coordinates.
        partyV = 1.0f - normalizeToUnit(gameWorld_->party().worldY(), bounds.minY, bounds.maxY);

        const int mapFrameX = sx(kMinimapX + static_cast<int>(kPad), scale, offsetX);
        const int mapFrameY = sy(kMinimapY + static_cast<int>(kPad), scale, offsetY);
        const int mapFrameW = sw(static_cast<int>(innerW), scale);
        const int mapFrameH = sh(static_cast<int>(innerH), scale);
        renderer.drawRect(mapFrameX, mapFrameY, mapFrameW, mapFrameH, 65, 120, 65, 180);
    }

    const float px = static_cast<float>(kMinimapX) + kPad + partyU * innerW;
    const float py = static_cast<float>(kMinimapY) + kPad + partyV * innerH;
    const int cx = sx(static_cast<int>(px), scale, offsetX);
    const int cy = sy(static_cast<int>(py), scale, offsetY);
    renderer.drawFilledRect(cx - 2, cy - 2, 5, 5, 255, 255, 100, 255);
}

void HUD::renderTimeDisplay(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                            float scale, float offsetX, float offsetY)
{
    (void)renderer;
    const auto& cal = gameWorld_->calendar();
    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    // Date
    int monthIdx = std::clamp(cal.month() - 1, 0, 11);
    int dayIdx = std::clamp(cal.dayOfWeek(), 0, 6);
    std::string dateStr = std::string(kDayNames[dayIdx]) + " " + std::to_string(cal.day()) + " " +
                          kMonthNames[monthIdx] + " " + std::to_string(cal.year());
    debugText.drawText(sdl, sx(kTimeX, scale, offsetX), sy(kTimeY, scale, offsetY), 1, 200, 200,
                       200, dateStr);

    // Time
    int h = cal.hour();
    int m = cal.minute();
    std::string timeStr = std::to_string(h / 10) + std::to_string(h % 10) + ":" +
                          std::to_string(m / 10) + std::to_string(m % 10);
    if (cal.isNight())
        timeStr += " (Night)";

    debugText.drawText(sdl, sx(kTimeX, scale, offsetX), sy(kTimeY + 10, scale, offsetY), 1, 180,
                       180, 220, timeStr);
}

} // namespace runeharbor::ui
