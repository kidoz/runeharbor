// SPDX-License-Identifier: MIT
#include "character_creation_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>

#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"
#include "../application.hpp"

namespace runeharbor::engine
{

namespace
{

const std::vector<std::string> kRaceNames = {"Human", "Elf", "Dwarf", "Goblin"};
const std::vector<std::string> kGenderNames = {"Male", "Female"};
const std::vector<std::string> kClassNames = {"Knight", "Paladin", "Archer", "Cleric", "Sorcerer",
                                              "Thief",  "Monk",    "Ranger", "Druid"};
const std::vector<std::string> kStatNames = {"Might", "Intellect", "Personality", "Endurance",
                                             "Speed", "Accuracy",  "Luck"};

// Race base stats: [race][stat] order: Might, Intellect, Personality, Endurance, Speed, Accuracy,
// Luck
constexpr int kRaceBaseStats[4][7] = {
    {11, 11, 11, 9, 11, 11, 9}, // Human
    {7, 14, 11, 7, 11, 14, 9},  // Elf
    {14, 11, 11, 14, 7, 7, 9},  // Dwarf
    {14, 7, 7, 11, 14, 11, 9},  // Goblin
};

constexpr int kRaceStatMax[4][7] = {
    {25, 25, 25, 25, 25, 25, 25}, // Human
    {15, 30, 25, 15, 25, 30, 20}, // Elf
    {30, 25, 25, 30, 15, 15, 20}, // Dwarf
    {30, 15, 15, 25, 30, 25, 20}, // Goblin
};

Race raceFromFace(int faceId)
{
    if (faceId < 8)
        return Race::Human;
    if (faceId < 12)
        return Race::Elf;
    if (faceId < 16)
        return Race::Dwarf;
    return Race::Goblin;
}

Gender genderFromFace(int faceId)
{
    int raceStart = 0;
    int raceCount = 8;
    if (faceId >= 16)
    {
        raceStart = 16;
        raceCount = 4;
    }
    else if (faceId >= 12)
    {
        raceStart = 12;
        raceCount = 4;
    }
    else if (faceId >= 8)
    {
        raceStart = 8;
        raceCount = 4;
    }
    int offset = faceId - raceStart;
    return offset < raceCount / 2 ? Gender::Male : Gender::Female;
}

struct ClassSkills
{
    const char* skill1;
    const char* skill2;
};

constexpr ClassSkills kClassStartingSkills[] = {
    {"Sword", "Leather Armor"}, // Knight
    {"Mace", "Spirit Magic"},   // Paladin
    {"Bow", "Air Magic"},       // Archer
    {"Mace", "Body Magic"},     // Cleric
    {"Staff", "Fire Magic"},    // Sorcerer
    {"Dagger", "Stealing"},     // Thief
    {"Dodging", "Unarmed"},     // Monk
    {"Axe", "Perception"},      // Ranger
    {"Dagger", "Earth Magic"},  // Druid
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

void CharacterCreationState::enter()
{
    activeCharacterIndex = 0;
    menuRowIndex = 0;
    isNaming = false;
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
    if (ctx.isKeyPressed(SDL_SCANCODE_1))
        activeCharacterIndex = 0;
    if (ctx.isKeyPressed(SDL_SCANCODE_2))
        activeCharacterIndex = 1;
    if (ctx.isKeyPressed(SDL_SCANCODE_3))
        activeCharacterIndex = 2;
    if (ctx.isKeyPressed(SDL_SCANCODE_4))
        activeCharacterIndex = 3;

    // Mouse click: column selection + bottom buttons
    if (ctx.window.wasMousePressed(platform::MouseButton::Left))
    {
        auto mouseState = ctx.window.getMouseState();
        int gameX = ctx.unscaleX(mouseState.x);
        int gameY = ctx.unscaleY(mouseState.y);

        constexpr int colX[] = {10, 168, 326, 484};
        constexpr int colWidth = 155;
        for (int i = 0; i < 4; i++)
        {
            if (gameX >= colX[i] && gameX < colX[i] + colWidth && gameY >= 30 && gameY < 420)
            {
                activeCharacterIndex = i;
                break;
            }
        }

        // OK button
        if (gameX >= 560 && gameX <= 620 && gameY >= 440 && gameY <= 465)
        {
            ctx.shared->quickStartReady = true;
            ctx.shared->autoLoadMap = true;
            ctx.shared->startupMapName = "";
            ctx.shared->startupPreferOutdoor = false;
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
            int oldRace = static_cast<int>(raceFromFace(activeChar.faceId));
            activeChar.faceId = (activeChar.faceId + hDelta + 20) % 20;
            int newRace = static_cast<int>(raceFromFace(activeChar.faceId));
            if (oldRace != newRace)
            {
                updateCharacterForFace(activeChar);
            }
        }
        else if (menuRowIndex == 2) // CLASS
        {
            int c = static_cast<int>(activeChar.charClass);
            c = (c + hDelta + static_cast<int>(kClassNames.size())) %
                static_cast<int>(kClassNames.size());
            activeChar.charClass = static_cast<CharacterClass>(c);
            updateSkillsForClass(activeChar);
        }
        else if (menuRowIndex >= 3 && menuRowIndex <= 9) // STATS
        {
            int statIdx = menuRowIndex - 3;
            Race race = raceFromFace(activeChar.faceId);
            int raceIdx = static_cast<int>(race);
            int minVal = activeChar.baseStats.byIndex(statIdx) - 2;
            int maxVal = kRaceStatMax[raceIdx][statIdx];

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

    if (background)
    {
        ctx.renderFullscreenTexture(background, backgroundWidth, backgroundHeight);
    }
    else if (fallbackBackground)
    {
        ctx.renderFullscreenTexture(fallbackBackground, fallbackWidth, fallbackHeight);
    }

    if (!ctx.debugText || !ctx.renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    float gameScale =
        std::min(static_cast<float>(ctx.viewportWidth) / static_cast<float>(kGameWidth),
                 static_cast<float>(ctx.viewportHeight) / static_cast<float>(kGameHeight));
    int textScale = std::max(1, static_cast<int>(gameScale));

    constexpr int colX[] = {10, 168, 326, 484};
    constexpr int colWidth = 155;

    for (int c = 0; c < 4; c++)
    {
        const Character& ch = party[c];
        bool isActive = (c == activeCharacterIndex);
        int sx = ctx.scaleX(colX[c]);

        // Highlight active column
        if (isActive)
        {
            SDL_FRect highlight = {static_cast<float>(sx), static_cast<float>(ctx.scaleY(30)),
                                   static_cast<float>(ctx.scaleW(colWidth)),
                                   static_cast<float>(ctx.scaleH(420))};
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 100, 30);
            SDL_RenderFillRect(sdlRenderer, &highlight);
        }

        // Portrait
        int portraitY = ctx.scaleY(35);
        if (ch.faceId >= 0 && ch.faceId < kPortraitCount && portraitTextures[ch.faceId])
        {
            int pw = ctx.scaleW(std::min(portraitWidths[ch.faceId], colWidth - 10));
            int ph = ctx.scaleH(portraitHeights[ch.faceId]);
            int px = sx + ctx.scaleW(colWidth / 2) - pw / 2;
            ctx.renderer->renderTexture(portraitTextures[ch.faceId], px, portraitY, pw, ph);
        }
        else
        {
            ctx.debugText->drawText(sdlRenderer, sx + ctx.scaleW(20), portraitY + ctx.scaleH(30),
                                    textScale, 100, 100, 100,
                                    std::format("[Face {}]", ch.faceId + 1));
        }

        // Face navigation arrows
        if (isActive && menuRowIndex == 1)
        {
            ctx.debugText->drawText(sdlRenderer, sx, portraitY, textScale, 255, 255, 0, "<");
            ctx.debugText->drawText(sdlRenderer, sx + ctx.scaleW(colWidth - 12), portraitY,
                                    textScale, 255, 255, 0, ">");
        }

        // Name
        int nameY = ctx.scaleY(130);
        uint8_t nr = 200, ng = 200, nb = 200;
        if (isActive && menuRowIndex == 0)
        {
            nr = 255;
            ng = 255;
            nb = 0;
        }
        std::string nameStr = ch.name;
        if (isActive && isNaming)
            nameStr += "_";
        ctx.debugText->drawText(sdlRenderer, sx, nameY, textScale, nr, ng, nb, nameStr);

        // Race + Gender
        Race race = raceFromFace(ch.faceId);
        Gender gender = genderFromFace(ch.faceId);
        std::string raceGender =
            kRaceNames[static_cast<int>(race)] + " " + kGenderNames[static_cast<int>(gender)];
        ctx.debugText->drawText(sdlRenderer, sx, ctx.scaleY(148), textScale, 180, 180, 180,
                                raceGender);

        // Class
        uint8_t cr = 200, cg = 200, cb = 200;
        if (isActive && menuRowIndex == 2)
        {
            cr = 255;
            cg = 255;
            cb = 0;
        }
        std::string classStr = kClassNames[static_cast<int>(ch.charClass)];
        if (isActive && menuRowIndex == 2)
            classStr = "< " + classStr + " >";
        ctx.debugText->drawText(sdlRenderer, sx, ctx.scaleY(166), textScale, cr, cg, cb, classStr);

        // Stats
        for (int s = 0; s < 7; s++)
        {
            int statY = ctx.scaleY(195 + s * 18);
            uint8_t sr = 180, sg = 180, sb = 180;
            if (isActive && menuRowIndex == 3 + s)
            {
                sr = 255;
                sg = 255;
                sb = 0;
            }
            std::string marker = (isActive && menuRowIndex == 3 + s) ? "> " : "  ";
            std::string statLine =
                std::format("{}{}:{}", marker, kStatNames[s].substr(0, 3), ch.stats.byIndex(s));
            ctx.debugText->drawText(sdlRenderer, sx, statY, textScale, sr, sg, sb, statLine);
        }

        // Skills
        int skillY = ctx.scaleY(325);
        for (const auto& skill : ch.skills)
        {
            ctx.debugText->drawText(sdlRenderer, sx, skillY, textScale, 150, 200, 150, skill);
            skillY += ctx.debugText->lineHeight(textScale);
        }
    }

    // Bottom controls
    int bonusPoints = calculateBonusPointsRemaining();
    uint8_t bonusR = bonusPoints > 0 ? 255 : 100;
    uint8_t bonusG = bonusPoints > 0 ? 230 : 255;
    uint8_t bonusB = 150;
    ctx.debugText->drawText(sdlRenderer, ctx.scaleX(20), ctx.scaleY(440), textScale, bonusR, bonusG,
                            bonusB, std::format("BONUS: {}", bonusPoints));

    ctx.debugText->drawText(sdlRenderer, ctx.scaleX(560), ctx.scaleY(440), textScale, 200, 255, 200,
                            "OK");
    ctx.debugText->drawText(sdlRenderer, ctx.scaleX(490), ctx.scaleY(440), textScale, 255, 200, 200,
                            "CLEAR");

    ctx.debugText->drawText(sdlRenderer, ctx.scaleX(20), ctx.scaleY(460),
                            std::max(1, textScale - 1), 150, 150, 150,
                            "1-4:Select  Arrows:Navigate  Enter:Edit name  ESC:Back");
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
    Race race = raceFromFace(ch.faceId);
    int raceIdx = static_cast<int>(race);
    for (int i = 0; i < 7; i++)
    {
        ch.baseStats.byIndex(i) = kRaceBaseStats[raceIdx][i];
    }
    ch.stats = ch.baseStats;
}

void CharacterCreationState::updateSkillsForClass(Character& ch)
{
    int classIdx = static_cast<int>(ch.charClass);
    ch.skills.clear();
    ch.skills.push_back(kClassStartingSkills[classIdx].skill1);
    ch.skills.push_back(kClassStartingSkills[classIdx].skill2);
}

} // namespace runeharbor::engine
