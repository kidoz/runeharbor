// SPDX-License-Identifier: MIT
#include "character.hpp"

#include <algorithm>
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
    return baseStats.byIndex(statIndex);
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

    // AC: base from speed
    armorClass = std::max(0, (stats.speed - 10) / 2);
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
    recalculateDerived();
    // In MM7 you gain HP/SP equal to class base + endurance/intellect bonus per level.
    // The recalculateDerived() already handles the max HP/SP, so we just fill the new diff or
    // restore. We'll fully restore HP/SP on level up as a simplification, or just leave it for
    // rest? Let's just restore them for now, or just add the delta. Actually, levelUp in MM7
    // requires training center. We'll just increment here.
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
    // docs/re/30-temple-healing-resurrection.md section 5.
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

bool Character::learnSpell(int spellId, SkillId schoolSkill)
{
    if (spellId <= 0 || spellId >= kSpellCount)
    {
        return false;
    }
    // FUN_004680F1 book branch: the character must know the magic school
    // (mastery >= Normal) to learn the spell.
    const auto& skill = skillLevels[static_cast<size_t>(schoolSkill)];
    if (!skill.learned())
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
    skill.level = static_cast<uint8_t>(std::clamp(level, 1, 63));
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

} // namespace runeharbor::game
