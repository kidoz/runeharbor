// SPDX-License-Identifier: MIT
#include "character.hpp"

#include <algorithm>

namespace runeharbor::game
{

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
    // The recalculateDerived() already handles the max HP/SP, so we just fill the new diff or restore.
    // We'll fully restore HP/SP on level up as a simplification, or just leave it for rest?
    // Let's just restore them for now, or just add the delta.
    // Actually, levelUp in MM7 requires training center. We'll just increment here.
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
        hitPoints = std::min(maxHitPoints, hitPoints + static_cast<int>(maxHitPoints * regenFraction));
        spellPoints = std::min(maxSpellPoints, spellPoints + static_cast<int>(maxSpellPoints * regenFraction));
    }
}

} // namespace runeharbor::game
