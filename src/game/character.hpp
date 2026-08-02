// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace runeharbor::game
{

enum class Gender : uint8_t
{
    Male = 0,
    Female,
    Count
};

// MM7 character classes: 36-ID scheme (4 per base class: base, tier1, tier2a, tier2b)
enum class CharacterClass : uint8_t
{
    Knight = 0,
    Cavalier = 1,
    Champion = 2,
    BlackKnight = 3,
    Thief = 4,
    Rogue = 5,
    Spy = 6,
    Assassin = 7,
    Monk = 8,
    Initiate = 9,
    Master = 10,
    Ninja = 11,
    Paladin = 12,
    Crusader = 13,
    Hero = 14,
    Villain = 15,
    Archer = 16,
    WarriorMage = 17,
    MasterArcher = 18,
    Sniper = 19,
    Ranger = 20,
    Hunter = 21,
    RangerLord = 22,
    BountyHunter = 23,
    Cleric = 24,
    Priest = 25,
    HighPriest = 26,
    PriestOfDark = 27,
    Druid = 28,
    GreatDruid = 29,
    ArchDruid = 30,
    Warlock = 31,
    Sorcerer = 32,
    Wizard = 33,
    Archmage = 34,
    Lich = 35,
    Count = 36
};

// Returns the base class index (0-8) from any promotion tier
inline int baseClassIndex(CharacterClass c)
{
    return static_cast<int>(c) >> 2;
}

// MM7 skills
enum class SkillId : uint8_t
{
    Staff = 0,
    Sword,
    Dagger,
    Axe,
    Spear,
    Bow,
    Mace,
    Blaster,
    Shield,
    Leather,
    Chain,
    Plate,
    Fire,
    Air,
    Water,
    Earth,
    Spirit,
    Mind,
    Body,
    Light,
    Dark,
    ItemId,
    Merchant,
    Repair,
    BodyBuilding,
    Meditation,
    Perception,
    Diplomacy,
    Thievery,
    DisarmTrap,
    Dodging,
    Unarmed,
    MonsterLore,
    Armsmaster,
    Stealing,
    Alchemy,
    Learning,
    Count
};

// Skill mastery levels
enum class SkillMastery : uint8_t
{
    None = 0,
    Normal = 1,
    Expert = 2,
    Master = 3,
    GrandMaster = 4
};

// Packed skill value: mastery in high bits, level in low bits
struct SkillValue
{
    uint8_t level = 0;
    SkillMastery mastery = SkillMastery::None;

    bool learned() const { return mastery != SkillMastery::None; }
    int effective() const { return learned() ? level : 0; }
};

// Character condition indices (MM7 stores 18 int64 timestamps, not a bitfield)
enum class ConditionIndex : uint8_t
{
    Cursed = 0,
    Weak = 1,
    Asleep = 2,
    Afraid = 3,
    Drunk = 4,
    Insane = 5,
    Poison1 = 6,
    Disease1 = 7,
    Poison2 = 8,
    Disease2 = 9,
    Poison3 = 10,
    Disease3 = 11,
    Paralyzed = 12,
    Unconscious = 13,
    Dead = 14,
    Stoned = 15,
    Eradicated = 16,
    Zombie = 17,
    Count = 18
};

// Equipment slots (matches MM7 binary layout: Rings 9-14, Amulet 15)
enum class EquipSlot : uint8_t
{
    MainHand = 0,
    OffHand,
    Bow,
    Armor,
    Helmet,
    Belt,
    Cloak,
    Gauntlets,
    Boots,
    Ring1,  // 9
    Ring2,  // 10
    Ring3,  // 11
    Ring4,  // 12
    Ring5,  // 13
    Ring6,  // 14
    Amulet, // 15
    Count
};

// Lightweight item reference (full item system is separate)
struct ItemSlot
{
    int itemId = 0;
    bool occupied() const { return itemId != 0; }
};

// The 7 primary stats
struct Stats
{
    int might = 11;
    int intellect = 11;
    int personality = 11;
    int endurance = 11;
    int speed = 11;
    int accuracy = 11;
    int luck = 11;

    static constexpr int kCount = 7;

    // MM7 stat index order: 0=Might, 1=Intellect, 2=Personality, 3=Endurance,
    // 4=Accuracy, 5=Speed, 6=Luck
    int& byIndex(int i)
    {
        switch (i)
        {
        case 1:
            return intellect;
        case 2:
            return personality;
        case 3:
            return endurance;
        case 4:
            return accuracy;
        case 5:
            return speed;
        case 6:
            return luck;
        default:
            return might;
        }
    }

    int byIndex(int i) const
    {
        switch (i)
        {
        case 0:
            return might;
        case 1:
            return intellect;
        case 2:
            return personality;
        case 3:
            return endurance;
        case 4:
            return accuracy;
        case 5:
            return speed;
        case 6:
            return luck;
        default:
            return 0;
        }
    }
};

struct Character
{
    std::string name = "New Hero";
    int faceId = 0;
    CharacterClass charClass = CharacterClass::Knight;
    Gender gender = Gender::Male;

    // Primary attributes
    Stats stats;
    Stats baseStats;

    // Derived attributes
    int level = 1;
    int experience = 0;
    int hitPoints = 0;
    int maxHitPoints = 0;
    int spellPoints = 0;
    int maxSpellPoints = 0;
    int armorClass = 0;
    int age = 20;

    // Base resistances (matches MM7 character struct: Fire, Air, Water, Earth, Mind, Body)
    // Spirit, Light, Dark resistances come from equipment/buffs only (no base value)
    int fireResistance = 0;
    int airResistance = 0;
    int waterResistance = 0;
    int earthResistance = 0;
    int mindResistance = 0;
    int bodyResistance = 0;

    // Skills
    std::array<SkillValue, static_cast<size_t>(SkillId::Count)> skillLevels = {};
    std::vector<std::string> skills; // Display names (for character creation UI)

    // Known spells (spellbook). MM7 stores one known-spell byte per spell at
    // charBase+0x192; RuneHarbor models it as a bool array indexed by spell id
    // (1..99). Books grant a spell here (FUN_004680F1 book branch).
    static constexpr int kSpellCount = 100; // spell ids 1..99 (1-based)
    std::array<bool, kSpellCount> knownSpells = {};
    bool knowsSpell(int spellId) const
    {
        return spellId > 0 && spellId < kSpellCount && knownSpells[static_cast<size_t>(spellId)];
    }
    // Learns a spell. Returns false if the character lacks the school skill at
    // Normal mastery (the gate from the book branch), true on success.
    bool learnSpell(int spellId, SkillId schoolSkill);

    // Spell quickbar: 2 assignable spell slots (RE: per-character bytes at
    // +0x1A4E / +0x1A4F). 0 = empty.
    static constexpr int kQuickbarSlots = 2;
    std::array<int, kQuickbarSlots> quickbarSpells = {0, 0};
    void setQuickbarSpell(int slot, int spellId)
    {
        if (slot >= 0 && slot < kQuickbarSlots)
            quickbarSpells[static_cast<size_t>(slot)] = spellId;
    }

    // Equipment
    std::array<ItemSlot, static_cast<size_t>(EquipSlot::Count)> equipment = {};

    // Conditions: MM7 stores 18 int64 timestamps (0 = not active, nonzero = game tick when set)
    static constexpr int kConditionCount = static_cast<int>(ConditionIndex::Count);
    std::array<int64_t, kConditionCount> conditionTimestamps = {};

    bool hasCondition(ConditionIndex c) const
    {
        return conditionTimestamps[static_cast<size_t>(c)] != 0;
    }

    void setCondition(ConditionIndex c, int64_t gameTime = 1)
    {
        conditionTimestamps[static_cast<size_t>(c)] = gameTime;
    }

    void clearCondition(ConditionIndex c) { conditionTimestamps[static_cast<size_t>(c)] = 0; }

    // Returns the most severe active condition per the MM7 priority order
    // (Eradicated > Stoned > Dead > Zombie > Unconscious > Asleep > Paralyzed >
    // diseases/poisons descending > Insane > Drunk > Afraid > Weak > Cursed),
    // or ConditionIndex::Count if none are active. Mirrors the engine's
    // GetWorstCondition (FUN_0048E9EC, priority table at 0x4EDDA0).
    ConditionIndex worstActiveCondition() const;

    // Clears every active condition (used by temple heal/resurrect).
    void clearAllConditions();

    bool isAlive() const
    {
        return !hasCondition(ConditionIndex::Dead) && !hasCondition(ConditionIndex::Stoned) &&
               !hasCondition(ConditionIndex::Eradicated);
    }

    bool isConscious() const { return isAlive() && !hasCondition(ConditionIndex::Unconscious); }

    // Calculate effective stat (base stats only — no racial system in MM7)
    int effectiveStat(int statIndex) const;

    // Recalculate derived stats (HP, SP, AC, resistances) from base stats + equipment
    void recalculateDerived();

    // Centralized HP mutation. takeDamage subtracts (clamped at 0) and sets the
    // Unconscious condition when HP reaches 0 — unless a worse condition (Dead,
    // Stoned, Eradicated) is already active. heal adds (clamped at maxHitPoints)
    // but does NOT auto-clear conditions (call clearCondition explicitly, e.g.
    // via temple/rest). Use these instead of writing hitPoints directly so the
    // death/condition side-effects stay consistent across combat and events.
    void takeDamage(int amount);
    void heal(int amount);
    // Set HP to an absolute value with the same clamping + condition handling
    // as takeDamage/heal. Used by EVT SetPlayerVar (param1 == 7).
    void setHitPoints(int value);

    // Experience and Leveling
    int xpRequiredForNextLevel() const;
    bool canLevelUp() const;
    void addExperience(int amount);
    void levelUp();

    // Resting
    void rest(int hours);
};

// Character-creation attribute rules, transcribed from the original's table at
// MM7-Rel.exe 0x4ED658 (four bytes per race/attribute pair).
//
// `lowRate`/`highRate` encode the asymmetric buy/sell rates the original
// applies (fcn.004905ed for "+", fcn.00490485 for "-", fcn.0049090b for the
// running point total). Above the racial base an attribute moves in steps of
// `highRate` and is charged `lowRate` points; at or below the base the two
// swap. Humans use 1/1 everywhere, so every point buys exactly one. A race's
// favoured attributes (rate pair 1/2) gain +2 per point spent, and its weak
// ones (2/1) cost 2 points per +1.
struct AttributeRule
{
    int base = 11;
    int max = 25;
    int lowRate = 1;  // original byte [2]
    int highRate = 1; // original byte [3]
};

// Bonus points shared by the whole party, not per character (fcn.0049090b
// seeds its running total with 50 and then walks all four members).
inline constexpr int kCreationBonusPoints = 50;

// How far below the racial base an attribute may be pushed (fcn.00490485:
// `lea esi, [ecx - 2]`).
inline constexpr int kMaxAttributePointsBelowBase = 2;

// Portrait index -> race group. The original derives a race id in fcn.00490101
// (faces 0-7 -> 0, 8-11 -> 1, 12-15 -> 3, 16-19 -> 2); the group returned here
// is ordered by face range instead, so 2 is the faces-12-15 group.
int faceGroupFromFaceId(int faceId);

/// Attribute rule for a face group (0-3) and MM7 stat index (0-6).
const AttributeRule& attributeRule(int faceGroup, int statIndex);

/// Points consumed by holding `value` in an attribute governed by `rule`.
/// Negative values mean the attribute is below its base and refunds points.
int attributePointsSpent(const AttributeRule& rule, int value);

/// Amount one press of "+" / "-" moves the attribute by.
int attributeIncreaseStep(const AttributeRule& rule, int value);
int attributeDecreaseStep(const AttributeRule& rule, int value);

/// Points charged for one press of "+".
int attributeIncreaseCost(const AttributeRule& rule, int value);

// Class skill data, transcribed from the original's [9 base classes][37 skills]
// table at MM7-Rel.exe 0x4ED6C8. Player::SetClass (fcn.00490242) clears all 37
// skill slots and then learns every skill whose entry is 2; entries of 1 are the
// further skills the class is allowed to pick up, and 0 means never.
inline constexpr int kBaseClassCount = 9;
inline constexpr size_t kClassStartingSkillCount = 2;
inline constexpr size_t kClassAvailableSkillCount = 9;

/// The two skills a base class (0-8, ordered Knight, Thief, Monk, Paladin,
/// Archer, Ranger, Cleric, Druid, Sorcerer) begins with.
const std::array<SkillId, kClassStartingSkillCount>& classStartingSkills(int baseClassIndex);

/// The nine further skills a base class may choose during creation. The count
/// is exactly nine for every class, which is what fills the 3x3 selection grid.
const std::array<SkillId, kClassAvailableSkillCount>& classAvailableSkills(int baseClassIndex);

/// Canonical display name for a skill. Magic schools use the full
/// "<School> Magic" form and BodyBuilding its full name so that every entry is
/// unique and round-trips through skillIdFromName.
std::string_view skillDisplayName(SkillId id);

std::optional<SkillId> skillIdFromName(std::string_view name);
void learnSkill(Character& character, SkillId skillId, int level = 1,
                SkillMastery mastery = SkillMastery::Normal);
void forgetSkill(Character& character, SkillId skillId);
void syncSkillLevelsFromDisplaySkills(Character& character);

} // namespace runeharbor::game
