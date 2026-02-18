// SPDX-License-Identifier: MIT
#include "spells.hpp"

#include <algorithm>
#include <random>

#include <cmath>

#include "combat.hpp"
#include "game_world.hpp"

namespace runeharbor::game
{

namespace
{
int randomInt(int min, int max)
{
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}
} // namespace

SpellSystem::SpellSystem(util::ILogger& logger) : logger_(logger) {}

void SpellSystem::loadSpellData(const std::vector<formats::SpellEntry>& spells)
{
    spells_.clear();
    spellIndex_.clear();

    for (const auto& entry : spells)
    {
        SpellInfo info;
        info.spellId = entry.id;
        info.name = entry.name;
        info.shortName = entry.shortName;
        info.level = entry.level;
        info.school = determineSchool(entry.id);
        info.normalEffect = entry.normalEffect;
        info.expertEffect = entry.expertEffect;
        info.masterEffect = entry.masterEffect;
        info.grandMasterEffect = entry.grandMasterEffect;
        info.resistance = entry.resistance;

        // Base mana costs scale with spell level
        info.manaCostNormal = entry.level * 3;
        info.manaCostExpert = entry.level * 2;
        info.manaCostMaster = static_cast<int>(std::ceil(entry.level * 1.5));
        info.manaCostGM = entry.level;

        // Determine target type from spell school and level
        // Simple heuristic: damage spells target enemies, healing/buff target allies
        if (info.school == SpellSchool::Spirit || info.school == SpellSchool::Body)
        {
            info.target = SpellTarget::SingleAlly;
        }
        else
        {
            info.target = SpellTarget::SingleEnemy;
        }

        spellIndex_[info.spellId] = spells_.size();
        spells_.push_back(std::move(info));
    }

    logger_.info("Loaded " + std::to_string(spells_.size()) + " spell definitions");
}

const SpellInfo* SpellSystem::getSpell(int spellId) const
{
    auto it = spellIndex_.find(spellId);
    if (it == spellIndex_.end())
        return nullptr;
    return &spells_[it->second];
}

bool SpellSystem::canCast(int characterIndex, int spellId) const
{
    if (!gameWorld_ || characterIndex < 0 || characterIndex >= kPartySize)
        return false;

    const auto* spell = getSpell(spellId);
    if (!spell)
        return false;

    const auto& ch = gameWorld_->party().member(characterIndex);
    if (!ch.isConscious())
        return false;

    // Check if character has the required school skill
    int skillLevel = getEffectiveSkillLevel(characterIndex, spell->school);
    if (skillLevel < spell->level)
        return false;

    // Check mana
    int cost = getManaCost(characterIndex, spellId);
    return ch.spellPoints >= cost;
}

int SpellSystem::getManaCost(int characterIndex, int spellId) const
{
    const auto* spell = getSpell(spellId);
    if (!spell)
        return 999;

    SkillMastery mastery = getEffectiveMastery(characterIndex, spell->school);
    switch (mastery)
    {
    case SkillMastery::GrandMaster:
        return spell->manaCostGM;
    case SkillMastery::Master:
        return spell->manaCostMaster;
    case SkillMastery::Expert:
        return spell->manaCostExpert;
    default:
        return spell->manaCostNormal;
    }
}

SkillMastery SpellSystem::getEffectiveMastery(int characterIndex, SpellSchool school) const
{
    if (!gameWorld_ || characterIndex < 0 || characterIndex >= kPartySize)
        return SkillMastery::None;

    const auto& ch = gameWorld_->party().member(characterIndex);
    SkillId skill = schoolToSkill(school);
    return ch.skillLevels[static_cast<size_t>(skill)].mastery;
}

int SpellSystem::getEffectiveSkillLevel(int characterIndex, SpellSchool school) const
{
    if (!gameWorld_ || characterIndex < 0 || characterIndex >= kPartySize)
        return 0;

    const auto& ch = gameWorld_->party().member(characterIndex);
    SkillId skill = schoolToSkill(school);
    return ch.skillLevels[static_cast<size_t>(skill)].effective();
}

SpellResult SpellSystem::castDamageSpell(int characterIndex, int spellId, MonsterInstance* target)
{
    SpellResult result;

    if (!canCast(characterIndex, spellId))
    {
        result.description = "Cannot cast spell";
        if (callbacks_.onSpellFailed)
            callbacks_.onSpellFailed(spellId, result.description);
        return result;
    }

    const auto* spell = getSpell(spellId);
    auto& ch = gameWorld_->party().member(characterIndex);

    // Deduct mana
    int cost = getManaCost(characterIndex, spellId);
    ch.spellPoints -= cost;

    if (!target || !target->isAlive())
    {
        result.description = spell->name + " fizzles (no target)";
        return result;
    }

    SkillMastery mastery = getEffectiveMastery(characterIndex, spell->school);
    int damage = calculateSpellDamage(
        spellId, getEffectiveSkillLevel(characterIndex, spell->school), mastery);

    // Check resistance
    // Simple: each element has a corresponding resistance on the monster
    DamageType dmgType = static_cast<DamageType>(static_cast<int>(spell->school) + 1);
    // Resistance roll: target has X% chance to resist
    int resistChance = 0;
    switch (dmgType)
    {
    case DamageType::Fire:
        resistChance = target->resistFire;
        break;
    case DamageType::Air:
        resistChance = target->resistAir;
        break;
    case DamageType::Water:
        resistChance = target->resistWater;
        break;
    case DamageType::Earth:
        resistChance = target->resistEarth;
        break;
    case DamageType::Mind:
        resistChance = target->resistMind;
        break;
    case DamageType::Spirit:
        resistChance = target->resistSpirit;
        break;
    case DamageType::Body:
        resistChance = target->resistBody;
        break;
    default:
        break;
    }

    if (randomInt(1, 100) <= resistChance)
    {
        result.resisted = true;
        damage = damage / 2;
    }

    result.damage = std::max(1, damage);
    target->currentHP -= result.damage;
    result.success = true;

    if (target->currentHP <= 0)
    {
        target->currentHP = 0;
        target->aiState = MonsterInstance::AIState::Dead;
        result.description = ch.name + " casts " + spell->name + " killing " + target->name + " (" +
                             std::to_string(result.damage) + " damage)";
    }
    else
    {
        result.description = ch.name + " casts " + spell->name + " on " + target->name + " for " +
                             std::to_string(result.damage) +
                             (result.resisted ? " (partially resisted)" : "");
    }

    if (callbacks_.onSpellCast)
        callbacks_.onSpellCast(spellId, characterIndex, result);

    return result;
}

SpellResult SpellSystem::castHealSpell(int characterIndex, int spellId, int targetCharIndex)
{
    SpellResult result;

    if (!canCast(characterIndex, spellId))
    {
        result.description = "Cannot cast spell";
        return result;
    }

    const auto* spell = getSpell(spellId);
    auto& caster = gameWorld_->party().member(characterIndex);

    int cost = getManaCost(characterIndex, spellId);
    caster.spellPoints -= cost;

    if (targetCharIndex < 0 || targetCharIndex >= kPartySize)
    {
        result.description = spell->name + " fizzles (no target)";
        return result;
    }

    auto& target = gameWorld_->party().member(targetCharIndex);
    SkillMastery mastery = getEffectiveMastery(characterIndex, spell->school);
    int healing =
        calculateHealing(spellId, getEffectiveSkillLevel(characterIndex, spell->school), mastery);

    target.hitPoints = std::min(target.hitPoints + healing, target.maxHitPoints);
    result.success = true;
    result.healing = healing;
    result.description = caster.name + " casts " + spell->name + " on " + target.name + " (+" +
                         std::to_string(healing) + " HP)";

    if (callbacks_.onSpellCast)
        callbacks_.onSpellCast(spellId, characterIndex, result);

    return result;
}

SpellResult SpellSystem::castBuffSpell(int characterIndex, int spellId)
{
    SpellResult result;

    if (!canCast(characterIndex, spellId))
    {
        result.description = "Cannot cast spell";
        return result;
    }

    const auto* spell = getSpell(spellId);
    auto& caster = gameWorld_->party().member(characterIndex);

    int cost = getManaCost(characterIndex, spellId);
    caster.spellPoints -= cost;

    result.success = true;
    result.description = caster.name + " casts " + spell->name;

    if (callbacks_.onSpellCast)
        callbacks_.onSpellCast(spellId, characterIndex, result);

    return result;
}

std::vector<int> SpellSystem::getAvailableSpells(int characterIndex) const
{
    std::vector<int> available;
    if (!gameWorld_ || characterIndex < 0 || characterIndex >= kPartySize)
        return available;

    for (const auto& spell : spells_)
    {
        int skillLevel = getEffectiveSkillLevel(characterIndex, spell.school);
        if (skillLevel >= spell.level)
        {
            available.push_back(spell.spellId);
        }
    }
    return available;
}

int SpellSystem::calculateSpellDamage(int spellId, int casterLevel, SkillMastery mastery) const
{
    (void)spellId;
    // Base damage scales with caster level and mastery
    int base = casterLevel * 2 + 1;

    int multiplier = 1;
    switch (mastery)
    {
    case SkillMastery::GrandMaster:
        multiplier = 4;
        break;
    case SkillMastery::Master:
        multiplier = 3;
        break;
    case SkillMastery::Expert:
        multiplier = 2;
        break;
    default:
        multiplier = 1;
        break;
    }

    int damage = base * multiplier;
    // Add some randomness
    damage += randomInt(0, casterLevel);
    return std::max(1, damage);
}

int SpellSystem::calculateHealing(int spellId, int casterLevel, SkillMastery mastery) const
{
    (void)spellId;
    int base = casterLevel * 3 + 5;

    switch (mastery)
    {
    case SkillMastery::GrandMaster:
        base *= 3;
        break;
    case SkillMastery::Master:
        base *= 2;
        break;
    case SkillMastery::Expert:
        base = base * 3 / 2;
        break;
    default:
        break;
    }

    return std::max(1, base);
}

SpellSchool SpellSystem::determineSchool(int spellId) const
{
    // MM7 spell IDs: 1-11 = Fire, 12-22 = Air, 23-33 = Water, etc.
    // Each school has 11 spells
    if (spellId <= 0)
        return SpellSchool::Fire;
    int schoolIndex = (spellId - 1) / 11;
    if (schoolIndex >= static_cast<int>(SpellSchool::Count))
        return SpellSchool::Fire;
    return static_cast<SpellSchool>(schoolIndex);
}

} // namespace runeharbor::game
