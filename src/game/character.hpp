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

    // Experience and Leveling
    int xpRequiredForNextLevel() const;
    bool canLevelUp() const;
    void addExperience(int amount);
    void levelUp();

    // Resting
    void rest(int hours);
};

std::optional<SkillId> skillIdFromName(std::string_view name);
void learnSkill(Character& character, SkillId skillId, int level = 1,
                SkillMastery mastery = SkillMastery::Normal);
void forgetSkill(Character& character, SkillId skillId);
void syncSkillLevelsFromDisplaySkills(Character& character);

} // namespace runeharbor::game
