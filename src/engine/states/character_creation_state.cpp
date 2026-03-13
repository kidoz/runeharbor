// SPDX-License-Identifier: MIT
#include "character_creation_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>

#include <cctype>

#include "../../graphics/bitmap_font.hpp"
#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"
#include "../application.hpp"

namespace runeharbor::engine
{

namespace
{

const std::vector<std::string> kFaceGroupNames = {"Human", "Elf", "Dwarf", "Goblin"};

// 9 base class names (used for cycling during character creation)
const std::vector<std::string> kBaseClassNames = {"Knight", "Thief",  "Monk",  "Paladin", "Archer",
                                                  "Ranger", "Cleric", "Druid", "Sorcerer"};

// Maps base class display index (0-8) to CharacterClass enum value
constexpr CharacterClass kBaseClasses[] = {
    CharacterClass::Knight,  CharacterClass::Thief,  CharacterClass::Monk,
    CharacterClass::Paladin, CharacterClass::Archer, CharacterClass::Ranger,
    CharacterClass::Cleric,  CharacterClass::Druid,  CharacterClass::Sorcerer,
};
constexpr int kBaseClassCount = 9;

// Reverse: find display index from CharacterClass
int baseClassDisplayIndex(CharacterClass c)
{
    for (int i = 0; i < kBaseClassCount; i++)
    {
        if (kBaseClasses[i] == c)
            return i;
    }
    return 0;
}

const std::vector<std::string> kStatNames = {"Might",    "Intellect", "Personality", "Endurance",
                                             "Accuracy", "Speed",     "Luck"};

// Face group base stats
constexpr int kFaceBaseStats[4][7] = {
    {11, 11, 11, 9, 11, 11, 9}, // Faces 0-7 (Human)
    {7, 14, 11, 7, 11, 14, 9},  // Faces 8-11 (Elf)
    {14, 11, 11, 14, 7, 7, 9},  // Faces 12-15 (Dwarf)
    {14, 7, 7, 11, 14, 11, 9},  // Faces 16-19 (Goblin)
};

constexpr int kFaceStatMax[4][7] = {
    {25, 25, 25, 25, 25, 25, 25}, // Faces 0-7
    {15, 30, 25, 15, 25, 30, 20}, // Faces 8-11
    {30, 25, 25, 30, 15, 15, 20}, // Faces 12-15
    {30, 15, 15, 25, 30, 25, 20}, // Faces 16-19
};

int faceGroupFromId(int faceId)
{
    if (faceId < 8)
        return 0;
    if (faceId < 12)
        return 1;
    if (faceId < 16)
        return 2;
    return 3;
}

std::string getLowerExtension(const std::string& filename)
{
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos)
    {
        return "";
    }

    std::string ext = filename.substr(dot);
    for (char& c : ext)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

struct ClassSkills
{
    const char* skill1;
    const char* skill2;
};

constexpr ClassSkills kClassStartingSkills[] = {
    {"Sword", "Leather Armor"}, // Knight (base 0)
    {"Dagger", "Stealing"},     // Thief (base 1)
    {"Dodging", "Unarmed"},     // Monk (base 2)
    {"Mace", "Spirit Magic"},   // Paladin (base 3)
    {"Bow", "Air Magic"},       // Archer (base 4)
    {"Axe", "Perception"},      // Ranger (base 5)
    {"Mace", "Body Magic"},     // Cleric (base 6)
    {"Dagger", "Earth Magic"},  // Druid (base 7)
    {"Staff", "Fire Magic"},    // Sorcerer (base 8)
};

// Skill display names (indexed by SkillId-like order)
const char* kSkillNames[] = {
    "Staff",    "Sword",    "Dagger",  "Axe",      "Spear",      "Bow",        "Mace",
    "Blaster",  "Shield",   "Leather", "Chain",    "Plate",      "Fire",       "Air",
    "Water",    "Earth",    "Spirit",  "Mind",     "Body",       "Light",      "Dark",
    "Identify", "Merchant", "Repair",  "Body Bld", "Meditation", "Perception", "Diplomacy",
    "Thievery", "Disarm",   "Dodging", "Unarmed",  "Mon. Lore",  "Armsmaster", "Stealing",
    "Alchemy",  "Learning",
};

// Available additional skills per base class (indices into kSkillNames)
// These are the skills each class CAN learn, minus their 2 starting skills.
const std::vector<std::vector<int>> kClassAvailableSkills = {
    // Knight(0): Axe, Mace, Bow, Shield, Chain, Plate, BodyBld, Repair, Armsmaster, Perception
    {3, 6, 5, 8, 10, 11, 24, 23, 33, 26},
    // Thief(1): Sword, Bow, Mace, Leather, Shield, DisarmTrap, Perception, Merchant, Dodging
    {1, 5, 6, 9, 8, 29, 26, 22, 30},
    // Monk(2): Staff, Mace, Leather, BodyBld, Meditation, Spirit, Mind, Body, Perception, Learning
    {0, 6, 9, 24, 25, 16, 17, 18, 26, 36},
    // Paladin(3): Sword, Shield, Leather, Chain, Plate, Mind, Body, BodyBld, Diplomacy, Repair
    {1, 8, 9, 10, 11, 17, 18, 24, 27, 23},
    // Archer(4): Dagger, Mace, Leather, Chain, Fire, Water, Earth, Perception, DisarmTrap
    {2, 6, 9, 10, 12, 14, 15, 26, 29},
    // Ranger(5): Sword, Mace, Bow, Shield, Leather, Chain, Earth, Water, Alchemy, Dodging
    {1, 6, 5, 8, 9, 10, 15, 14, 35, 30},
    // Cleric(6): Shield, Leather, Chain, Spirit, Mind, Light, Dark, BodyBld, Meditation, Diplomacy
    {8, 9, 10, 16, 17, 19, 20, 24, 25, 27},
    // Druid(7): Mace, Staff, Leather, Fire, Air, Water, Meditation, Alchemy, Learning, Dodging
    {6, 0, 9, 12, 13, 14, 25, 35, 36, 30},
    // Sorcerer(8): Dagger, Leather, Air, Water, Earth, Light, Dark, Meditation, Learning, Alchemy
    {2, 9, 13, 14, 15, 19, 20, 25, 36, 35},
};

} // namespace

CharacterCreationState::CharacterCreationState(StateContext& ctx) : ctx(ctx) {}

CharacterCreationState::~CharacterCreationState() = default;

void CharacterCreationState::setBackground(void* tex, int w, int h)
{
    background = tex;
    backgroundWidth = w;
    backgroundHeight = h;
}

void CharacterCreationState::setFallbackBackground(void* tex, int w, int h)
{
    fallbackBackground = tex;
    fallbackWidth = w;
    fallbackHeight = h;
}

void CharacterCreationState::setPortraitTexture(int index, void* tex, int w, int h)
{
    if (index >= 0 && index < kPortraitCount)
    {
        portraitTextures[index] = tex;
        portraitWidths[index] = w;
        portraitHeights[index] = h;
    }
}

void CharacterCreationState::setFaceMask(void* tex, int w, int h)
{
    faceMask = {tex, w, h};
}
void CharacterCreationState::setOkButton(void* tex, int w, int h)
{
    okButton = {tex, w, h};
}
void CharacterCreationState::setClearButton(void* tex, int w, int h)
{
    clearButton = {tex, w, h};
}
void CharacterCreationState::setMinusButton(void* tex, int w, int h)
{
    minusButton = {tex, w, h};
}
void CharacterCreationState::setPlusButton(void* tex, int w, int h)
{
    plusButton = {tex, w, h};
}
void CharacterCreationState::setLeftArrow(void* tex, int w, int h)
{
    leftArrow = {tex, w, h};
}
void CharacterCreationState::setRightArrow(void* tex, int w, int h)
{
    rightArrow = {tex, w, h};
}
void CharacterCreationState::setClassIcon(int index, void* tex, int w, int h)
{
    if (index >= 0 && index < kClassIconCount)
    {
        classIcons[index] = {tex, w, h};
    }
}

void CharacterCreationState::enter()
{
    activeCharacterIndex = 0;
    menuRowIndex = 0;
    isNaming = false;
    rebuildAvailableSkills();
}

void CharacterCreationState::rebuildAvailableSkills()
{
    availableSkills.clear();
    if (!ctx.shared || !ctx.shared->party)
    {
        return;
    }
    auto& ch = (*ctx.shared->party)[activeCharacterIndex];
    int classIdx = baseClassDisplayIndex(ch.charClass);
    if (classIdx < 0 || classIdx >= static_cast<int>(kClassAvailableSkills.size()))
    {
        return;
    }
    const auto& skillIndices = kClassAvailableSkills[classIdx];
    for (int idx : skillIndices)
    {
        if (idx >= 0 && idx < static_cast<int>(std::size(kSkillNames)))
        {
            // Check if already selected by this character
            bool sel = false;
            for (size_t s = 2; s < ch.skills.size(); s++)
            {
                if (ch.skills[s] == kSkillNames[idx])
                {
                    sel = true;
                    break;
                }
            }
            availableSkills.push_back({kSkillNames[idx], sel});
        }
    }
}

void CharacterCreationState::exit() {}

std::optional<GameStateId> CharacterCreationState::update()
{
    if (!ctx.shared || !ctx.shared->party)
    {
        return std::nullopt;
    }
    auto& party = *ctx.shared->party;

    // Select active character with 1-4
    int prevActive = activeCharacterIndex;
    if (ctx.isKeyPressed(SDL_SCANCODE_1))
        activeCharacterIndex = 0;
    if (ctx.isKeyPressed(SDL_SCANCODE_2))
        activeCharacterIndex = 1;
    if (ctx.isKeyPressed(SDL_SCANCODE_3))
        activeCharacterIndex = 2;
    if (ctx.isKeyPressed(SDL_SCANCODE_4))
        activeCharacterIndex = 3;
    if (activeCharacterIndex != prevActive)
        rebuildAvailableSkills();

    // Mouse click: column selection + bottom buttons
    if (ctx.window.wasMousePressed(platform::MouseButton::Left))
    {
        auto mouseState = ctx.window.getMouseState();
        int gameX = ctx.unscaleX(mouseState.x);
        int gameY = ctx.unscaleY(mouseState.y);

        constexpr int colX[] = {10, 163, 321, 478};
        constexpr int colWidth = 150;
        for (int i = 0; i < 4; i++)
        {
            if (gameX >= colX[i] && gameX < colX[i] + colWidth && gameY >= 30 && gameY < 420)
            {
                if (activeCharacterIndex != i)
                {
                    activeCharacterIndex = i;
                    rebuildAvailableSkills();
                }
                break;
            }
        }

        // Available skill buttons at bottom (Y=368..390, each ~60px wide)
        if (gameY >= 368 && gameY <= 400 && !availableSkills.empty())
        {
            constexpr int skillBtnY = 368;
            constexpr int skillBtnW = 60;
            constexpr int skillBtnH = 28;
            constexpr int skillStartX = 8;
            int skillsPerRow = (620 - skillStartX) / skillBtnW;
            int totalSkills = static_cast<int>(availableSkills.size());
            for (int si = 0; si < totalSkills && si < skillsPerRow; si++)
            {
                int bx = skillStartX + si * skillBtnW;
                if (gameX >= bx && gameX < bx + skillBtnW - 2 && gameY >= skillBtnY &&
                    gameY < skillBtnY + skillBtnH)
                {
                    auto& sk = availableSkills[si];
                    Character& ch = party[activeCharacterIndex];
                    // Count current extra skills (beyond the 2 starting ones)
                    int extraCount = static_cast<int>(ch.skills.size()) - 2;
                    if (sk.selected)
                    {
                        // Deselect
                        sk.selected = false;
                        std::erase(ch.skills, std::string(sk.name));
                    }
                    else if (extraCount < kMaxExtraSkills)
                    {
                        // Select
                        sk.selected = true;
                        ch.skills.push_back(sk.name);
                    }
                    break;
                }
            }
        }

        // OK button
        if (gameX >= 560 && gameX <= 620 && gameY >= 440 && gameY <= 465)
        {
            std::string startMap = "out01.odm";
            if (!ctx.shared->newGameStartMapName.empty())
            {
                startMap = ctx.shared->newGameStartMapName;
            }

            std::string ext = getLowerExtension(startMap);
            if (ext.empty())
            {
                startMap += ".odm";
                ext = ".odm";
            }

            ctx.shared->quickStartReady = true;
            ctx.shared->autoLoadMap = true;
            ctx.shared->startupMapName = startMap;
            ctx.shared->startupPreferOutdoor = (ext == ".odm");
            return GameStateId::Loading;
        }
        // CLEAR button
        if (gameX >= 490 && gameX <= 555 && gameY >= 440 && gameY <= 465)
        {
            Character& ch = party[activeCharacterIndex];
            ch.stats = ch.baseStats;
        }
    }

    Character& activeChar = party[activeCharacterIndex];

    // Naming mode: capture text input
    if (isNaming)
    {
        if (ctx.isKeyPressed(SDL_SCANCODE_BACKSPACE) && !activeChar.name.empty())
        {
            activeChar.name.pop_back();
        }
        else if (ctx.isKeyPressed(SDL_SCANCODE_RETURN) || ctx.isKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            isNaming = false;
        }
        for (int i = SDL_SCANCODE_A; i <= SDL_SCANCODE_Z; i++)
        {
            if (ctx.isKeyPressed(static_cast<SDL_Scancode>(i)))
            {
                if (activeChar.name.size() < 15)
                {
                    char c = 'A' + (i - SDL_SCANCODE_A);
                    activeChar.name += c;
                }
            }
        }
        return std::nullopt;
    }

    // Row navigation
    if (ctx.isKeyPressed(SDL_SCANCODE_UP))
    {
        menuRowIndex = (menuRowIndex + kRowCount - 1) % kRowCount;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_DOWN))
    {
        menuRowIndex = (menuRowIndex + 1) % kRowCount;
    }

    // Horizontal navigation
    int hDelta = 0;
    if (ctx.isKeyPressed(SDL_SCANCODE_LEFT))
        hDelta = -1;
    if (ctx.isKeyPressed(SDL_SCANCODE_RIGHT))
        hDelta = 1;

    if (hDelta != 0)
    {
        if (menuRowIndex == 1) // FACE
        {
            int oldGroup = faceGroupFromId(activeChar.faceId);
            activeChar.faceId = (activeChar.faceId + hDelta + 20) % 20;
            int newGroup = faceGroupFromId(activeChar.faceId);
            if (oldGroup != newGroup)
            {
                updateCharacterForFace(activeChar);
            }
        }
        else if (menuRowIndex == 2) // CLASS
        {
            int displayIdx = baseClassDisplayIndex(activeChar.charClass);
            displayIdx = (displayIdx + hDelta + kBaseClassCount) % kBaseClassCount;
            activeChar.charClass = kBaseClasses[displayIdx];
            updateSkillsForClass(activeChar);
            rebuildAvailableSkills();
        }
        else if (menuRowIndex >= 3 && menuRowIndex <= 9) // STATS
        {
            int statIdx = menuRowIndex - 3;
            int groupIdx = faceGroupFromId(activeChar.faceId);
            int minVal = activeChar.baseStats.byIndex(statIdx) - 2;
            int maxVal = kFaceStatMax[groupIdx][statIdx];

            if (hDelta > 0 && calculateBonusPointsRemaining() <= 0)
            {
                // No bonus points left
            }
            else
            {
                int& stat = activeChar.stats.byIndex(statIdx);
                stat = std::clamp(stat + hDelta, minVal, maxVal);
            }
        }
    }

    // Enter key
    if (ctx.isKeyPressed(SDL_SCANCODE_RETURN))
    {
        if (menuRowIndex == 0) // NAME
        {
            isNaming = true;
        }
    }

    // ESC to go back
    if (ctx.isKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        return GameStateId::TitleScreen;
    }

    return std::nullopt;
}

void CharacterCreationState::render()
{
    if (!ctx.renderer || !ctx.shared || !ctx.shared->party)
    {
        return;
    }
    auto& party = *ctx.shared->party;

    // 1. Background — dim to approximate VGA 6-bit DAC (original rendered through
    //    256-color palette with 6-bit output, producing ~63% brightness vs true-color)
    if (background)
    {
        SDL_SetTextureColorMod(static_cast<SDL_Texture*>(background), 160, 160, 160);
        ctx.renderFullscreenTexture(background, backgroundWidth, backgroundHeight);
        SDL_SetTextureColorMod(static_cast<SDL_Texture*>(background), 255, 255, 255);
    }
    else if (fallbackBackground)
    {
        SDL_SetTextureColorMod(static_cast<SDL_Texture*>(fallbackBackground), 160, 160, 160);
        ctx.renderFullscreenTexture(fallbackBackground, fallbackWidth, fallbackHeight);
        SDL_SetTextureColorMod(static_cast<SDL_Texture*>(fallbackBackground), 255, 255, 255);
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    if (!sdlRenderer)
    {
        return;
    }
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    // Font pointers (may be null if .fnt files weren't found)
    // Use arrus (h=19) for character names — cchar (h=29) is too tall and overlaps
    // the race/gender line below. The original game uses a compact name display.
    auto* nameFont = ctx.arrusFont;
    auto* labelFont = ctx.arrusFont;
    auto* numFont = ctx.smallnumFont ? ctx.smallnumFont : ctx.arrusFont;

    // Scale factor for debug text fallback
    float gameScale =
        std::min(static_cast<float>(ctx.viewportWidth) / static_cast<float>(kGameWidth),
                 static_cast<float>(ctx.viewportHeight) / static_cast<float>(kGameHeight));
    int textScale = std::max(1, static_cast<int>(gameScale));

    // Helper lambda: render text with bitmap font (scaled), fallback to debug text
    auto drawText = [&](int gameX, int gameY, std::string_view text, graphics::BitmapFont* font,
                        uint8_t r, uint8_t g, uint8_t b)
    {
        if (font && font->hasAtlas())
        {
            int sx = ctx.scaleX(gameX);
            int sy = ctx.scaleY(gameY);
            font->renderText(sdlRenderer, sx, sy, gameScale, text, r, g, b);
        }
        else if (ctx.debugText)
        {
            ctx.debugText->drawText(sdlRenderer, ctx.scaleX(gameX), ctx.scaleY(gameY), textScale, r,
                                    g, b, text);
        }
    };

    // Helper: measure text width in game coordinates (unscaled, for centering)
    auto measureGameText = [&](std::string_view text, graphics::BitmapFont* font) -> int
    {
        if (font && font->isLoaded())
        {
            return font->measureText(text);
        }
        return 0;
    };

    // Helper: render overlay texture at game coords
    auto drawOverlay = [&](const TexRef& ref, int gameX, int gameY)
    {
        if (ref.tex)
        {
            ctx.renderer->renderTexture(ref.tex, ctx.scaleX(gameX), ctx.scaleY(gameY),
                                        ctx.scaleW(ref.w), ctx.scaleH(ref.h));
        }
    };

    // 1b. "CREATE PARTY" header at top center
    {
        auto* headerFont = ctx.createFont;
        std::string_view headerText = "CREATE PARTY";
        int headerW = measureGameText(headerText, headerFont);
        int headerX = (kGameWidth - headerW) / 2;
        drawText(headerX, 4, headerText, headerFont, 255, 255, 255);
    }

    // Layout constants from Ghidra reverse-engineering of original MM7-Rel.exe
    // FUN_004968e2 (setup) and FUN_00495b4f (render) — 640x480 base resolution
    constexpr int colX[] = {8, 166, 324, 482}; // stride 158 (0x9E)
    constexpr int colWidth = 153;              // 0x99

    constexpr int portraitY = 35;     // 0x23 from Ghidra
    constexpr int nameY = 124;        // 0x7C from Ghidra
    constexpr int statsStartY = 169;  // 0xA9 from Ghidra
    constexpr int statSpacing = 17;   // fontHeight(arrus=19) - 2, matching original formula
    constexpr int classLabelY = 291;  // 0x123 — class display below all 7 stats
    constexpr int skillsStartY = 311; // 0x137 — per-char skills below class label
    constexpr int skillSpacing = 17;  // same as statSpacing
    // Ghidra: class name overlaid on portrait right side; class icon also on portrait right
    constexpr int nameClassX[] = {18, 177, 336, 495}; // local_13c stride 0x9F
    constexpr int classOverlayY = 100;                // class name on portrait (Ghidra line 98)
    constexpr int classIconY = 50; // 0x32 — class icon on portrait (Ghidra line 99)
    constexpr int faceMaskY = 29;  // 0x1D — FACEMASK for selected char
    constexpr int faceMaskX[] = {12, 171, 329, 488}; // local_120 values

    for (int c = 0; c < 4; c++)
    {
        const Character& ch = party[c];
        bool isActive = (c == activeCharacterIndex);
        int panelX = colX[c];
        int panelCenterX = panelX + colWidth / 2;

        // 2. Portrait — use Ghidra exact X positions: 17, 176, 335, 494 (stride 159)
        constexpr int portraitX[] = {17, 176, 335, 494}; // 0x11, 0xB0, 0x14F, 0x1EE
        if (ch.faceId >= 0 && ch.faceId < kPortraitCount && portraitTextures[ch.faceId])
        {
            int pw = portraitWidths[ch.faceId];
            int ph = portraitHeights[ch.faceId];
            // Render portrait at natural size, centered at the Ghidra X position
            int px = portraitX[c];
            ctx.renderer->renderTexture(portraitTextures[ch.faceId], ctx.scaleX(px),
                                        ctx.scaleY(portraitY), ctx.scaleW(pw), ctx.scaleH(ph));
        }
        else if (ctx.debugText)
        {
            ctx.debugText->drawText(sdlRenderer, ctx.scaleX(panelX + 20),
                                    ctx.scaleY(portraitY + 30), textScale, 100, 100, 100,
                                    std::format("[Face {}]", ch.faceId + 1));
        }

        // 3. Face mask overlay for selected character (Ghidra: FUN_004a6204 at local_120, 0x1D)
        if (isActive && faceMask.tex)
        {
            drawOverlay(faceMask, faceMaskX[c], faceMaskY);
        }

        // 4. Portrait navigation arrows (when face row is selected)
        //    Original: face arrows at Y=103 (0x67), class arrows at Y=32 (0x20)
        if (isActive && menuRowIndex == 1)
        {
            constexpr int faceArrowY = 103; // 0x67 from original
            if (leftArrow.tex)
            {
                drawOverlay(leftArrow, panelX + 2, faceArrowY);
            }
            else if (ctx.debugText)
            {
                ctx.debugText->drawText(sdlRenderer, ctx.scaleX(panelX), ctx.scaleY(faceArrowY),
                                        textScale, 255, 255, 0, "<");
            }
            if (rightArrow.tex)
            {
                drawOverlay(rightArrow, panelX + colWidth - rightArrow.w - 2, faceArrowY);
            }
            else if (ctx.debugText)
            {
                ctx.debugText->drawText(sdlRenderer, ctx.scaleX(panelX + colWidth - 12),
                                        ctx.scaleY(faceArrowY), textScale, 255, 255, 0, ">");
            }
        }

        // 5. Character name
        {
            uint8_t nr = 255, ng = 255, nb = 235;
            if (isActive && menuRowIndex == 0)
            {
                nr = 255;
                ng = 255;
                nb = 0;
            }
            std::string nameStr = ch.name;
            if (isActive && isNaming)
                nameStr += "_";
            // Center the name within the panel
            int nameW = measureGameText(nameStr, nameFont);
            int nameX = panelCenterX - nameW / 2;
            if (nameX < panelX)
                nameX = panelX;
            drawText(nameX, nameY, nameStr, nameFont, nr, ng, nb);
        }

        // 6a. Class icon in portrait area (Ghidra: local_13c + 0x4D, Y=0x32)
        {
            int displayIdx = baseClassDisplayIndex(ch.charClass);
            if (displayIdx >= 0 && displayIdx < kClassIconCount && classIcons[displayIdx].tex)
            {
                int ciX = nameClassX[c] + 77; // local_13c + 0x4D
                drawOverlay(classIcons[displayIdx], ciX, classIconY);
            }
        }

        // 6b. Class name overlay on portrait right side (Ghidra: local_13c + 0x49, Y=100)
        {
            uint8_t cr = 255, cg = 255, cb = 235;
            if (isActive && menuRowIndex == 2)
            {
                cr = 255;
                cg = 255;
                cb = 0;
            }
            int displayIdx = baseClassDisplayIndex(ch.charClass);
            std::string classStr = kBaseClassNames[displayIdx];
            if (isActive && menuRowIndex == 2)
                classStr = "< " + classStr + " >";
            int classTextX = nameClassX[c] + 73; // local_13c + 0x49
            drawText(classTextX, classOverlayY, classStr, labelFont, cr, cg, cb);
        }

        // 7. Stats
        for (int s = 0; s < 7; s++)
        {
            int statY = statsStartY + s * statSpacing;
            uint8_t sr = 255, sg = 255, sb = 235;
            if (isActive && menuRowIndex == 3 + s)
            {
                sr = 255;
                sg = 255;
                sb = 0;
            }

            // Stat label on the left
            drawText(panelX + 6, statY, kStatNames[s], labelFont, sr, sg, sb);

            // Stat value right-aligned with consistent margin
            std::string valStr = std::to_string(ch.stats.byIndex(s));
            int valW = measureGameText(valStr, numFont);
            int valX = panelX + colWidth - valW - 10;
            drawText(valX, statY, valStr, numFont, sr, sg, sb);

            // +/- buttons for active stat row
            if (isActive && menuRowIndex == 3 + s)
            {
                if (minusButton.tex)
                {
                    drawOverlay(minusButton, panelX + colWidth - 30 - minusButton.w, statY);
                }
                if (plusButton.tex)
                {
                    drawOverlay(plusButton, panelX + colWidth - 14, statY);
                }
            }
        }

        // 8. Class label below stats (Ghidra: Y=0x123=291, X=colX[c])
        {
            int displayIdx = baseClassDisplayIndex(ch.charClass);
            std::string classStr = kBaseClassNames[displayIdx];
            int cw = measureGameText(classStr, labelFont);
            int cx = panelCenterX - cw / 2;
            if (cx < panelX)
                cx = panelX;
            drawText(cx, classLabelY, classStr, labelFont, 255, 255, 235);
        }

        // 9. Per-character skills (Ghidra: Y=0x137=311, spacing=statSpacing)
        auto* skillFont = labelFont;
        int skillY = skillsStartY;
        for (const auto& skill : ch.skills)
        {
            drawText(panelX + 6, skillY, skill, skillFont, 150, 230, 150);
            skillY += skillSpacing;
        }
    }

    // 10b. Stat points remaining + "Available Skills" header
    //    Ghidra: "stat points" left label at FUN_0044c52e+0x25 (~37), Y=0x18B (395)
    //    "Available Skills" label centered at same Y level
    {
        // Stat points (bonus) on left — Ghidra line 355
        int bonusPoints = calculateBonusPointsRemaining();
        uint8_t bonusR = bonusPoints > 0 ? 255 : 200;
        uint8_t bonusG = bonusPoints > 0 ? 230 : 200;
        uint8_t bonusB = 150;
        drawText(37, 395, std::format("Bonus  {}", bonusPoints), labelFont, bonusR, bonusG, bonusB);

        // "Available Skills" label — centered
        std::string_view availLabel = "Available Skills";
        int aw = measureGameText(availLabel, labelFont);
        int ax = (kGameWidth - aw) / 2;
        drawText(ax, 395, availLabel, labelFont, 255, 255, 200);
    }

    // 10c. Available skills grid — 3-column × 3-row at Y=417 (0x1A1)
    {
        constexpr int skillGridY = 417;     // 0x1A1 from Ghidra
        constexpr int skillGridColW = 100;  // 100px per column (original)
        constexpr int skillGridStartX = 17; // 0x11 from original
        constexpr int skillGridRows = 3;
        constexpr int skillRowH = 17; // fontHeight - 2
        auto* skillBtnFont = numFont;

        int extraCount = 0;
        if (ctx.shared && ctx.shared->party)
        {
            const auto& ch = (*ctx.shared->party)[activeCharacterIndex];
            extraCount = static_cast<int>(ch.skills.size()) - 2;
        }

        for (size_t si = 0; si < availableSkills.size(); si++)
        {
            const auto& sk = availableSkills[si];
            int col = static_cast<int>(si) / skillGridRows;
            int row = static_cast<int>(si) % skillGridRows;
            int bx = skillGridStartX + col * skillGridColW;
            int by = skillGridY + row * skillRowH;

            // Background highlight for selected skills
            if (sk.selected)
            {
                SDL_FRect btnRect = {static_cast<float>(ctx.scaleX(bx)),
                                     static_cast<float>(ctx.scaleY(by)),
                                     static_cast<float>(ctx.scaleW(skillGridColW - 2)),
                                     static_cast<float>(ctx.scaleH(skillRowH))};
                SDL_SetRenderDrawColor(sdlRenderer, 80, 120, 80, 180);
                SDL_RenderFillRect(sdlRenderer, &btnRect);
            }

            // Skill name
            uint8_t sr = sk.selected ? 100 : 255;
            uint8_t sg = sk.selected ? 255 : 255;
            uint8_t sb = sk.selected ? 100 : 235;
            // Dim skills that can't be selected (already at max extra)
            if (!sk.selected && extraCount >= kMaxExtraSkills)
            {
                sr = 120;
                sg = 120;
                sb = 120;
            }
            drawText(bx + 2, by + 1, sk.name, skillBtnFont, sr, sg, sb);
        }
    }

    // 12. Bottom controls: OK + Clear buttons (from Ghidra: OK at 580,431; Clear at 527,431)
    if (okButton.tex)
    {
        drawOverlay(okButton, 580, 431);
    }
    else
    {
        drawText(580, 441, "OK", labelFont, 200, 255, 200);
    }

    if (clearButton.tex)
    {
        drawOverlay(clearButton, 527, 431);
    }
    else
    {
        drawText(527, 441, "Clear", labelFont, 255, 200, 200);
    }
}

int CharacterCreationState::calculateBonusPointsRemaining() const
{
    if (!ctx.shared || !ctx.shared->party)
        return 0;
    int totalSpent = 0;
    for (const auto& ch : *ctx.shared->party)
    {
        for (int i = 0; i < 7; i++)
        {
            totalSpent += ch.stats.byIndex(i) - ch.baseStats.byIndex(i);
        }
    }
    return 50 - totalSpent;
}

void CharacterCreationState::updateCharacterForFace(Character& ch)
{
    int groupIdx = faceGroupFromId(ch.faceId);
    for (int i = 0; i < 7; i++)
    {
        ch.baseStats.byIndex(i) = kFaceBaseStats[groupIdx][i];
    }
    ch.stats = ch.baseStats;
}

void CharacterCreationState::updateSkillsForClass(Character& ch)
{
    int classIdx = baseClassIndex(ch.charClass);
    ch.skills.clear();
    ch.skills.push_back(kClassStartingSkills[classIdx].skill1);
    ch.skills.push_back(kClassStartingSkills[classIdx].skill2);
}

} // namespace runeharbor::engine
