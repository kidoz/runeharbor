// SPDX-License-Identifier: MIT
#include "character.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <string_view>

#include <cctype>

namespace runeharbor::game
{

namespace
{
std::string normalizeSkillName(std::string_view name)
{
    std::string normalized;
    normalized.reserve(name.size());
    for (char c : name)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return normalized;
}

struct SkillAlias
{
    std::string_view name;
    SkillId id;
};

constexpr SkillAlias kSkillAliases[] = {
    {"staff", SkillId::Staff},
    {"sword", SkillId::Sword},
    {"dagger", SkillId::Dagger},
    {"axe", SkillId::Axe},
    {"spear", SkillId::Spear},
    {"bow", SkillId::Bow},
    {"mace", SkillId::Mace},
    {"blaster", SkillId::Blaster},
    {"shield", SkillId::Shield},
    {"leather", SkillId::Leather},
    {"leatherarmor", SkillId::Leather},
    {"chain", SkillId::Chain},
    {"chainarmor", SkillId::Chain},
    {"plate", SkillId::Plate},
    {"platearmor", SkillId::Plate},
    {"fire", SkillId::Fire},
    {"firemagic", SkillId::Fire},
    {"air", SkillId::Air},
    {"airmagic", SkillId::Air},
    {"water", SkillId::Water},
    {"watermagic", SkillId::Water},
    {"earth", SkillId::Earth},
    {"earthmagic", SkillId::Earth},
    {"spirit", SkillId::Spirit},
    {"spiritmagic", SkillId::Spirit},
    {"mind", SkillId::Mind},
    {"mindmagic", SkillId::Mind},
    {"body", SkillId::Body},
    {"bodymagic", SkillId::Body},
    {"light", SkillId::Light},
    {"lightmagic", SkillId::Light},
    {"dark", SkillId::Dark},
    {"darkmagic", SkillId::Dark},
    {"identify", SkillId::ItemId},
    {"identifyitem", SkillId::ItemId},
    {"itemid", SkillId::ItemId},
    {"merchant", SkillId::Merchant},
    {"repair", SkillId::Repair},
    {"bodybld", SkillId::BodyBuilding},
    {"bodybuilding", SkillId::BodyBuilding},
    {"meditation", SkillId::Meditation},
    {"perception", SkillId::Perception},
    {"diplomacy", SkillId::Diplomacy},
    {"thievery", SkillId::Thievery},
    {"disarm", SkillId::DisarmTrap},
    {"disarmtrap", SkillId::DisarmTrap},
    {"dodging", SkillId::Dodging},
    {"unarmed", SkillId::Unarmed},
    {"monlore", SkillId::MonsterLore},
    {"monsterlore", SkillId::MonsterLore},
    {"armsmaster", SkillId::Armsmaster},
    {"stealing", SkillId::Stealing},
    {"alchemy", SkillId::Alchemy},
    {"learning", SkillId::Learning},
};
} // namespace

int Character::effectiveStat(int statIndex) const
{
    return baseStats.byIndex(statIndex) + timedStatBonus(statIndex);
}

void Character::addTimedEffect(TimedEffect effect)
{
    for (auto& existing : timedEffects)
    {
        if (existing.spellId != 0 && existing.spellId == effect.spellId)
        {
            existing = std::move(effect);
            recalculateDerived();
            return;
        }
    }
    timedEffects.push_back(std::move(effect));
    recalculateDerived();
}

bool Character::hasTimedEffect(int spellId) const
{
    return std::ranges::any_of(timedEffects, [spellId](const TimedEffect& effect)
                               { return effect.spellId == spellId; });
}

void Character::clearExpiredEffects(int64_t nowTicks)
{
    const size_t before = timedEffects.size();
    std::erase_if(timedEffects, [nowTicks](const TimedEffect& effect)
                  { return effect.expiresAtTicks > 0 && nowTicks >= effect.expiresAtTicks; });
    if (timedEffects.size() != before)
    {
        recalculateDerived();
    }
}

void Character::clearAllTimedEffects()
{
    if (!timedEffects.empty())
    {
        timedEffects.clear();
        recalculateDerived();
    }
}

int Character::timedStatBonus(int statIndex) const
{
    int total = 0;
    for (const auto& effect : timedEffects)
    {
        total += effect.statBonus[static_cast<size_t>(statIndex)];
    }
    return total;
}

int Character::timedArmorClassBonus() const
{
    int total = 0;
    for (const auto& effect : timedEffects)
    {
        total += effect.armorClassBonus;
    }
    return total;
}

int Character::timedResistanceBonus(int index) const
{
    if (index < 0 || index >= 6)
    {
        return 0;
    }
    int total = 0;
    for (const auto& effect : timedEffects)
    {
        total += effect.resistanceBonus[static_cast<size_t>(index)];
    }
    return total;
}

void Character::takeDamage(int amount)
{
    setHitPoints(hitPoints - amount);
}

void Character::heal(int amount)
{
    setHitPoints(hitPoints + amount);
}

void Character::setHitPoints(int value)
{
    // Clamp to [0, maxHitPoints] (maxHitPoints may be 0 pre-recalculate; treat
    // the upper bound as at least the current HP so a heal never reduces HP).
    const int upper = std::max(0, std::max(maxHitPoints, hitPoints));
    hitPoints = std::clamp(value, 0, upper);

    // Reaching 0 HP downs the character unless a worse condition is already
    // active. This matches the combat path's behaviour and closes the gap where
    // the event engine's direct HP write left a 0-HP character "conscious".
    if (hitPoints <= 0)
    {
        const ConditionIndex worst = worstActiveCondition();
        const bool alreadyWorse = worst == ConditionIndex::Dead ||
                                  worst == ConditionIndex::Stoned ||
                                  worst == ConditionIndex::Eradicated;
        if (!alreadyWorse)
        {
            setCondition(ConditionIndex::Unconscious);
        }
    }
}

void Character::recalculateDerived()
{
    // Update current stats from base
    for (int i = 0; i < Stats::kCount; i++)
    {
        stats.byIndex(i) = effectiveStat(i);
    }

    // HP: base from endurance + level + class bonus
    int endBonus = std::max(0, (stats.endurance - 10) / 2);
    int classHpBase = 0;
    switch (baseClassIndex(charClass))
    {
    case 0: // Knight
        classHpBase = 12;
        break;
    case 3: // Paladin
        classHpBase = 10;
        break;
    case 4: // Archer
        classHpBase = 8;
        break;
    case 6: // Cleric
        classHpBase = 8;
        break;
    case 8: // Sorcerer
        classHpBase = 6;
        break;
    case 1: // Thief
        classHpBase = 8;
        break;
    case 2: // Monk
        classHpBase = 10;
        break;
    case 5: // Ranger
        classHpBase = 8;
        break;
    case 7: // Druid
        classHpBase = 6;
        break;
    default:
        classHpBase = 8;
        break;
    }
    maxHitPoints = classHpBase + (classHpBase + endBonus) * (level - 1);
    hitPoints = std::min(hitPoints, maxHitPoints);

    // SP: base from intellect/personality + level + class bonus
    int intBonus = std::max(0, (stats.intellect - 10) / 2);
    int perBonus = std::max(0, (stats.personality - 10) / 2);
    int classSpBase = 0;
    switch (baseClassIndex(charClass))
    {
    case 0: // Knight
        classSpBase = 0;
        break;
    case 3: // Paladin
        classSpBase = 4;
        break;
    case 4: // Archer
        classSpBase = 4;
        break;
    case 6: // Cleric
        classSpBase = 6;
        break;
    case 8: // Sorcerer
        classSpBase = 8;
        break;
    case 1: // Thief
        classSpBase = 0;
        break;
    case 2: // Monk
        classSpBase = 0;
        break;
    case 5: // Ranger
        classSpBase = 4;
        break;
    case 7: // Druid
        classSpBase = 6;
        break;
    default:
        classSpBase = 0;
        break;
    }
    int spBonus = std::max(intBonus, perBonus); // Use whichever is higher
    maxSpellPoints = classSpBase + (classSpBase > 0 ? (classSpBase + spBonus) * (level - 1) : 0);
    spellPoints = std::min(spellPoints, maxSpellPoints);

    // AC: base from speed, plus timed buff bonuses (Shield, Stone Skin, ...).
    armorClass = std::max(0, (stats.speed - 10) / 2) + timedArmorClassBonus();
    armorClass = std::max(0, armorClass);
}

int Character::xpRequiredForNextLevel() const
{
    // Triangle formula: level * (level + 1) / 2 * 1000
    // But since level is already 1-based, xpNeeded for level N is: N * (N - 1) / 2 * 1000
    // To reach level+1, we need level * (level + 1) / 2 * 1000?
    // Let's use the formula from docs: xpNeeded = N*(N-1)/2 * 1000
    int targetLevel = level + 1;
    return (targetLevel * (targetLevel - 1) / 2) * 1000;
}

bool Character::canLevelUp() const
{
    return experience >= xpRequiredForNextLevel();
}

void Character::addExperience(int amount)
{
    if (amount > 0 && isConscious())
    {
        experience += amount;
    }
}

void Character::levelUp()
{
    if (!canLevelUp())
    {
        return;
    }

    level++;

    // FUN_004B4673 (0x4B4B4A): training grants (newLevel / 10) + 5 skill
    // points and heals HP/SP to the new maximum.
    skillPoints += (level / 10) + 5;

    recalculateDerived();
    hitPoints = maxHitPoints;
    spellPoints = maxSpellPoints;
}

int Character::skillUpgradeCost(SkillId skill) const
{
    const size_t index = static_cast<size_t>(skill);
    if (index >= skillLevels.size() || !skillLevels[index].learned())
    {
        return 0;
    }
    return skillLevels[index].level + 1;
}

bool Character::raiseSkill(SkillId skill)
{
    const size_t index = static_cast<size_t>(skill);
    if (index >= skillLevels.size())
    {
        return false;
    }

    SkillValue& value = skillLevels[index];
    if (!value.learned() || value.level >= kMaxSkillLevel)
    {
        return false;
    }

    const int cost = value.level + 1;
    if (skillPoints < cost)
    {
        return false;
    }

    skillPoints -= cost;
    value.level++;
    return true;
}

void Character::rest(int hours)
{
    if (!isAlive())
    {
        return;
    }

    // Simple regeneration
    if (hours >= 8)
    {
        hitPoints = maxHitPoints;
        spellPoints = maxSpellPoints;

        // Cure weak
        clearCondition(ConditionIndex::Weak);
    }
    else
    {
        // Partial regeneration
        float regenFraction = static_cast<float>(hours) / 8.0f;
        hitPoints =
            std::min(maxHitPoints, hitPoints + static_cast<int>(maxHitPoints * regenFraction));
        spellPoints = std::min(maxSpellPoints,
                               spellPoints + static_cast<int>(maxSpellPoints * regenFraction));
    }
}

ConditionIndex Character::worstActiveCondition() const
{
    // MM7 priority order (worst-first), from the table at 0x4EDDA0. See
    // docs/temple-healing-resurrection.md section 5.
    static constexpr ConditionIndex kPriorityOrder[] = {
        ConditionIndex::Eradicated, ConditionIndex::Stoned,      ConditionIndex::Dead,
        ConditionIndex::Zombie,     ConditionIndex::Unconscious, ConditionIndex::Asleep,
        ConditionIndex::Paralyzed,  ConditionIndex::Disease3,    ConditionIndex::Poison3,
        ConditionIndex::Disease2,   ConditionIndex::Poison2,     ConditionIndex::Disease1,
        ConditionIndex::Poison1,    ConditionIndex::Insane,      ConditionIndex::Drunk,
        ConditionIndex::Afraid,     ConditionIndex::Weak,        ConditionIndex::Cursed};

    for (ConditionIndex c : kPriorityOrder)
    {
        if (hasCondition(c))
        {
            return c;
        }
    }
    return ConditionIndex::Count;
}

void Character::clearAllConditions()
{
    conditionTimestamps.fill(0);
}

bool Character::learnSpell(int spellId, SkillId schoolSkill, int requiredSpellLevel)
{
    if (spellId <= 0 || spellId >= kSpellCount)
    {
        return false;
    }
    // FUN_004680F1 book branch: the character must know the magic school and
    // meet the spell's required mastery. pSpellDatas[].skillMastery groups the
    // 11 spells of each school into 1-4 Normal, 5-7 Expert, 8-10 Master and 11
    // Grandmaster, matching spellCountForMastery.
    const auto& skill = skillLevels[static_cast<size_t>(schoolSkill)];
    if (!skill.learned())
    {
        return false;
    }
    const int spellLevel = requiredSpellLevel > 0 ? requiredSpellLevel : ((spellId - 1) % 11 + 1);
    if (spellLevel > spellsLearnableAtMastery(skill.mastery))
    {
        return false;
    }
    if (knownSpells[static_cast<size_t>(spellId)])
    {
        return false; // already known
    }
    knownSpells[static_cast<size_t>(spellId)] = true;
    return true;
}

std::optional<SkillId> skillIdFromName(std::string_view name)
{
    const std::string normalized = normalizeSkillName(name);
    for (const SkillAlias& alias : kSkillAliases)
    {
        if (normalized == alias.name)
        {
            return alias.id;
        }
    }
    return std::nullopt;
}

void learnSkill(Character& character, SkillId skillId, int level, SkillMastery mastery)
{
    auto& skill = character.skillLevels[static_cast<size_t>(skillId)];
    skill.level = static_cast<uint8_t>(std::clamp(level, 1, Character::kMaxSkillLevel));
    skill.mastery = mastery == SkillMastery::None ? SkillMastery::Normal : mastery;
}

void forgetSkill(Character& character, SkillId skillId)
{
    character.skillLevels[static_cast<size_t>(skillId)] = {};
}

void syncSkillLevelsFromDisplaySkills(Character& character)
{
    std::fill(character.skillLevels.begin(), character.skillLevels.end(), SkillValue{});
    for (const std::string& name : character.skills)
    {
        if (const auto skillId = skillIdFromName(name); skillId.has_value())
        {
            learnSkill(character, *skillId);
        }
    }
}

namespace
{
// [36 classes][37 skills] maximum mastery, transcribed from the original's
// skillMaxMasteryPerClass table. Values are the
// SkillMastery enum: 0 = cannot learn, 1 = Normal, 2 = Expert, 3 = Master,
// 4 = Grandmaster. Rows follow CharacterClass order; columns follow SkillId.
// clang-format off
constexpr uint8_t kClassMaxMastery[static_cast<size_t>(CharacterClass::Count)]
                                  [static_cast<size_t>(SkillId::Count)] = {
    {2, 3, 2, 3, 3, 2, 3, 4, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 3, 0, 2, 1, 0, 1, 2, 2, 0, 3, 0, 0, 1}, // Knight
    {2, 3, 2, 3, 3, 2, 3, 4, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 3, 0, 2, 1, 0, 1, 2, 2, 0, 3, 0, 0, 1}, // Cavalier
    {2, 4, 2, 3, 4, 2, 3, 4, 4, 3, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 4, 0, 2, 1, 0, 1, 2, 2, 0, 4, 0, 0, 1}, // Champion
    {2, 4, 2, 3, 4, 2, 3, 4, 4, 3, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 4, 0, 2, 1, 0, 1, 2, 2, 0, 4, 0, 0, 1}, // Black Knight
    {0, 3, 3, 0, 0, 2, 2, 4, 1, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 1, 2, 0, 3, 0, 0, 3, 3, 2, 0, 3, 3, 2, 2}, // Thief
    {0, 3, 3, 0, 0, 2, 2, 4, 1, 3, 2, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 3, 3, 1, 2, 0, 3, 0, 0, 3, 3, 2, 0, 3, 3, 2, 2}, // Rogue
    {0, 3, 4, 0, 0, 2, 2, 4, 1, 4, 2, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 3, 3, 1, 2, 0, 3, 0, 0, 4, 3, 2, 0, 3, 4, 2, 2}, // Spy
    {0, 3, 4, 0, 0, 2, 2, 4, 1, 4, 2, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 3, 3, 1, 2, 0, 3, 0, 0, 4, 3, 2, 0, 3, 4, 2, 2}, // Assassin
    {3, 2, 2, 0, 2, 1, 0, 4, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 2, 1, 0, 1, 3, 3, 2, 3, 1, 0, 3}, // Monk
    {3, 2, 2, 0, 2, 1, 0, 4, 0, 3, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 3, 0, 2, 1, 0, 2, 3, 3, 2, 3, 1, 0, 3}, // Initiate
    {4, 2, 2, 0, 2, 1, 0, 4, 0, 3, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 4, 0, 2, 1, 0, 2, 4, 4, 2, 3, 1, 0, 4}, // Master
    {4, 2, 2, 0, 2, 1, 0, 4, 0, 3, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 4, 0, 2, 1, 0, 3, 4, 4, 2, 3, 2, 0, 4}, // Ninja
    {1, 3, 2, 2, 2, 2, 3, 4, 3, 2, 2, 3, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 2, 3, 3, 2, 1, 3, 0, 0, 1, 1, 0, 2, 0, 0, 1}, // Paladin
    {1, 3, 2, 2, 2, 2, 3, 4, 3, 2, 2, 3, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 2, 3, 3, 2, 1, 3, 0, 0, 1, 1, 0, 2, 0, 0, 1}, // Crusader
    {1, 3, 2, 2, 2, 2, 4, 4, 4, 2, 2, 3, 0, 0, 0, 0, 3, 3, 3, 1, 0, 0, 2, 4, 3, 2, 1, 4, 0, 0, 1, 1, 0, 2, 0, 0, 1}, // Hero
    {1, 3, 2, 2, 2, 2, 4, 4, 4, 2, 2, 3, 0, 0, 0, 0, 3, 3, 3, 0, 1, 0, 2, 4, 3, 2, 1, 4, 0, 0, 1, 1, 0, 2, 0, 0, 1}, // Villain
    {1, 2, 2, 2, 3, 3, 0, 4, 0, 3, 3, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 0, 0, 1, 2, 1, 0, 2, 0, 0, 3}, // Archer
    {1, 2, 2, 2, 3, 3, 0, 4, 0, 3, 3, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 0, 0, 1, 2, 1, 0, 2, 0, 0, 3}, // Warrior Mage
    {1, 2, 2, 2, 3, 4, 0, 4, 0, 3, 4, 0, 3, 3, 3, 3, 0, 0, 0, 1, 0, 0, 2, 2, 2, 2, 4, 0, 0, 2, 2, 1, 0, 2, 0, 0, 3}, // Master Archer
    {1, 2, 2, 2, 3, 4, 0, 4, 0, 3, 4, 0, 3, 3, 3, 3, 0, 0, 0, 0, 1, 0, 2, 2, 2, 2, 4, 0, 0, 2, 2, 1, 0, 2, 0, 0, 3}, // Sniper
    {1, 2, 2, 3, 2, 3, 0, 4, 2, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 1, 3, 2, 0, 2, 2, 1, 3, 2, 2, 1, 2}, // Ranger
    {1, 2, 2, 3, 2, 3, 0, 4, 2, 3, 3, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 2, 1, 3, 2, 0, 2, 2, 1, 3, 2, 2, 1, 2}, // Hunter
    {1, 2, 2, 4, 2, 3, 0, 4, 2, 3, 3, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 1, 1, 1, 2, 1, 3, 2, 0, 2, 2, 1, 4, 2, 2, 1, 2}, // Ranger Lord
    {1, 2, 2, 4, 2, 3, 0, 4, 2, 3, 3, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 1, 1, 1, 2, 1, 3, 2, 0, 2, 2, 1, 4, 2, 2, 1, 2}, // Bounty Hunter
    {1, 0, 0, 0, 0, 2, 3, 4, 3, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 3, 3, 1, 3, 2, 3, 0, 0, 0, 0, 2, 0, 0, 2, 3}, // Cleric
    {1, 0, 0, 0, 0, 2, 3, 4, 3, 2, 2, 0, 0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 3, 3, 1, 3, 2, 3, 0, 0, 0, 0, 2, 0, 0, 2, 3}, // Priest
    {1, 0, 0, 0, 0, 2, 3, 4, 3, 2, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 0, 0, 4, 3, 1, 3, 2, 3, 0, 0, 0, 0, 2, 0, 0, 2, 3}, // Priest of Sun
    {1, 0, 0, 0, 0, 2, 3, 4, 3, 2, 2, 0, 0, 0, 0, 0, 4, 4, 4, 0, 4, 0, 4, 3, 1, 3, 2, 3, 0, 0, 0, 0, 2, 0, 0, 2, 3}, // Priest of Moon
    {1, 0, 3, 0, 0, 1, 2, 4, 2, 2, 0, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 0, 3, 2, 0, 0, 0, 0, 0, 2, 1, 0, 3, 3}, // Druid
    {1, 0, 3, 0, 0, 1, 2, 4, 2, 2, 0, 0, 3, 3, 3, 3, 3, 3, 3, 0, 0, 2, 2, 0, 0, 3, 2, 0, 0, 0, 0, 0, 2, 1, 0, 3, 3}, // Great Druid
    {1, 0, 3, 0, 0, 1, 2, 4, 2, 2, 0, 0, 3, 3, 3, 3, 3, 3, 3, 0, 0, 2, 2, 0, 0, 4, 2, 0, 0, 0, 0, 0, 2, 1, 0, 4, 3}, // Arch Druid
    {1, 0, 3, 0, 0, 1, 2, 4, 2, 2, 0, 0, 3, 3, 3, 3, 3, 3, 3, 0, 0, 2, 2, 0, 0, 4, 2, 0, 0, 0, 0, 0, 2, 1, 0, 4, 3}, // Warlock
    {3, 0, 2, 0, 0, 1, 0, 4, 0, 2, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 3, 1, 2, 0, 3, 2, 1, 0, 0, 0, 0, 3, 0, 0, 3, 3}, // Sorcerer
    {3, 0, 2, 0, 0, 1, 0, 4, 0, 2, 0, 0, 3, 3, 3, 3, 0, 0, 0, 0, 0, 3, 1, 2, 0, 3, 2, 1, 0, 0, 0, 0, 3, 0, 0, 3, 3}, // Wizard
    {3, 0, 2, 0, 0, 1, 0, 4, 0, 2, 0, 0, 4, 4, 4, 4, 0, 0, 0, 4, 0, 4, 1, 2, 0, 3, 2, 1, 0, 0, 0, 0, 4, 0, 0, 3, 3}, // Archmage
    {3, 0, 2, 0, 0, 1, 0, 4, 0, 2, 0, 0, 4, 4, 4, 4, 0, 0, 0, 0, 4, 4, 1, 2, 0, 3, 2, 1, 0, 0, 0, 0, 4, 0, 0, 3, 3}, // Lich
};
// clang-format on

// Gold prices for the three trainable tiers, from the original's
// expert/master/grandmaster cost tables. A zero means no trainer exists.
constexpr int kExpertCost[static_cast<size_t>(SkillId::Count)] = {
    2000, 2000, 2000, 2000, 2000, 2000, 2000, 0,    1000, 1000, 1000, 1000, 1000,
    1000, 1000, 1000, 1000, 1000, 1000, 2000, 2000, 500,  2000, 500,  500,  500,
    500,  0,    0,    500,  2000, 2000, 500,  2000, 500,  500,  2000,
};
constexpr int kMasterCost[static_cast<size_t>(SkillId::Count)] = {
    5000, 5000, 5000, 5000, 5000, 5000, 5000, 0,    3000, 3000, 3000, 3000, 4000,
    4000, 4000, 4000, 4000, 4000, 4000, 5000, 5000, 2500, 5000, 2500, 2500, 2500,
    2500, 0,    0,    2500, 5000, 5000, 2500, 5000, 2500, 2500, 5000,
};
constexpr int kGrandmasterCost[static_cast<size_t>(SkillId::Count)] = {
    8000, 8000, 8000, 8000, 8000, 8000, 8000, 0,    7000, 7000, 7000, 7000, 8000,
    8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 6000, 8000, 6000, 6000, 6000,
    6000, 0,    0,    6000, 8000, 8000, 6000, 8000, 6000, 6000, 8000,
};
} // namespace

SkillMastery maxMasteryForClass(CharacterClass charClass, SkillId skill)
{
    const size_t classIndex = static_cast<size_t>(charClass);
    const size_t skillIndex = static_cast<size_t>(skill);
    if (classIndex >= std::size(kClassMaxMastery) || skillIndex >= std::size(kClassMaxMastery[0]))
    {
        return SkillMastery::None;
    }
    return static_cast<SkillMastery>(kClassMaxMastery[classIndex][skillIndex]);
}

std::string_view masteryName(SkillMastery mastery)
{
    switch (mastery)
    {
    case SkillMastery::Normal:
        return "Normal";
    case SkillMastery::Expert:
        return "Expert";
    case SkillMastery::Master:
        return "Master";
    case SkillMastery::GrandMaster:
        return "Grandmaster";
    case SkillMastery::None:
        return "None";
    }
    return "None";
}

int masteryTrainingCost(SkillId skill, SkillMastery target)
{
    const size_t index = static_cast<size_t>(skill);
    if (index >= static_cast<size_t>(SkillId::Count))
    {
        return 0;
    }

    switch (target)
    {
    case SkillMastery::Expert:
        return kExpertCost[index];
    case SkillMastery::Master:
        return kMasterCost[index];
    case SkillMastery::GrandMaster:
        return kGrandmasterCost[index];
    case SkillMastery::None:
    case SkillMastery::Normal:
        return 0;
    }
    return 0;
}

bool meetsMasteryRequirements(const Character& character, SkillId skill, SkillMastery target)
{
    const size_t index = static_cast<size_t>(skill);
    if (index >= character.skillLevels.size())
    {
        return false;
    }

    const SkillValue& value = character.skillLevels[index];
    if (!value.learned() || static_cast<int>(target) <= static_cast<int>(value.mastery))
    {
        return false;
    }
    if (static_cast<int>(maxMasteryForClass(character.charClass, skill)) < static_cast<int>(target))
    {
        return false;
    }

    switch (target)
    {
    case SkillMastery::Expert:
        // Expert training requires skill level 4.
        return value.level >= 4;
    case SkillMastery::Master:
    {
        // Master training requires level 7 and Expert mastery. Light and Dark
        // retain the quest-bit gate in the original; the class cap already
        // restricts them to the tier-3 path classes here.
        if (value.level < 7 || value.mastery != SkillMastery::Expert)
        {
            return false;
        }
        switch (skill)
        {
        case SkillId::Merchant:
            return character.baseStats.personality >= 50;
        case SkillId::BodyBuilding:
            return character.baseStats.endurance >= 50;
        case SkillId::Learning:
            return character.baseStats.intellect >= 50;
        default:
            return true;
        }
    }
    case SkillMastery::GrandMaster:
    {
        // Grandmaster training requires level 10 and Master mastery.
        if (value.level < 10 || value.mastery != SkillMastery::Master)
        {
            return false;
        }
        switch (skill)
        {
        case SkillId::Light:
            return character.charClass == CharacterClass::Archmage ||
                   character.charClass == CharacterClass::PriestOfSun;
        case SkillId::Dark:
            return character.charClass == CharacterClass::Lich ||
                   character.charClass == CharacterClass::PriestOfMoon;
        case SkillId::Dodging:
            return character.skillLevels[static_cast<size_t>(SkillId::Unarmed)].level >= 10;
        case SkillId::Unarmed:
            return character.skillLevels[static_cast<size_t>(SkillId::Dodging)].level >= 10;
        default:
            return true;
        }
    }
    case SkillMastery::None:
    case SkillMastery::Normal:
        return false;
    }
    return false;
}

std::optional<SkillMastery> nextMastery(const Character& character, SkillId skill)
{
    const size_t index = static_cast<size_t>(skill);
    if (index >= character.skillLevels.size() || !character.skillLevels[index].learned())
    {
        return std::nullopt;
    }

    const int current = static_cast<int>(character.skillLevels[index].mastery);
    const int cap = static_cast<int>(maxMasteryForClass(character.charClass, skill));
    if (current >= cap || current >= static_cast<int>(SkillMastery::GrandMaster))
    {
        return std::nullopt;
    }

    return static_cast<SkillMastery>(current + 1);
}

int spellsLearnableAtMastery(SkillMastery mastery)
{
    switch (mastery)
    {
    case SkillMastery::Normal:
        return 4;
    case SkillMastery::Expert:
        return 7;
    case SkillMastery::Master:
        return 10;
    case SkillMastery::GrandMaster:
        return 11;
    case SkillMastery::None:
        return 0;
    }
    return 0;
}

bool isPromotion(CharacterClass from, CharacterClass to)
{
    const int fromValue = static_cast<int>(from);
    const int toValue = static_cast<int>(to);
    if (fromValue < 0 || fromValue >= static_cast<int>(CharacterClass::Count) || toValue < 0 ||
        toValue >= static_cast<int>(CharacterClass::Count))
    {
        return false;
    }

    const int base = fromValue & ~3;
    const int tier = fromValue & 3;
    if (tier == 0)
    {
        return toValue == base + 1;
    }
    if (tier == 1)
    {
        return toValue == base + 2 || toValue == base + 3;
    }
    return false;
}

bool promoteCharacter(Character& character, CharacterClass newClass)
{
    if (!isPromotion(character.charClass, newClass))
    {
        return false;
    }
    character.charClass = newClass;
    character.recalculateDerived();
    return true;
}

namespace
{
// [face group][attribute] = {base, max, lowRate, highRate}, from MM7-Rel.exe
// 0x4ED658. Rows are ordered by face range, so rows 2 and 3 correspond to the
// original's race ids 3 and 2 respectively (see faceGroupFromFaceId).
constexpr AttributeRule kAttributeRules[4][Stats::kCount] = {
    // Faces 0-7 (Human) — original race 0
    {{11, 25, 1, 1},
     {11, 25, 1, 1},
     {11, 25, 1, 1},
     {9, 25, 1, 1},
     {11, 25, 1, 1},
     {11, 25, 1, 1},
     {9, 25, 1, 1}},
    // Faces 8-11 (Elf) — original race 1
    {{7, 15, 2, 1},
     {14, 30, 1, 2},
     {11, 25, 1, 1},
     {7, 15, 2, 1},
     {14, 30, 1, 2},
     {11, 25, 1, 1},
     {9, 20, 1, 1}},
    // Faces 12-15 (Dwarf) — original race 3
    {{14, 30, 1, 2},
     {11, 25, 1, 1},
     {11, 25, 1, 1},
     {14, 30, 1, 2},
     {7, 15, 2, 1},
     {7, 15, 2, 1},
     {9, 20, 1, 1}},
    // Faces 16-19 (Goblin) — original race 2
    {{14, 30, 1, 2},
     {7, 15, 2, 1},
     {7, 15, 2, 1},
     {11, 25, 1, 1},
     {11, 25, 1, 1},
     {14, 30, 1, 2},
     {9, 20, 1, 1}},
};
} // namespace

int faceGroupFromFaceId(int faceId)
{
    if (faceId < 8)
    {
        return 0;
    }
    if (faceId < 12)
    {
        return 1;
    }
    if (faceId < 16)
    {
        return 2;
    }
    return 3;
}

const AttributeRule& attributeRule(int faceGroup, int statIndex)
{
    const int group = std::clamp(faceGroup, 0, 3);
    const int stat = std::clamp(statIndex, 0, Stats::kCount - 1);
    return kAttributeRules[group][stat];
}

int attributePointsSpent(const AttributeRule& rule, int value)
{
    // Mirrors fcn.0049090b: (base - value) * a / b with a/b swapping at the
    // base, and C-style truncating division matching the original's idiv.
    const int numerator = (value >= rule.base) ? rule.lowRate : rule.highRate;
    const int denominator = (value >= rule.base) ? rule.highRate : rule.lowRate;
    return -((rule.base - value) * numerator / denominator);
}

int attributeIncreaseStep(const AttributeRule& rule, int value)
{
    return (value >= rule.base) ? rule.highRate : rule.lowRate;
}

int attributeDecreaseStep(const AttributeRule& rule, int value)
{
    return (value > rule.base) ? rule.highRate : rule.lowRate;
}

int attributeIncreaseCost(const AttributeRule& rule, int value)
{
    return (value >= rule.base) ? rule.lowRate : rule.highRate;
}

namespace
{
// [base class][skill] entries of 2 from MM7-Rel.exe 0x4ED6C8.
constexpr std::array<SkillId, kClassStartingSkillCount> kClassStartingSkills[kBaseClassCount] = {
    {SkillId::Sword, SkillId::Leather},   // Knight
    {SkillId::Dagger, SkillId::Stealing}, // Thief
    {SkillId::Dodging, SkillId::Unarmed}, // Monk
    {SkillId::Mace, SkillId::Spirit},     // Paladin
    {SkillId::Bow, SkillId::Air},         // Archer
    {SkillId::Axe, SkillId::Perception},  // Ranger
    {SkillId::Mace, SkillId::Body},       // Cleric
    {SkillId::Dagger, SkillId::Earth},    // Druid
    {SkillId::Staff, SkillId::Fire},      // Sorcerer
};

// [base class][skill] entries of 1 from the same table, in skill-index order.
constexpr std::array<SkillId, kClassAvailableSkillCount> kClassAvailableSkills[kBaseClassCount] = {
    // Knight
    {SkillId::Axe, SkillId::Spear, SkillId::Bow, SkillId::Mace, SkillId::Shield, SkillId::Chain,
     SkillId::BodyBuilding, SkillId::Perception, SkillId::Armsmaster},
    // Thief
    {SkillId::Sword, SkillId::Bow, SkillId::Leather, SkillId::ItemId, SkillId::Merchant,
     SkillId::Perception, SkillId::DisarmTrap, SkillId::Dodging, SkillId::Alchemy},
    // Monk
    {SkillId::Staff, SkillId::Sword, SkillId::Dagger, SkillId::Spear, SkillId::Leather,
     SkillId::BodyBuilding, SkillId::Perception, SkillId::MonsterLore, SkillId::Armsmaster},
    // Paladin
    {SkillId::Sword, SkillId::Dagger, SkillId::Axe, SkillId::Shield, SkillId::Leather,
     SkillId::Merchant, SkillId::Repair, SkillId::BodyBuilding, SkillId::Armsmaster},
    // Archer
    {SkillId::Sword, SkillId::Axe, SkillId::Spear, SkillId::Leather, SkillId::Fire, SkillId::Water,
     SkillId::Perception, SkillId::Armsmaster, SkillId::Learning},
    // Ranger
    {SkillId::Sword, SkillId::Dagger, SkillId::Bow, SkillId::Leather, SkillId::BodyBuilding,
     SkillId::DisarmTrap, SkillId::Dodging, SkillId::MonsterLore, SkillId::Armsmaster},
    // Cleric
    {SkillId::Shield, SkillId::Leather, SkillId::Spirit, SkillId::Mind, SkillId::Merchant,
     SkillId::Repair, SkillId::Meditation, SkillId::Alchemy, SkillId::Learning},
    // Druid
    {SkillId::Mace, SkillId::Leather, SkillId::Water, SkillId::Spirit, SkillId::Body,
     SkillId::Meditation, SkillId::Perception, SkillId::Alchemy, SkillId::Learning},
    // Sorcerer
    {SkillId::Dagger, SkillId::Leather, SkillId::Air, SkillId::Water, SkillId::Earth,
     SkillId::ItemId, SkillId::Merchant, SkillId::MonsterLore, SkillId::Alchemy},
};

// clang-format off
constexpr std::array<std::string_view, static_cast<size_t>(SkillId::Count)> kSkillDisplayNames = {
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
} // namespace

const std::array<SkillId, kClassStartingSkillCount>& classStartingSkills(int baseClassIndex)
{
    return kClassStartingSkills[std::clamp(baseClassIndex, 0, kBaseClassCount - 1)];
}

const std::array<SkillId, kClassAvailableSkillCount>& classAvailableSkills(int baseClassIndex)
{
    return kClassAvailableSkills[std::clamp(baseClassIndex, 0, kBaseClassCount - 1)];
}

std::string_view skillDisplayName(SkillId id)
{
    const auto index = static_cast<size_t>(id);
    if (index >= kSkillDisplayNames.size())
    {
        return {};
    }
    return kSkillDisplayNames[index];
}

} // namespace runeharbor::game
