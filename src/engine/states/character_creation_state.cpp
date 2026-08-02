// SPDX-License-Identifier: MIT
#include "character_creation_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <format>

#include <cctype>
#include <climits>

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

// Bonus-panel geometry. The -/+ hit rects are the original's button rects from
// fcn.004968e2 (MM7-Rel.exe 0x497440 and 0x497469), which also register '-' and
// '+' as hotkeys.
constexpr int kBonusPanelCenterX = 577; // center of the value panel
constexpr int kBonusMinusX = 523;
constexpr int kBonusPlusX = 613;
constexpr int kBonusButtonY = 393;
constexpr int kBonusButtonW = 20;
constexpr int kBonusButtonH = 35;
// The value box spans game-y ~393-427 (Clear/OK start at 428). create.fnt is 18px
// tall, so the label and count stack tight near the top to avoid the buttons.
constexpr int kBonusLabelY = 393; // "Bonus" label row (below the panel top border)
constexpr int kBonusValueY = 410; // remaining-points count row

// Per-character column hit rects (0x49701D-0x497091): x = 5/163/321/479 with a
// 158px stride, y = 21, 153x365. Hotkeys '1'-'4' select the same columns.
constexpr int kColumnX[] = {5, 163, 321, 479};
constexpr int kColumnY = 21;
constexpr int kColumnW = 153;
constexpr int kColumnH = 365;

// Class selection grid (0x497217-0x49733A): three 65x17 cells per row, columns
// at x = 323/388/453 and rows at y = 417/434/451.
constexpr int kClassGridStartX = 323;
constexpr int kClassGridY = 417;
constexpr int kClassGridColW = 65;
constexpr int kClassGridRows = 3;
constexpr int kClassGridRowH = 17;

// OK / Clear (0x4973D5 and 0x497411), both 51x39 at y=431 with Enter and 'C'
// as hotkeys.
constexpr int kOkButtonX = 580;
constexpr int kClearButtonX = 527;
constexpr int kBottomButtonY = 431;
constexpr int kBottomButtonW = 51;
constexpr int kBottomButtonH = 39;

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

// Display names for every skill, indexed by SkillId. Magic schools use the
// full "<School> Magic" form and BodyBuilding its full name so that every entry
// is unique — the skill resolver (game::skillIdFromName) normalizes case and
// strips non-alphanumerics, so bare "Fire" and "Fire Magic" both resolve to
// SkillId::Fire, but a duplicate bare "Body" collapsed BodyBuilding onto Body
// Magic and made BodyBuilding unlearnable. Keeping names unique avoids that and
// keeps the creation UI display consistent with the spellbook.
// clang-format off
constexpr std::array<const char*, static_cast<size_t>(game::SkillId::Count)> kSkillDisplayNames = {
    "Staff",         // Staff
    "Sword",         // Sword
    "Dagger",        // Dagger
    "Axe",           // Axe
    "Spear",         // Spear
    "Bow",           // Bow
    "Mace",          // Mace
    "Blaster",       // Blaster
    "Shield",        // Shield
    "Leather",       // Leather
    "Chain",         // Chain
    "Plate",         // Plate
    "Fire Magic",    // Fire
    "Air Magic",     // Air
    "Water Magic",   // Water
    "Earth Magic",   // Earth
    "Spirit Magic",  // Spirit
    "Mind Magic",    // Mind
    "Body Magic",    // Body
    "Light Magic",   // Light
    "Dark Magic",    // Dark
    "Identify Item", // ItemId
    "Merchant",      // Merchant
    "Repair",        // Repair
    "Bodybuilding",  // BodyBuilding
    "Meditation",    // Meditation
    "Perception",    // Perception
    "Diplomacy",     // Diplomacy
    "Thievery",      // Thievery
    "Disarm Trap",   // DisarmTrap
    "Dodging",       // Dodging
    "Unarmed",       // Unarmed
    "Monster Lore",  // MonsterLore
    "Armsmaster",    // Armsmaster
    "Stealing",      // Stealing
    "Alchemy",       // Alchemy
    "Learning",      // Learning
};
// clang-format on
static_assert(kSkillDisplayNames.size() == static_cast<size_t>(game::SkillId::Count),
              "kSkillDisplayNames must cover every SkillId");

// Shorthand for indexing the display-name table by SkillId.
constexpr const char* skillDisplayName(game::SkillId id)
{
    return kSkillDisplayNames[static_cast<size_t>(id)];
}

// Pick a starter weapon itemId for a character from the loaded item table.
// Returns the lowest-value weapon whose skillGroup matches the character's first
// starting skill, or 0 if the table is empty / has no matching weapon.
// `firstStartingSkill` is the SkillId of the class's first starting skill
// (kClassStartingSkills[baseClassIndex].skill1).
int pickStarterWeaponItemId(const game::Inventory& inventory, game::SkillId firstStartingSkill)
{
    const std::string_view wanted = skillDisplayName(firstStartingSkill);
    // The item table's skillGroup uses the bare family name ("Sword", "Dagger",
    // "Axe", "Spear", "Staff", "Mace"), so compare against the first token.
    // "Fire Magic" -> "Fire" is never a weapon; weapons are single-word, and
    // the starting-skill first slot is always a weapon family for the 9 base
    // classes, so taking up to the first space is safe here.
    auto space = wanted.find(' ');
    std::string_view familyName =
        space == std::string_view::npos ? wanted : wanted.substr(0, space);

    int bestId = 0;
    int bestValue = INT_MAX;
    for (const auto& entry : inventory.itemTable())
    {
        if (entry.skillGroup != familyName)
        {
            continue;
        }
        // Cheapest matching weapon = basic starter tier; tie-break on lowest id.
        if (entry.value < bestValue || (entry.value == bestValue && bestId == 0))
        {
            bestValue = entry.value;
            bestId = entry.id;
        }
    }
    return bestId;
}

struct ClassSkills
{
    game::SkillId skill1;
    game::SkillId skill2;
};

// Two starting skills per base class (indexed by baseClassIndex 0-8).
constexpr ClassSkills kClassStartingSkills[] = {
    {game::SkillId::Sword, game::SkillId::Leather},   // Knight (base 0)
    {game::SkillId::Dagger, game::SkillId::Stealing}, // Thief (base 1)
    {game::SkillId::Dodging, game::SkillId::Unarmed}, // Monk (base 2)
    {game::SkillId::Mace, game::SkillId::Spirit},     // Paladin (base 3)
    {game::SkillId::Bow, game::SkillId::Air},         // Archer (base 4)
    {game::SkillId::Axe, game::SkillId::Perception},  // Ranger (base 5)
    {game::SkillId::Mace, game::SkillId::Body},       // Cleric (base 6)
    {game::SkillId::Dagger, game::SkillId::Earth},    // Druid (base 7)
    {game::SkillId::Staff, game::SkillId::Fire},      // Sorcerer (base 8)
};

// Available additional skills per base class (SkillId values). These are the
// skills each class CAN learn, minus their 2 starting skills. Expressed as
// SkillId rather than a raw int index so the mapping cannot silently drift
// from the display-name table above.
const std::vector<std::vector<game::SkillId>> kClassAvailableSkills = {
    // Knight(0): Axe, Spear, Bow, Mace, Shield, Chain, Bodybuilding, Perception, Armsmaster
    {game::SkillId::Axe, game::SkillId::Spear, game::SkillId::Bow, game::SkillId::Mace,
     game::SkillId::Shield, game::SkillId::Chain, game::SkillId::BodyBuilding,
     game::SkillId::Perception, game::SkillId::Armsmaster},
    // Thief(1): Sword, Bow, Mace, Leather, Shield, Disarm Trap, Perception, Merchant, Dodging
    {game::SkillId::Sword, game::SkillId::Bow, game::SkillId::Mace, game::SkillId::Leather,
     game::SkillId::Shield, game::SkillId::DisarmTrap, game::SkillId::Perception,
     game::SkillId::Merchant, game::SkillId::Dodging},
    // Monk(2): Staff, Mace, Leather, Bodybuilding, Meditation, Spirit, Mind, Body, Perception,
    // Learning
    {game::SkillId::Staff, game::SkillId::Mace, game::SkillId::Leather, game::SkillId::BodyBuilding,
     game::SkillId::Meditation, game::SkillId::Spirit, game::SkillId::Mind, game::SkillId::Body,
     game::SkillId::Perception, game::SkillId::Learning},
    // Paladin(3): Sword, Shield, Leather, Chain, Plate, Mind, Body, Bodybuilding, Diplomacy, Repair
    {game::SkillId::Sword, game::SkillId::Shield, game::SkillId::Leather, game::SkillId::Chain,
     game::SkillId::Plate, game::SkillId::Mind, game::SkillId::Body, game::SkillId::BodyBuilding,
     game::SkillId::Diplomacy, game::SkillId::Repair},
    // Archer(4): Dagger, Mace, Leather, Chain, Fire, Water, Earth, Perception, Disarm Trap
    {game::SkillId::Dagger, game::SkillId::Mace, game::SkillId::Leather, game::SkillId::Chain,
     game::SkillId::Fire, game::SkillId::Water, game::SkillId::Earth, game::SkillId::Perception,
     game::SkillId::DisarmTrap},
    // Ranger(5): Sword, Mace, Bow, Shield, Leather, Chain, Earth, Water, Alchemy, Dodging
    {game::SkillId::Sword, game::SkillId::Mace, game::SkillId::Bow, game::SkillId::Shield,
     game::SkillId::Leather, game::SkillId::Chain, game::SkillId::Earth, game::SkillId::Water,
     game::SkillId::Alchemy, game::SkillId::Dodging},
    // Cleric(6): Shield, Leather, Chain, Spirit, Mind, Light, Dark, Bodybuilding, Meditation,
    // Diplomacy
    {game::SkillId::Shield, game::SkillId::Leather, game::SkillId::Chain, game::SkillId::Spirit,
     game::SkillId::Mind, game::SkillId::Light, game::SkillId::Dark, game::SkillId::BodyBuilding,
     game::SkillId::Meditation, game::SkillId::Diplomacy},
    // Druid(7): Mace, Staff, Leather, Fire, Air, Water, Meditation, Alchemy, Learning, Dodging
    {game::SkillId::Mace, game::SkillId::Staff, game::SkillId::Leather, game::SkillId::Fire,
     game::SkillId::Air, game::SkillId::Water, game::SkillId::Meditation, game::SkillId::Alchemy,
     game::SkillId::Learning, game::SkillId::Dodging},
    // Sorcerer(8): Dagger, Leather, Air, Water, Earth, Light, Dark, Meditation, Learning, Alchemy
    {game::SkillId::Dagger, game::SkillId::Leather, game::SkillId::Air, game::SkillId::Water,
     game::SkillId::Earth, game::SkillId::Light, game::SkillId::Dark, game::SkillId::Meditation,
     game::SkillId::Learning, game::SkillId::Alchemy},
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
    // Resync each member's base stats to their default face's race group, so
    // the creation screen shows correct stats immediately (not the initDefault
    // placeholder stats that only resolve when a face is cycled across a race).
    if (ctx.shared && ctx.shared->party)
    {
        for (int i = 0; i < game::kPartySize; i++)
        {
            updateCharacterForFace((*ctx.shared->party)[i]);
        }
    }
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
    const auto& classSkills = kClassAvailableSkills[classIdx];
    for (game::SkillId id : classSkills)
    {
        // Check if already selected by this character (the two starting skills
        // occupy slots 0-1; extra selections are in slots 2+).
        const char* displayName = skillDisplayName(id);
        bool sel = false;
        for (size_t s = 2; s < ch.skills.size(); s++)
        {
            if (ch.skills[s] == displayName)
            {
                sel = true;
                break;
            }
        }
        availableSkills.push_back({id, sel});
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
        const int oldGroup = game::faceGroupFromFaceId(ch.faceId);
        ch.faceId = (ch.faceId + delta + kPortraitCount) % kPortraitCount;
        const int newGroup = game::faceGroupFromFaceId(ch.faceId);
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

    // `delta` is a direction (+1 / -1), not an amount: the original moves the
    // attribute by a race-dependent step and charges a race-dependent price.
    const auto adjustStat = [this](Character& ch, int statIdx, int delta)
    {
        const game::AttributeRule& rule =
            game::attributeRule(game::faceGroupFromFaceId(ch.faceId), statIdx);
        int& stat = ch.stats.byIndex(statIdx);

        if (delta > 0)
        {
            const int step = game::attributeIncreaseStep(rule, stat);
            if (calculateBonusPointsRemaining() < game::attributeIncreaseCost(rule, stat))
            {
                return;
            }
            if (stat + step > rule.max)
            {
                return;
            }
            stat += step;
        }
        else if (delta < 0)
        {
            const int step = game::attributeDecreaseStep(rule, stat);
            if (stat - step < rule.base - game::kMaxAttributePointsBelowBase)
            {
                return;
            }
            stat -= step;
        }
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

        for (int i = 0; i < 4; i++)
        {
            if (inRect(gameX, gameY, kColumnX[i], kColumnY, kColumnW, kColumnH))
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
        const int panelX = kColumnX[activeCharacterIndex];
        bool handledPanelClick = false;

        if (inRect(gameX, gameY, panelX, 120, kColumnW, 22))
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

        if (!handledPanelClick && inRect(gameX, gameY, panelX, 35, kColumnW, 82))
        {
            menuRowIndex = 1;
            // Split prev/next on the drawn portrait center, not the panel center:
            // portraits are drawn at portraitX (17/176/335/494, ~9px right of the
            // panel left edge) and may be narrower than the panel, so the panel
            // center biases the split toward "previous".
            constexpr int portraitX[] = {17, 176, 335, 494};
            const int faceId = std::clamp(ch.faceId, 0, kPortraitCount - 1);
            const int pw = portraitWidths[faceId] > 0 ? portraitWidths[faceId] : kColumnW;
            const int portraitCenterX = portraitX[activeCharacterIndex] + pw / 2;
            if (gameX < portraitCenterX - 20)
            {
                changeFace(ch, -1);
            }
            else if (gameX > portraitCenterX + 20)
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
            if (!inRect(gameX, gameY, panelX, statY - 2, kColumnW, statSpacing))
            {
                continue;
            }

            menuRowIndex = 3 + s;
            break;
        }

        // Class selector: original bottom 3x3 class grid.
        {
            for (int i = 0; i < kBaseClassCount; i++)
            {
                const int col = i % 3;
                const int row = i / kClassGridRows;
                const int bx = kClassGridStartX + col * kClassGridColW;
                const int by = kClassGridY + row * kClassGridRowH;
                if (inRect(gameX, gameY, bx, by, kClassGridColW - 2, kClassGridRowH))
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
            if (inRect(gameX, gameY, kBonusMinusX, kBonusButtonY, kBonusButtonW, kBonusButtonH))
            {
                adjustStat(ch, statIdx, -1);
            }
            else if (inRect(gameX, gameY, kBonusPlusX, kBonusButtonY, kBonusButtonW, kBonusButtonH))
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
                        std::erase(ch.skills, std::string(skillDisplayName(sk.id)));
                        syncCreationSkills(ch);
                        if (ctx.playUiSound)
                            ctx.playUiSound("ClickSkill");
                    }
                    else if (extraCount < kMaxExtraSkills)
                    {
                        // Select
                        sk.selected = true;
                        ch.skills.push_back(skillDisplayName(sk.id));
                        syncCreationSkills(ch);
                        if (ctx.playUiSound)
                            ctx.playUiSound("ClickSkill");
                    }
                    break;
                }
            }
        }

        // OK button
        if (inRect(gameX, gameY, kOkButtonX, kBottomButtonY, kBottomButtonW, kBottomButtonH))
        {
            // Finalize each member on the Application-side party vector. The
            // CharacterCreation->Loading transition also calls
            // Application::commitPartyToGameWorld(), which re-runs this same
            // finalize on its own copy; the two are redundant but idempotent,
            // and we keep this one so the data is consistent immediately.
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

                // Starting provisions (RE: Party::initDefault sets gold=200,
                // food=7; bump to a more playable starting amount and ensure
                // each character begins with a basic weapon from their first
                // starting skill).
                party.setGold(std::max(party.gold(), 200));
                party.setFood(std::max(party.food(), 7));
                if (ctx.shared->inventory)
                {
                    // Per RE FUN_00491375: a starting weapon per character from
                    // their first starting skill. Pick the cheapest weapon in the
                    // loaded item table whose skill family matches that skill; if
                    // the table has no match, fall back to itemId 0 (empty slot)
                    // rather than handing every class the same hardcoded weapon.
                    for (int i = 0; i < game::kPartySize; i++)
                    {
                        const game::SkillId firstSkill =
                            kClassStartingSkills[baseClassIndex(party[i].charClass)].skill1;
                        const int starterId =
                            pickStarterWeaponItemId(*ctx.shared->inventory, firstSkill);
                        if (starterId <= 0)
                        {
                            continue;
                        }
                        game::Item starter;
                        starter.itemId = starterId;
                        starter.identified = true;
                        ctx.shared->inventory->addToBackpack(i, starter);
                    }
                }

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
        // CLEAR button: reset the active character's allocation — bonus-point
        // stat changes are reverted to base and any extra skills chosen beyond
        // the two starting ones are dropped. The two starting skills stay (they
        // are determined by class, not by player choice).

        if (inRect(gameX, gameY, kClearButtonX, kBottomButtonY, kBottomButtonW, kBottomButtonH))
        {
            ch.stats = ch.baseStats;
            if (ch.skills.size() > 2)
            {
                ch.skills.resize(2);
                syncCreationSkills(ch);
            }
            rebuildAvailableSkills();
        }
    }

    Character& activeChar = party[activeCharacterIndex];

    // Naming mode: capture text input. Deliberately restricted to uppercase
    // A-Z plus Backspace/Enter/Escape: create.fnt's glyph range does not cover
    // lowercase or digits, so those keys are ignored to avoid writing characters
    // the font cannot render. Spaces and numbers are also dropped for the same
    // reason. Names are capped at 15 characters (MM7's name field width).
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

    // '-' and '+' are registered as hotkeys on the bonus buttons themselves
    // (MM7-Rel.exe 0x497440 / 0x497469), so they adjust the selected stat row
    // regardless of the arrow-key focus.
    if (menuRowIndex >= 3 && menuRowIndex <= 9)
    {
        const int statIdx = menuRowIndex - 3;
        if (ctx.isKeyPressed(SDL_SCANCODE_MINUS) || ctx.isKeyPressed(SDL_SCANCODE_KP_MINUS))
        {
            adjustStat(activeChar, statIdx, -1);
        }
        if (ctx.isKeyPressed(SDL_SCANCODE_EQUALS) || ctx.isKeyPressed(SDL_SCANCODE_KP_PLUS))
        {
            adjustStat(activeChar, statIdx, 1);
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
    // reserved for the large, widely spaced title. The original engine used
    // arrus.fnt for stat/label rows and smallnum.fnt for numbers; RuneHarbor
    // currently uses create.fnt for all body text, so statSpacing below is a
    // fixed literal rather than derived from a loaded font's height.
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

    constexpr int portraitY = 35;    // 0x23 from Ghidra
    constexpr int nameY = 124;       // 0x7C from Ghidra
    constexpr int statsStartY = 169; // 0xA9 from Ghidra
    constexpr int statSpacing =
        17; // Matches original row pitch (arrus.fnt height 19 - 2); see font note above.
    constexpr int skillsHeaderY = 291; // Original labels this region "Skills"
    constexpr int skillsStartY = 311;  // 0x137 — per-char skills below header
    constexpr int skillSpacing = 17;   // same as statSpacing
    // Ghidra: class name overlaid on portrait right side; class icon also on portrait right
    constexpr int nameClassX[] = {18, 177, 336, 495}; // local_13c stride 0x9F
    constexpr int classOverlayY = 100;                // class name on portrait (Ghidra line 98)
    constexpr int classIconY = 50; // 0x32 — class icon on portrait (Ghidra line 99)
    // FACEMASK is the active-character selection ring; it must sit concentric with
    // the oval frame baked into makeme.pcx, i.e. aligned to the portrait position.
    constexpr int faceMaskY = 35;                    // match portraitY
    constexpr int faceMaskX[] = {17, 176, 335, 494}; // match portraitX

    for (int c = 0; c < 4; c++)
    {
        const Character& ch = party[c];
        bool isActive = (c == activeCharacterIndex);
        int panelX = kColumnX[c];
        int panelCenterX = panelX + kColumnW / 2;

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
            std::string_view raceStr = kFaceGroupNames[game::faceGroupFromFaceId(ch.faceId)];
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
                drawOverlay(rightArrow, panelX + kColumnW - rightArrow.w - 2, faceArrowY);
            }
            else if (ctx.debugText)
            {
                ctx.debugText->drawText(sdlRenderer, ctx.scaleX(panelX + kColumnW - 12),
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
            int valX = panelX + kColumnW - valW - 10;
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
                    drawOverlay(leftArrow, panelX + kColumnW - leftArrow.w - 2, statY + 2);
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
        // The "Bonus" label and remaining-points count are centered in the value
        // panel of makeme.pcx — the dark box between the baked -/+ buttons (~x541-613).
        std::string_view bonusLabel = "Bonus";
        drawText(kBonusPanelCenterX - measureGameText(bonusLabel, labelFont) / 2, kBonusLabelY,
                 bonusLabel, labelFont, bonusR, bonusG, bonusB);
        std::string bonusStr = std::to_string(bonusPoints);
        drawText(kBonusPanelCenterX - measureGameText(bonusStr, numFont) / 2, kBonusValueY,
                 bonusStr, numFont, bonusR, bonusG, bonusB);

        if (minusButton.tex)
        {
            drawOverlay(minusButton, kBonusMinusX, kBonusButtonY);
        }
        if (plusButton.tex)
        {
            drawOverlay(plusButton, kBonusPlusX, kBonusButtonY);
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
            drawText(bx + 2, by + 1, skillDisplayName(sk.id), skillBtnFont, sr, sg, sb);
        }
    }

    // 11. Original bottom class grid.
    {
        auto* classGridFont = numFont;
        const Character& activeChar = party[activeCharacterIndex];

        for (int i = 0; i < kBaseClassCount; i++)
        {
            const int displayIdx = kClassGridDisplayIndices[i];
            const int col = i % 3;
            const int row = i / kClassGridRows;
            const int bx = kClassGridStartX + col * kClassGridColW;
            const int by = kClassGridY + row * kClassGridRowH;
            const bool selected = activeChar.charClass == kBaseClasses[displayIdx];
            const uint8_t cr = selected ? 0 : 255;
            const uint8_t cg = selected ? 220 : 255;
            const uint8_t cb = selected ? 220 : 235;
            drawText(bx, by + 1, kBaseClassNames[displayIdx], classGridFont, cr, cg, cb);
        }
    }

    // 12. Bottom controls: OK + Clear (MM7-Rel.exe 0x4973D5 / 0x497411)
    if (okButton.tex)
    {
        drawOverlay(okButton, kOkButtonX, kBottomButtonY);
    }
    else
    {
        drawText(580, 441, "OK", labelFont, 200, 255, 200);
    }

    if (clearButton.tex)
    {
        drawOverlay(clearButton, kClearButtonX, kBottomButtonY);
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
        const int group = game::faceGroupFromFaceId(ch.faceId);
        for (int i = 0; i < 7; i++)
        {
            totalSpent +=
                game::attributePointsSpent(game::attributeRule(group, i), ch.stats.byIndex(i));
        }
    }
    return game::kCreationBonusPoints - totalSpent;
}

void CharacterCreationState::updateCharacterForFace(Character& ch)
{
    int groupIdx = game::faceGroupFromFaceId(ch.faceId);
    for (int i = 0; i < 7; i++)
    {
        ch.baseStats.byIndex(i) = game::attributeRule(groupIdx, i).base;
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
    ch.skills.push_back(skillDisplayName(kClassStartingSkills[classIdx].skill1));
    ch.skills.push_back(skillDisplayName(kClassStartingSkills[classIdx].skill2));
    syncCreationSkills(ch);
}

} // namespace runeharbor::engine
