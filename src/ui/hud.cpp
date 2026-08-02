// SPDX-License-Identifier: MIT
#include "hud.hpp"

#include <algorithm>
#include <string>

#include <cmath>
#include <cstdio>

#include "../engine/map_scene.hpp"
#include "../game/game_world.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"

namespace runeharbor::ui
{

// MM7 HUD layout constants (in 640x480 game coordinates)
namespace
{
// The 3D view occupies the inclusive rect (8,8)-(468,351) (mm7.ini vx1/vy1/
// vx2/vy2 defaults, MM7-Rel.exe 0x4660C7-0x466103). The frame panels tile
// around it, and their rects are fixed by the native size of the shipped art
// rather than derived: ib-t-A is 468x8, ib-l-A is 8x344, ib-r-A is 172x480 and
// ib-b-A is 468x128. Note the right panel therefore starts at 468, painting
// over the view's inclusive right-edge column — a quirk of the original.
constexpr int kViewLeft = 8;
constexpr int kViewTop = 8;
constexpr int kViewBottom = 351; // inclusive
constexpr int kSidePanelH = 344; // ib-l-A height
constexpr int kRightPanelX = 468;
constexpr int kRightPanelW = 172;
constexpr int kTopPanelW = 468;

// Party bar at the bottom of the screen (ib-b-A).
constexpr int kPartyBarY = 352;
constexpr int kPartyBarW = 468;
constexpr int kPartyBarH = 128;

// Portrait art. X array at 0x4ED5F0, drawn at y=385 at the face texture's
// native 63x73 (fcn.004921b9).
constexpr int kPortraitX[] = {34, 149, 264, 379};
constexpr int kPortraitY = 385;
constexpr int kPortraitW = 63;
constexpr int kPortraitH = 73;

// Character-select hotspots (fcn.0041b639 @ 0x41B8FD-0x41B96E). Deliberately
// smaller than the portrait art: the original registers a 31x40 button per
// character, with '1'-'4' as hotkeys.
constexpr int kSelectX[] = {61, 177, 292, 407};
constexpr int kSelectY = 424;
constexpr int kSelectW = 31;
constexpr int kSelectH = 40;

// HP / SP indicator strips. X arrays at 0x4E2A98 and 0x4E2AA8 with a shared
// base y of 402 (fcn.0041b072); the hover hotspots registered alongside them
// at 0x41B98E-0x41BA8A are 5x49.
constexpr int kHpBarX[] = {22, 137, 251, 366};
constexpr int kSpBarX[] = {102, 217, 331, 447};
constexpr int kBarY = 402;
constexpr int kBarW = 5;
constexpr int kBarH = 49;

// Resource bar (gold/food) at bottom-right
constexpr int kResourceX = 500;
constexpr int kResourceY = 456;

// Minimap placeholder at top-right. Sits above the original's zoom-out/zoom-in
// buttons at (519,136) and (574,136) (0x41BD6C / 0x41BD10); the exact art rect
// has not been transcribed from the original yet.
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

void* HUD::lookupCached(const std::string& name, int& w, int& h)
{
    auto it = portraitCache_.find(name);
    if (it != portraitCache_.end())
    {
        w = it->second.w;
        h = it->second.h;
        return it->second.tex;
    }
    CachedPortrait c;
    if (textureLookup_)
    {
        c.tex = textureLookup_(name, c.w, c.h);
    }
    portraitCache_[name] = c;
    w = c.w;
    h = c.h;
    return c.tex;
}

int HUD::portraitAt(float scale, float offsetX, float offsetY, int screenX, int screenY) const
{
    if (scale <= 0.0f)
    {
        return -1;
    }
    // Invert the screen->game transform: game = (screen - offset) / scale.
    const float gameX = (static_cast<float>(screenX) - offsetX) / scale;
    const float gameY = (static_cast<float>(screenY) - offsetY) / scale;

    // Hit-test against the original's character-select buttons rather than the
    // full portrait art, matching MM7's clickable region.
    if (gameY < kSelectY || gameY >= kSelectY + kSelectH)
    {
        return -1;
    }
    for (int i = 0; i < game::kPartySize; i++)
    {
        if (gameX >= kSelectX[i] && gameX < kSelectX[i] + kSelectW)
        {
            return i;
        }
    }
    return -1;
}

void HUD::render(graphics::IRenderer& renderer, const graphics::DebugText& debugText, float scale,
                 float offsetX, float offsetY)
{
    if (!gameWorld_)
        return;

    if (textureLookup_)
    {
        int w, h;
        // Right panel
        void* tex_r = lookupCached("ib-r-A", w, h);
        if (tex_r)
            renderer.renderTexture(tex_r, sx(kRightPanelX, scale, offsetX), sy(0, scale, offsetY),
                                   sw(kRightPanelW, scale), sh(480, scale));

        // Bottom panel
        void* tex_b = lookupCached("ib-b-A", w, h);
        bottomPanelDrawn_ = tex_b != nullptr;
        if (tex_b)
            renderer.renderTexture(tex_b, sx(0, scale, offsetX), sy(kPartyBarY, scale, offsetY),
                                   sw(kPartyBarW, scale), sh(kPartyBarH, scale));

        // Top border
        void* tex_t = lookupCached("ib-t-A", w, h);
        if (tex_t)
            renderer.renderTexture(tex_t, sx(0, scale, offsetX), sy(0, scale, offsetY),
                                   sw(kTopPanelW, scale), sh(kViewTop, scale));

        // Left border
        void* tex_l = lookupCached("ib-l-A", w, h);
        if (tex_l)
            renderer.renderTexture(tex_l, sx(0, scale, offsetX), sy(kViewTop, scale, offsetY),
                                   sw(kViewLeft, scale), sh(kSidePanelH, scale));
    }

    renderPartyBar(renderer, debugText, scale, offsetX, offsetY);
    renderResourceBar(renderer, debugText, scale, offsetX, offsetY);
    renderMinimap(renderer, debugText, scale, offsetX, offsetY);
    renderTimeDisplay(renderer, debugText, scale, offsetX, offsetY);
}

void HUD::renderPartyBar(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                         float scale, float offsetX, float offsetY)
{
    const auto& party = gameWorld_->party();

    // Placeholder backdrop, only when the real ib-b-A panel art is unavailable —
    // painting it unconditionally hid the shipped panel underneath.
    if (!bottomPanelDrawn_)
    {
        renderer.drawFilledRect(sx(0, scale, offsetX), sy(kPartyBarY, scale, offsetY),
                                sw(640, scale), sh(kPartyBarH, scale), 10, 10, 15, 200);
    }

    for (int i = 0; i < game::kPartySize; i++)
    {
        const auto& ch = party.member(i);
        const int baseX = kPortraitX[i];

        // Portrait rendering
        void* portraitTex = nullptr;
        int portraitW = 0;
        int portraitH = 0;
        if (textureLookup_)
        {
            int w, h;
            // Use the canonical worst-condition resolver (Character::
            // worstActiveCondition, MM7 priority table at 0x4EDDA0) so every
            // one of the 18 conditions maps to a face frame, not just the 5
            // the prior cascade covered.
            const game::ConditionIndex worst = ch.worstActiveCondition();
            int frameIndex = 1; // Healthy
            switch (worst)
            {
            case game::ConditionIndex::Eradicated:
            case game::ConditionIndex::Stoned:
                frameIndex = 56;
                break;
            case game::ConditionIndex::Dead:
                frameIndex = 55;
                break;
            case game::ConditionIndex::Zombie:
                frameIndex = 54; // share the unconscious frame
                break;
            case game::ConditionIndex::Unconscious:
            case game::ConditionIndex::Asleep:
                frameIndex = 54;
                break;
            case game::ConditionIndex::Paralyzed:
                frameIndex = 54; // frozen look
                break;
            case game::ConditionIndex::Poison1:
            case game::ConditionIndex::Poison2:
            case game::ConditionIndex::Poison3:
            case game::ConditionIndex::Disease1:
            case game::ConditionIndex::Disease2:
            case game::ConditionIndex::Disease3:
                frameIndex = 47; // sick / poisoned
                break;
            case game::ConditionIndex::Insane:
                frameIndex = 47; // distressed look
                break;
            case game::ConditionIndex::Drunk:
            case game::ConditionIndex::Afraid:
            case game::ConditionIndex::Weak:
            case game::ConditionIndex::Cursed:
                // No dedicated MM7 portrait frame; fall through to healthy.
                frameIndex = 1;
                break;
            case game::ConditionIndex::Count:
                frameIndex = 1; // healthy
                break;
            }

            char buf[16];
            std::snprintf(buf, sizeof(buf), "pc%02d-%02d", ch.faceId + 1, frameIndex);
            std::string texName(buf);
            portraitTex = lookupCached(texName, w, h);
            portraitW = w;
            portraitH = h;

            // Fallback to base name if specific frame not found
            if (!portraitTex)
            {
                std::snprintf(buf, sizeof(buf), "pc%02d", ch.faceId + 1);
                portraitTex = lookupCached(buf, w, h);
                portraitW = w;
                portraitH = h;
            }
        }

        if (portraitTex)
        {
            // Center the portrait at its natural size inside the 63x73 slot.
            // Face textures vary slightly in size, so stretching to the slot
            // distorts them; centering preserves aspect and seats them
            // correctly.
            const int natW = portraitW > 0 ? portraitW : kPortraitW;
            const int natH = portraitH > 0 ? portraitH : kPortraitH;
            const int slotX = baseX + (kPortraitW - natW) / 2;
            const int slotY = kPortraitY + (kPortraitH - natH) / 2;
            renderer.renderTexture(portraitTex, sx(slotX, scale, offsetX),
                                   sy(slotY, scale, offsetY), sw(natW, scale), sh(natH, scale));
        }
        else
        {
            // Portrait placeholder (colored rectangle based on class)
            uint8_t classHue =
                static_cast<uint8_t>((static_cast<int>(ch.charClass) * 37 + 60) % 200 + 55);
            renderer.drawFilledRect(sx(baseX, scale, offsetX), sy(kPortraitY, scale, offsetY),
                                    sw(kPortraitW, scale), sh(kPortraitH, scale), classHue,
                                    static_cast<uint8_t>(classHue / 2), 40, 180);
        }

        // Highlight the active party member (selected via portrait click).
        if (party.activeMemberIndex() == i)
        {
            renderer.drawRect(sx(baseX - 2, scale, offsetX), sy(kPortraitY - 2, scale, offsetY),
                              sw(kPortraitW + 4, scale), sh(kPortraitH + 4, scale), 255, 220, 80,
                              255);
        }

        // Name and class
        SDL_Renderer* sdl = renderer.getSDLRenderer();
        if (sdl)
        {
            int nameY = sy(kPortraitY + kPortraitH + 2, scale, offsetY);
            debugText.drawText(sdl, sx(baseX - 4, scale, offsetX), nameY, 1, 255, 255, 255,
                               ch.name);

            // Level
            std::string lvlStr = "Lv" + std::to_string(ch.level);
            debugText.drawText(sdl, sx(baseX + kPortraitW - 20, scale, offsetX), nameY, 1, 200, 200,
                               100, lvlStr);
        }

        // HP bar, left of the portrait (0x4E2A98 / y=402).
        int hpBarX = sx(kHpBarX[i], scale, offsetX);
        int hpBarY = sy(kBarY, scale, offsetY);
        int hpBarW = sw(kBarW, scale);
        int hpBarH = sh(kBarH, scale);

        renderer.drawFilledRect(hpBarX, hpBarY, hpBarW, hpBarH, 40, 10, 10, 200);

        if (ch.maxHitPoints > 0)
        {
            int hpFill = std::clamp(ch.hitPoints * hpBarH / ch.maxHitPoints, 0, hpBarH);
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
            renderer.drawFilledRect(hpBarX, hpBarY + (hpBarH - hpFill), hpBarW, hpFill, hpR, hpG,
                                    hpB, 220);
        }
        renderer.drawRect(hpBarX, hpBarY, hpBarW, hpBarH, 80, 80, 80, 255);

        // SP bar, right of the portrait (0x4E2AA8 / y=402).
        int spBarX = sx(kSpBarX[i], scale, offsetX);
        int spBarY = hpBarY;
        int spBarW = sw(kBarW, scale);
        int spBarH = sh(kBarH, scale);

        renderer.drawFilledRect(spBarX, spBarY, spBarW, spBarH, 10, 10, 40, 200);

        if (ch.maxSpellPoints > 0)
        {
            int spFill = std::clamp(ch.spellPoints * spBarH / ch.maxSpellPoints, 0, spBarH);
            renderer.drawFilledRect(spBarX, spBarY + (spBarH - spFill), spBarW, spFill, 40, 80, 220,
                                    220);
        }
        renderer.drawRect(spBarX, spBarY, spBarW, spBarH, 80, 80, 80, 255);

        // HP/SP numbers
        if (sdl)
        {
            // Simple hover/overlay text (original game shows this on hover in the status bar, we
            // show it overlaid or nearby for now) Just putting them near the bars to avoid overlap
            std::string hpStr = std::to_string(ch.hitPoints);
            std::string spStr = std::to_string(ch.spellPoints);
            debugText.drawText(sdl, hpBarX - 4, hpBarY + hpBarH + 2, 1, 255, 100, 100, hpStr);
            debugText.drawText(sdl, spBarX - 4, spBarY + spBarH + 2, 1, 100, 100, 255, spStr);
        }

        // Condition indicator
        if (!ch.isConscious())
        {
            renderer.drawFilledRect(sx(baseX, scale, offsetX), sy(kPortraitY, scale, offsetY),
                                    sw(kPortraitW, scale), sh(kPortraitH, scale), 0, 0, 0, 150);
            if (sdl)
            {
                const char* status = ch.isAlive() ? "UNCONSCIOUS" : "DEAD";
                int txtY = sy(kPortraitY + 20, scale, offsetY);
                debugText.drawText(sdl, sx(baseX + 10, scale, offsetX), txtY, 1, 255, 60, 60,
                                   status);
            }
        }

        // Spell quickbar: 2 small slots below the portrait name. RuneHarbor
        // doesn't yet model the per-character quickbar bytes (+0x1A4E/F), so
        // for now show the first 2 known spells as a read-only indicator.
        if (sdl)
        {
            const int qbarY = sy(kPortraitY + kPortraitH + 12, scale, offsetY);
            const int slotSize = sw(10, scale);
            for (int slot = 0; slot < 2; slot++)
            {
                const int slotX = sx(baseX + slot * (slotSize + 2), scale, offsetX);
                // Frame
                renderer.drawRect(slotX, qbarY, slotSize, slotSize, 90, 80, 50, 200);
                // Filled if the character has a spell assigned to this slot.
                const bool filled = (slot < game::Character::kQuickbarSlots &&
                                     ch.quickbarSpells[static_cast<size_t>(slot)] > 0);
                if (filled)
                {
                    renderer.drawFilledRect(slotX + 1, qbarY + 1, slotSize - 2, slotSize - 2, 80,
                                            60, 140, 220);
                }
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

    // Facing-direction indicator: a short line from the party dot in the
    // direction the party is looking (yaw is in MM7 turn units 0..2047).
    if (gameWorld_)
    {
        const float yaw = gameWorld_->party().yaw();
        // MM7 turn-units: 0..2047 = full circle → radians = yaw * 2π/2048 =
        // yaw * π/1024. (Was π/2048, which made the arrow point the wrong way —
        // half the correct angle.)
        const float yawRad = yaw * (3.14159265f / 1024.0f);
        const int arrowLen = static_cast<int>(8 * scale);
        const int ax = cx + static_cast<int>(std::cos(yawRad) * arrowLen);
        const int ay = cy + static_cast<int>(std::sin(yawRad) * arrowLen);
        if (sdl)
        {
            SDL_SetRenderDrawColor(sdl, 255, 255, 100, 255);
            SDL_RenderLine(sdl, static_cast<float>(cx), static_cast<float>(cy),
                           static_cast<float>(ax), static_cast<float>(ay));
        }
    }
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
