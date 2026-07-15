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

// Original create-party class selector order: Knight/Cleric/Archer,
// Paladin/Druid/Monk, Ranger/Sorcerer/Thief.
constexpr int kClassGridDisplayIndices[] = {0, 6, 4, 3, 7, 2, 5, 8, 1};

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
    {14, 7, 7, 11, 11, 14, 9},  // Faces 16-19 (Goblin)
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
    "Staff",    "Sword",    "Dagger",  "Axe",     "Spear",      "Bow",        "Mace",
    "Blaster",  "Shield",   "Leather", "Chain",   "Plate",      "Fire",       "Air",
    "Water",    "Earth",    "Spirit",  "Mind",    "Body",       "Light",      "Dark",
    "Identify", "Merchant", "Repair",  "Body",    "Meditation", "Perception", "Diplomacy",
    "Thievery", "Disarm",   "Dodging", "Unarmed", "Mon. Lore",  "Armsmaster", "Stealing",
    "Alchemy",  "Learning",
};

// Available additional skills per base class (indices into kSkillNames)
// These are the skills each class CAN learn, minus their 2 starting skills.
const std::vector<std::vector<int>> kClassAvailableSkills = {
    // Knight(0): Axe, Spear, Bow, Mace, Shield, Chain, Body, Perception, Armsmaster
    {3, 4, 5, 6, 8, 10, 24, 26, 33},
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

void syncCreationSkills(Character& ch)
{
    game::syncSkillLevelsFromDisplaySkills(ch);
}

void finalizeCreationCharacter(Character& ch)
{
    ch.baseStats = ch.stats;
    ch.recalculateDerived();
    ch.hitPoints = ch.maxHitPoints;
    ch.spellPoints = ch.maxSpellPoints;
    syncCreationSkills(ch);
}

const char* getFaceReadySound(int faceId)
{
    switch (faceId)
    {
    case 0:
        return "HM101a";
    case 1:
        return "HM201a";
    case 2:
        return "HM301a";
    case 3:
        return "HM401e";
    case 4:
        return "HF101a";
    case 5:
        return "HF201a";
    case 6:
        return "HF301d";
    case 7:
        return "Hf401b";
    case 8:
        return "EM101a";
    case 9:
        return "EM201b";
    case 10:
        return "Elf F101a";
    case 11:
        return "EF201a";
    case 12:
        return "DM101b";
    case 13:
        return "DM201a";
    case 14:
        return "DF101a";
    case 15:
        return "DF201c";
    case 16:
        return "GM101c";
    case 17:
        return "GM201a";
    case 18:
        return "GF101a";
    case 19:
        return "GF201a";
    default:
        return "HM101a";
    }
}

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

void CharacterCreationState::setSkyHeader(void* tex, int w, int h)
{
    skyHeader = {tex, w, h};
}

void CharacterCreationState::setTitleHeader(void* tex, int w, int h)
{
    titleHeader = {tex, w, h};
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
    menuRowIndex = 3;
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

    const auto changeFace = [this](Character& ch, int delta)
    {
        const int oldGroup = faceGroupFromId(ch.faceId);
        ch.faceId = (ch.faceId + delta + kPortraitCount) % kPortraitCount;
        const int newGroup = faceGroupFromId(ch.faceId);
        if (oldGroup != newGroup)
        {
            updateCharacterForFace(ch);
        }
    };

    const auto changeClass = [this](Character& ch, int delta)
    {
        int displayIdx = baseClassDisplayIndex(ch.charClass);
        displayIdx = (displayIdx + delta + kBaseClassCount) % kBaseClassCount;
        ch.charClass = kBaseClasses[displayIdx];
        updateSkillsForClass(ch);
        rebuildAvailableSkills();
    };

    const auto adjustStat = [this](Character& ch, int statIdx, int delta)
    {
        const int groupIdx = faceGroupFromId(ch.faceId);
        const int minVal = ch.baseStats.byIndex(statIdx) - 2;
        const int maxVal = kFaceStatMax[groupIdx][statIdx];
        if (delta > 0 && calculateBonusPointsRemaining() <= 0)
        {
            return;
        }

        int& stat = ch.stats.byIndex(statIdx);
        stat = std::clamp(stat + delta, minVal, maxVal);
    };

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
    {
        if (ctx.playUiSound)
            ctx.playUiSound(getFaceReadySound(party[activeCharacterIndex].faceId));
        rebuildAvailableSkills();
    }

    // Mouse click: column selection + bottom buttons
    if (ctx.window.wasMousePressed(platform::MouseButton::Left))
    {
        auto mouseState = ctx.window.getMouseState();
        int gameX = ctx.unscaleX(mouseState.x);
        int gameY = ctx.unscaleY(mouseState.y);

        const auto inRect = [](int x, int y, int rx, int ry, int rw, int rh)
        { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; };

        constexpr int colX[] = {8, 166, 324, 482};
        constexpr int colWidth = 153;
        for (int i = 0; i < 4; i++)
        {
            if (inRect(gameX, gameY, colX[i], 30, colWidth, 360))
            {
                if (activeCharacterIndex != i)
                {
                    activeCharacterIndex = i;
                    if (ctx.playUiSound)
                        ctx.playUiSound(getFaceReadySound(party[activeCharacterIndex].faceId));
                    rebuildAvailableSkills();
                }
                break;
            }
        }

        Character& ch = party[activeCharacterIndex];
        const int panelX = colX[activeCharacterIndex];
        const int panelCenterX = panelX + colWidth / 2;
        bool handledPanelClick = false;

        if (inRect(gameX, gameY, panelX, 120, colWidth, 22))
        {
            menuRowIndex = 0;
            isNaming = true;
            handledPanelClick = true;
        }

        if (!handledPanelClick && inRect(gameX, gameY, panelX + 70, 90, 78, 35))
        {
            menuRowIndex = 2;
            changeClass(ch, gameX < panelX + 109 ? -1 : 1);
            handledPanelClick = true;
        }

        if (!handledPanelClick && inRect(gameX, gameY, panelX, 35, colWidth, 82))
        {
            menuRowIndex = 1;
            if (gameX < panelCenterX - 20)
            {
                changeFace(ch, -1);
            }
            else if (gameX > panelCenterX + 20)
            {
                changeFace(ch, 1);
            }
            handledPanelClick = true;
        }

        constexpr int statsStartY = 169;
        constexpr int statSpacing = 17;
        for (int s = 0; s < 7; s++)
        {
            const int statY = statsStartY + s * statSpacing;
            if (!inRect(gameX, gameY, panelX, statY - 2, colWidth, statSpacing))
            {
                continue;
            }

            menuRowIndex = 3 + s;
            break;
        }

        // Class selector: original bottom 3x3 class grid.
        {
            constexpr int classGridStartX = 329;
            constexpr int classGridY = 417;
            constexpr int classGridColW = 57;
            constexpr int classGridRows = 3;
            constexpr int classGridRowH = 17;
            for (int i = 0; i < kBaseClassCount; i++)
            {
                const int col = i % 3;
                const int row = i / classGridRows;
                const int bx = classGridStartX + col * classGridColW;
                const int by = classGridY + row * classGridRowH;
                if (inRect(gameX, gameY, bx, by, classGridColW - 2, classGridRowH))
                {
                    const int displayIdx = kClassGridDisplayIndices[i];
                    if (ch.charClass != kBaseClasses[displayIdx])
                    {
                        ch.charClass = kBaseClasses[displayIdx];
                        updateSkillsForClass(ch);
                        rebuildAvailableSkills();
                    }
                    menuRowIndex = 2;
                    break;
                }
            }
        }

        // Original bottom bonus buttons adjust the currently selected stat row.
        if (menuRowIndex >= 3 && menuRowIndex <= 9)
        {
            const int statIdx = menuRowIndex - 3;
            const int minusW = minusButton.w > 0 ? minusButton.w : 12;
            const int minusH = minusButton.h > 0 ? minusButton.h : 12;
            const int plusW = plusButton.w > 0 ? plusButton.w : 12;
            const int plusH = plusButton.h > 0 ? plusButton.h : 12;
            if (inRect(gameX, gameY, 489, 402, minusW, minusH))
            {
                adjustStat(ch, statIdx, -1);
            }
            else if (inRect(gameX, gameY, 619, 402, plusW, plusH))
            {
                adjustStat(ch, statIdx, 1);
            }
        }

        // Available skill buttons use the same grid as render(): 3 rows, 3 columns.
        if (!availableSkills.empty())
        {
            constexpr int skillGridY = 417;
            constexpr int skillGridColW = 100;
            constexpr int skillGridStartX = 17;
            constexpr int skillGridRows = 3;
            constexpr int skillRowH = 17;
            constexpr size_t maxVisibleSkills = 9;
            const size_t skillCount = std::min(availableSkills.size(), maxVisibleSkills);
            for (size_t si = 0; si < skillCount; si++)
            {
                const int col = static_cast<int>(si) / skillGridRows;
                const int row = static_cast<int>(si) % skillGridRows;
                const int bx = skillGridStartX + col * skillGridColW;
                const int by = skillGridY + row * skillRowH;
                if (inRect(gameX, gameY, bx, by, skillGridColW - 2, skillRowH))
                {
                    auto& sk = availableSkills[si];
                    // Count current extra skills (beyond the 2 starting ones)
                    int extraCount = static_cast<int>(ch.skills.size()) - 2;
                    if (sk.selected)
                    {
                        // Deselect
                        sk.selected = false;
                        std::erase(ch.skills, std::string(sk.name));
                        syncCreationSkills(ch);
                        if (ctx.playUiSound)
                            ctx.playUiSound("ClickSkill");
                    }
                    else if (extraCount < kMaxExtraSkills)
                    {
                        // Select
                        sk.selected = true;
                        ch.skills.push_back(sk.name);
                        syncCreationSkills(ch);
                        if (ctx.playUiSound)
                            ctx.playUiSound("ClickSkill");
                    }
                    break;
                }
            }
        }

        // OK button
        const int okW = okButton.w > 0 ? okButton.w : 52;
        const int okH = okButton.h > 0 ? okButton.h : 34;
        if (inRect(gameX, gameY, 580, 431, okW, okH))
        {
            for (auto& ch : party)
            {
                finalizeCreationCharacter(ch);
            }

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

            if (ctx.shared->gameWorld)
            {
                auto& party = ctx.shared->gameWorld->party();
                // MM7 starts the new game at this exact coordinate on Emerald Island
                // RE doc 27-game-flow.md §7.2: X=12552, Y=1816, Z=512, angles=0
                party.setWorldPosition(12552.0f, 1816.0f, 512.0f);
                party.setOrientation(0.0f, 0.0f);

                ctx.shared->arrivalOverrideActive = true;
                ctx.shared->arrivalX = 12552.0f;
                ctx.shared->arrivalY = 1816.0f;
                ctx.shared->arrivalZ = 512.0f;
                ctx.shared->arrivalYaw = 0.0f;
            }

            ctx.shared->quickStartReady = true;
            ctx.shared->autoLoadMap = true;
            ctx.shared->startupMapName = startMap;
            ctx.shared->startupPreferOutdoor = (ext == ".odm");
            return GameStateId::Loading;
        }
        // CLEAR button
        const int clearW = clearButton.w > 0 ? clearButton.w : 52;
        const int clearH = clearButton.h > 0 ? clearButton.h : 34;
        if (inRect(gameX, gameY, 527, 431, clearW, clearH))
        {
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
            changeFace(activeChar, hDelta);
        }
        else if (menuRowIndex == 2) // CLASS
        {
            changeClass(activeChar, hDelta);
        }
        else if (menuRowIndex >= 3 && menuRowIndex <= 9) // STATS
        {
            adjustStat(activeChar, menuRowIndex - 3, hDelta);
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

    // 1. Background
    if (background)
    {
        ctx.renderFullscreenTexture(background, backgroundWidth, backgroundHeight);
    }
    else if (fallbackBackground)
    {
        ctx.renderFullscreenTexture(fallbackBackground, fallbackWidth, fallbackHeight);
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    if (!sdlRenderer)
    {
        return;
    }
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    // create.fnt is the compact body font designed for this screen. cchar.fnt is
    // reserved for the large, widely spaced title.
    auto* nameFont = ctx.createFont;
    auto* labelFont = ctx.createFont;
    auto* numFont = ctx.createFont;

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

    // 1b. Original top overlays replace the cyan color-key areas in makeme.pcx.
    drawOverlay(skyHeader, 0, 0);
    drawOverlay(titleHeader, 0, 0);

    {
        auto* headerFont = ctx.ccharFont;
        constexpr std::string_view headerText = "C R E A T E   P A R T Y";
        const int headerWidth = measureGameText(headerText, headerFont);
        drawText((kGameWidth - headerWidth) / 2, -3, headerText, headerFont, 255, 255, 255);
    }

    // Layout constants from Ghidra reverse-engineering of original MM7-Rel.exe
    // FUN_004968e2 (setup) and FUN_00495b4f (render) — 640x480 base resolution
    constexpr int colX[] = {8, 166, 324, 482}; // stride 158 (0x9E)
    constexpr int colWidth = 153;              // 0x99

    constexpr int portraitY = 35;      // 0x23 from Ghidra
    constexpr int nameY = 124;         // 0x7C from Ghidra
    constexpr int statsStartY = 169;   // 0xA9 from Ghidra
    constexpr int statSpacing = 17;    // fontHeight(arrus=19) - 2, matching original formula
    constexpr int skillsHeaderY = 291; // Original labels this region "Skills"
    constexpr int skillsStartY = 311;  // 0x137 — per-char skills below header
    constexpr int skillSpacing = 17;   // same as statSpacing
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

        // Race label in the portrait header area, matching the original's race/class pairing.
        {
            std::string_view raceStr = kFaceGroupNames[faceGroupFromId(ch.faceId)];
            drawText(nameClassX[c] + 70, 35, raceStr, labelFont, 255, 255, 235);
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
            drawText(panelX + 10, nameY, nameStr, nameFont, nr, ng, nb);
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

            // The selected stat is bracketed by inward-pointing gold arrows.
            // Bonus changes are made with the +/- controls in the bottom panel.
            if (isActive && menuRowIndex == 3 + s)
            {
                if (rightArrow.tex)
                {
                    auto* arrow = static_cast<SDL_Texture*>(rightArrow.tex);
                    SDL_SetTextureColorMod(arrow, 232, 176, 72);
                    drawOverlay(rightArrow, panelX + 2, statY + 2);
                    SDL_SetTextureColorMod(arrow, 255, 255, 255);
                }
                if (leftArrow.tex)
                {
                    auto* arrow = static_cast<SDL_Texture*>(leftArrow.tex);
                    SDL_SetTextureColorMod(arrow, 232, 176, 72);
                    drawOverlay(leftArrow, panelX + colWidth - leftArrow.w - 2, statY + 2);
                    SDL_SetTextureColorMod(arrow, 255, 255, 255);
                }
            }
        }

        // 8. Skills header below stats.
        {
            std::string_view skillsLabel = "Skills";
            int sw = measureGameText(skillsLabel, labelFont);
            int cx = panelCenterX - sw / 2;
            if (cx < panelX)
                cx = panelX;
            drawText(cx, skillsHeaderY, skillsLabel, labelFont, 220, 190, 100);
        }

        // 9. Per-character skills (four original slots: two starting skills plus two choices).
        auto* skillFont = labelFont;
        for (int slot = 0; slot < 4; slot++)
        {
            const bool hasSkill = slot < static_cast<int>(ch.skills.size());
            std::string skill = hasSkill ? ch.skills[slot] : "None";
            uint8_t sr = 255;
            uint8_t sg = 255;
            uint8_t sb = 235;
            if (!hasSkill)
            {
                sr = 0;
                sg = 220;
                sb = 220;
            }
            else if (slot >= 2)
            {
                sr = 80;
                sg = 230;
                sb = 120;
            }

            const int skillW = measureGameText(skill, skillFont);
            int skillX = panelCenterX - skillW / 2;
            if (skillX < panelX + 4)
                skillX = panelX + 4;
            drawText(skillX, skillsStartY + slot * skillSpacing, skill, skillFont, sr, sg, sb);
        }
    }

    // 10b. Bottom section headers: Available Skills, Class, Bonus.
    {
        std::string_view availLabel = "Available Skills";
        int aw = measureGameText(availLabel, labelFont);
        drawText(166 - aw / 2, 395, availLabel, labelFont, 255, 255, 200);

        std::string_view classLabel = "Class";
        int cw = measureGameText(classLabel, labelFont);
        drawText(410 - cw / 2, 395, classLabel, labelFont, 255, 255, 200);

        int bonusPoints = calculateBonusPointsRemaining();
        uint8_t bonusR = bonusPoints > 0 ? 255 : 200;
        uint8_t bonusG = bonusPoints > 0 ? 230 : 200;
        uint8_t bonusB = 150;
        drawText(529, 395, "Bonus", labelFont, bonusR, bonusG, bonusB);
        drawText(536, 417, std::to_string(bonusPoints), numFont, bonusR, bonusG, bonusB);

        if (minusButton.tex)
        {
            drawOverlay(minusButton, 489, 402);
        }
        if (plusButton.tex)
        {
            drawOverlay(plusButton, 619, 402);
        }
    }

    // 10c. Available skills grid — 3-column × 3-row at Y=417 (0x1A1)
    {
        constexpr int skillGridY = 417;     // 0x1A1 from Ghidra
        constexpr int skillGridColW = 100;  // 100px per column (original)
        constexpr int skillGridStartX = 17; // 0x11 from original
        constexpr int skillGridRows = 3;
        constexpr int skillRowH = 17; // fontHeight - 2
        constexpr size_t maxVisibleSkills = 9;
        auto* skillBtnFont = numFont;

        int extraCount = 0;
        if (ctx.shared && ctx.shared->party)
        {
            const auto& ch = (*ctx.shared->party)[activeCharacterIndex];
            extraCount = static_cast<int>(ch.skills.size()) - 2;
        }

        const size_t skillCount = std::min(availableSkills.size(), maxVisibleSkills);
        for (size_t si = 0; si < skillCount; si++)
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

    // 11. Original bottom class grid.
    {
        constexpr int classGridStartX = 329;
        constexpr int classGridY = 417;
        constexpr int classGridColW = 57;
        constexpr int classGridRows = 3;
        constexpr int classGridRowH = 17;
        auto* classGridFont = numFont;
        const Character& activeChar = party[activeCharacterIndex];

        for (int i = 0; i < kBaseClassCount; i++)
        {
            const int displayIdx = kClassGridDisplayIndices[i];
            const int col = i % 3;
            const int row = i / classGridRows;
            const int bx = classGridStartX + col * classGridColW;
            const int by = classGridY + row * classGridRowH;
            const bool selected = activeChar.charClass == kBaseClasses[displayIdx];
            const uint8_t cr = selected ? 0 : 255;
            const uint8_t cg = selected ? 220 : 255;
            const uint8_t cb = selected ? 220 : 235;
            drawText(bx, by + 1, kBaseClassNames[displayIdx], classGridFont, cr, cg, cb);
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

    // Approximate MM7 gender mapping
    // Human (0-7): 0-3 Male, 4-7 Female
    // Elf (8-11): 8-9 Male, 10-11 Female
    // Dwarf (12-15): 12-13 Male, 14-15 Female
    // Goblin (16-19): 16-17 Male, 18-19 Female
    if (ch.faceId < 4 || (ch.faceId >= 8 && ch.faceId < 10) ||
        (ch.faceId >= 12 && ch.faceId < 14) || (ch.faceId >= 16 && ch.faceId < 18))
    {
        ch.gender = Gender::Male;
    }
    else
    {
        ch.gender = Gender::Female;
    }
}

void CharacterCreationState::updateSkillsForClass(Character& ch)
{
    int classIdx = baseClassIndex(ch.charClass);
    ch.skills.clear();
    ch.skills.push_back(kClassStartingSkills[classIdx].skill1);
    ch.skills.push_back(kClassStartingSkills[classIdx].skill2);
    syncCreationSkills(ch);
}

} // namespace runeharbor::engine
