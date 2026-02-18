// SPDX-License-Identifier: MIT
#include "combat.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <random>

#include "game_world.hpp"

namespace runeharbor::game
{

namespace
{
// Thread-local RNG for combat rolls
std::mt19937& rng()
{
    static thread_local std::mt19937 gen(std::random_device{}());
    return gen;
}

int randomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng());
}
} // namespace

CombatSystem::CombatSystem(util::ILogger& logger) : logger_(logger) {}

void CombatSystem::loadMonsterData(const std::vector<formats::MonsterEntry>& monsters)
{
    monsterDefs_.clear();
    for (const auto& m : monsters)
    {
        monsterDefs_[m.id] = m;
    }
    logger_.info("Loaded " + std::to_string(monsters.size()) + " monster definitions");
}

int CombatSystem::spawnMonster(int monsterId, float x, float y, float z)
{
    auto it = monsterDefs_.find(monsterId);
    if (it == monsterDefs_.end())
    {
        logger_.warning("Unknown monster ID: " + std::to_string(monsterId));
        return -1;
    }

    const auto& def = it->second;
    MonsterInstance inst;
    inst.monsterId = monsterId;
    inst.name = def.name;
    inst.currentHP = def.hitPoints;
    inst.maxHP = def.hitPoints;
    inst.armorClass = def.armorClass;
    inst.level = def.level;
    inst.experience = def.experience;
    inst.speed = def.speed;
    inst.recoveryTime = 0;
    inst.x = x;
    inst.y = y;
    inst.z = z;
    inst.aiState = MonsterInstance::AIState::Standing;

    inst.resistFire = def.resistFire;
    inst.resistAir = def.resistAir;
    inst.resistWater = def.resistWater;
    inst.resistEarth = def.resistEarth;
    inst.resistMind = def.resistMind;
    inst.resistSpirit = def.resistSpirit;
    inst.resistBody = def.resistBody;
    inst.resistLight = def.resistLight;
    inst.resistDark = def.resistDark;
    inst.resistPhysical = def.resistPhysical;

    int index = static_cast<int>(monsters_.size());
    monsters_.push_back(std::move(inst));
    return index;
}

void CombatSystem::clearMonsters()
{
    monsters_.clear();
    inCombat_ = false;
}

MonsterInstance* CombatSystem::getMonster(int instanceIndex)
{
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(monsters_.size()))
        return nullptr;
    return &monsters_[static_cast<size_t>(instanceIndex)];
}

void CombatSystem::update(float deltaMs)
{
    if (!inCombat_ || !gameWorld_)
        return;

    for (auto& monster : monsters_)
    {
        if (!monster.isAlive())
            continue;

        // Reduce recovery time
        if (monster.recoveryTime > 0)
        {
            monster.recoveryTime -= static_cast<int>(deltaMs);
            if (monster.recoveryTime < 0)
                monster.recoveryTime = 0;
            continue;
        }

        updateMonsterAI(monster, deltaMs);
    }

    // Check if combat is over
    if (aliveMonsterCount() == 0)
    {
        inCombat_ = false;
    }
}

AttackResult CombatSystem::playerAttack(int characterIndex, int monsterIndex)
{
    AttackResult result;

    if (!gameWorld_)
        return result;

    auto& party = gameWorld_->party();
    if (characterIndex < 0 || characterIndex >= kPartySize)
        return result;

    auto& ch = party.member(characterIndex);
    if (!ch.isConscious())
    {
        result.description = ch.name + " is incapacitated";
        return result;
    }

    auto* monster = getMonster(monsterIndex);
    if (!monster || !monster->isAlive())
    {
        result.description = "No valid target";
        return result;
    }

    // Calculate hit chance based on accuracy vs AC
    int accuracy = ch.stats.accuracy;
    int chance = hitChance(ch.level, accuracy, monster->armorClass);

    if (!rollHit(chance))
    {
        result.hit = false;
        result.description = ch.name + " misses " + monster->name;
        return result;
    }

    result.hit = true;

    // Base damage from might + weapon
    int mightBonus = std::max(0, (ch.stats.might - 10) / 2);
    int baseDamage = 1 + mightBonus + ch.level;

    // Critical hit (5% chance, double damage)
    if (randomInt(1, 20) == 20)
    {
        result.critical = true;
        baseDamage *= 2;
    }

    result.damage = calculateDamage(baseDamage, DamageElement::Physical, *monster);
    monster->currentHP -= result.damage;

    if (monster->currentHP <= 0)
    {
        monster->currentHP = 0;
        monster->aiState = MonsterInstance::AIState::Dead;
        result.description =
            ch.name + " kills " + monster->name + " (" + std::to_string(result.damage) + " damage)";

        distributeXP(monster->experience);

        if (callbacks_.onMonsterKilled)
        {
            callbacks_.onMonsterKilled(*monster, monster->experience);
        }
    }
    else
    {
        result.description = ch.name + " hits " + monster->name + " for " +
                             std::to_string(result.damage) +
                             (result.critical ? " (critical!)" : "");
    }

    if (callbacks_.onCharacterAttack)
    {
        callbacks_.onCharacterAttack(characterIndex, *monster, result);
    }

    return result;
}

// Parse dice expressions like "2d8+10", "3d6", "4"
int CombatSystem::rollDamage(const std::string& diceExpr)
{
    if (diceExpr.empty())
        return 0;

    // Look for 'D' or 'd' separator
    size_t dPos = diceExpr.find_first_of("dD");
    if (dPos == std::string::npos)
    {
        // Plain number
        int val = 0;
        auto [p, ec] = std::from_chars(diceExpr.data(), diceExpr.data() + diceExpr.size(), val);
        (void)p;
        return ec == std::errc() ? val : 0;
    }

    // Parse NdM+B format
    int numDice = 1;
    if (dPos > 0)
    {
        std::from_chars(diceExpr.data(), diceExpr.data() + dPos, numDice);
    }

    int dieSides = 6;
    size_t plusPos = diceExpr.find_first_of("+-", dPos + 1);
    if (plusPos != std::string::npos)
    {
        std::from_chars(diceExpr.data() + dPos + 1, diceExpr.data() + plusPos, dieSides);
    }
    else
    {
        std::from_chars(diceExpr.data() + dPos + 1, diceExpr.data() + diceExpr.size(), dieSides);
    }

    int bonus = 0;
    if (plusPos != std::string::npos)
    {
        std::from_chars(diceExpr.data() + plusPos, diceExpr.data() + diceExpr.size(), bonus);
    }

    int total = bonus;
    for (int i = 0; i < numDice; i++)
    {
        total += randomInt(1, std::max(1, dieSides));
    }
    return std::max(0, total);
}

int CombatSystem::calculateDamage(int baseDamage, DamageElement type,
                                  const MonsterInstance& target) const
{
    int resistance = 0;
    switch (type)
    {
    case DamageElement::Physical:
        resistance = target.resistPhysical;
        break;
    case DamageElement::Fire:
        resistance = target.resistFire;
        break;
    case DamageElement::Air:
        resistance = target.resistAir;
        break;
    case DamageElement::Water:
        resistance = target.resistWater;
        break;
    case DamageElement::Earth:
        resistance = target.resistEarth;
        break;
    case DamageElement::Mind:
        resistance = target.resistMind;
        break;
    case DamageElement::Spirit:
        resistance = target.resistSpirit;
        break;
    case DamageElement::Body:
        resistance = target.resistBody;
        break;
    case DamageElement::Light:
        resistance = target.resistLight;
        break;
    case DamageElement::Dark:
        resistance = target.resistDark;
        break;
    default:
        break;
    }

    // Resistance reduces damage: damage * (100 - resist) / 100
    int reduced = baseDamage * std::max(0, 100 - resistance) / 100;
    return std::max(1, reduced); // Always at least 1 damage on a hit
}

int CombatSystem::calculateMonsterDamage(int baseDamage, DamageElement type, int characterIndex) const
{
    if (!gameWorld_ || characterIndex < 0 || characterIndex >= kPartySize)
        return baseDamage;

    const auto& ch = gameWorld_->party().member(characterIndex);
    int resistance = 0;
    switch (type)
    {
    case DamageElement::Fire:
        resistance = ch.fireResistance;
        break;
    case DamageElement::Air:
        resistance = ch.airResistance;
        break;
    case DamageElement::Water:
        resistance = ch.waterResistance;
        break;
    case DamageElement::Earth:
        resistance = ch.earthResistance;
        break;
    case DamageElement::Mind:
        resistance = ch.mindResistance;
        break;
    case DamageElement::Body:
        resistance = ch.bodyResistance;
        break;
    case DamageElement::Physical:
        resistance = ch.armorClass;
        break;
    default:
        break;
    }

    int reduced = baseDamage * std::max(0, 100 - resistance) / 100;
    return std::max(1, reduced);
}

int CombatSystem::hitChance(int attackerLevel, int attackerAccuracy, int defenderAC) const
{
    // Base hit chance: 50% + (accuracy - AC) * 2 + level * 2
    int chance = 50 + (attackerAccuracy - defenderAC) * 2 + attackerLevel * 2;
    return std::clamp(chance, 5, 95); // Always 5-95% chance
}

bool CombatSystem::rollHit(int chance) const
{
    return randomInt(1, 100) <= chance;
}

int CombatSystem::aliveMonsterCount() const
{
    int count = 0;
    for (const auto& m : monsters_)
    {
        if (m.isAlive())
            count++;
    }
    return count;
}

void CombatSystem::updateMonsterAI(MonsterInstance& monster, float /*deltaMs*/)
{
    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();

    switch (monster.aiState)
    {
    case MonsterInstance::AIState::Standing:
    {
        // Check distance to party
        float dx = monster.x - party.worldX();
        float dy = monster.y - party.worldY();
        float distSq = dx * dx + dy * dy;
        if (distSq < monster.aggroRange * monster.aggroRange)
        {
            monster.aiState = MonsterInstance::AIState::Pursuing;
            // Pick random conscious target
            std::vector<int> targets;
            for (int i = 0; i < kPartySize; i++)
            {
                if (party.member(i).isConscious())
                    targets.push_back(i);
            }
            if (!targets.empty())
            {
                monster.targetCharacter = targets[static_cast<size_t>(
                    randomInt(0, static_cast<int>(targets.size()) - 1))];
            }
        }
        break;
    }
    case MonsterInstance::AIState::Pursuing:
        // For now, immediately attack when in pursuit
        monster.aiState = MonsterInstance::AIState::Attacking;
        break;

    case MonsterInstance::AIState::Attacking:
        monsterAttack(monster);
        break;

    case MonsterInstance::AIState::Fleeing:
    case MonsterInstance::AIState::Dead:
        break;

    default:
        break;
    }
}

void CombatSystem::monsterAttack(MonsterInstance& monster)
{
    if (!gameWorld_ || monster.targetCharacter < 0)
        return;

    auto& party = gameWorld_->party();
    auto& target = party.member(monster.targetCharacter);

    if (!target.isConscious())
    {
        // Retarget
        monster.targetCharacter = -1;
        for (int i = 0; i < kPartySize; i++)
        {
            if (party.member(i).isConscious())
            {
                monster.targetCharacter = i;
                break;
            }
        }
        if (monster.targetCharacter < 0)
            return;
    }

    // Look up monster attack data
    auto it = monsterDefs_.find(monster.monsterId);
    std::string damageExpr = "1d4";
    if (it != monsterDefs_.end())
    {
        damageExpr = it->second.attack1.damage;
        if (damageExpr.empty())
            damageExpr = "1d4";
    }

    AttackResult result;
    int chance = hitChance(monster.level, monster.level * 3, target.armorClass);

    if (!rollHit(chance))
    {
        result.hit = false;
        result.description = monster.name + " misses " + target.name;
    }
    else
    {
        result.hit = true;
        int baseDmg = rollDamage(damageExpr);
        result.damage =
            calculateMonsterDamage(baseDmg, DamageElement::Physical, monster.targetCharacter);
        target.hitPoints -= result.damage;

        if (target.hitPoints <= 0)
        {
            target.hitPoints = 0;
            target.setCondition(ConditionIndex::Unconscious);
            result.description = monster.name + " knocks out " + target.name + " (" +
                                 std::to_string(result.damage) + " damage)";

            if (callbacks_.onCharacterDowned)
            {
                callbacks_.onCharacterDowned(monster.targetCharacter);
            }
        }
        else
        {
            result.description = monster.name + " hits " + target.name + " for " +
                                 std::to_string(result.damage) + " damage";
        }
    }

    if (callbacks_.onMonsterAttack)
    {
        callbacks_.onMonsterAttack(monster, monster.targetCharacter, result);
    }

    // Recovery time from monster stats
    monster.recoveryTime =
        std::max(500, (it != monsterDefs_.end()) ? it->second.recovery * 100 : 1000);
    monster.aiState = MonsterInstance::AIState::Standing;
}

void CombatSystem::distributeXP(int xp)
{
    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();
    int conscious = party.consciousCount();
    if (conscious == 0)
        return;

    int xpEach = xp / conscious;
    for (int i = 0; i < kPartySize; i++)
    {
        auto& ch = party.member(i);
        if (ch.isConscious())
        {
            ch.experience += xpEach;
        }
    }
}

} // namespace runeharbor::game
