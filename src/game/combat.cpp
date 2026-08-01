// SPDX-License-Identifier: MIT
#include "combat.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <format>
#include <random>
#include <string_view>

#include <cmath>

#include "../util/string_utils.hpp"
#include "game_world.hpp"
#include "inventory.hpp"

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

int pickFirstConscious(const Party& party)
{
    for (int i = 0; i < kPartySize; i++)
    {
        if (party.member(i).isConscious())
        {
            return i;
        }
    }
    return -1;
}

bool isStateRecoverable(MonsterInstance::AIState state)
{
    return state == MonsterInstance::AIState::Paralyzed ||
           state == MonsterInstance::AIState::Stunned || state == MonsterInstance::AIState::Stoned;
}

bool isAggressiveState(MonsterInstance::AIState state)
{
    return state == MonsterInstance::AIState::Pursuing ||
           state == MonsterInstance::AIState::Attacking ||
           state == MonsterInstance::AIState::AttackingMelee2 ||
           state == MonsterInstance::AIState::AttackingRanged ||
           state == MonsterInstance::AIState::CastingSpell1 ||
           state == MonsterInstance::AIState::CastingSpell2 ||
           state == MonsterInstance::AIState::CastingSpell3;
}

MonsterInstance::Personality parsePersonality(std::string_view aiTypeRaw)
{
    const std::string aiType = util::toLower(std::string(aiTypeRaw));
    if (aiType.find("wimp") != std::string::npos)
    {
        return MonsterInstance::Personality::Wimp;
    }
    if (aiType.find("aggress") != std::string::npos)
    {
        return MonsterInstance::Personality::Aggressive;
    }
    if (aiType.find("suicid") != std::string::npos)
    {
        return MonsterInstance::Personality::Suicidal;
    }
    if (aiType.find("friend") != std::string::npos || aiType.find("civil") != std::string::npos)
    {
        return MonsterInstance::Personality::Friendly;
    }
    return MonsterInstance::Personality::Normal;
}

} // namespace

CombatSystem::CombatSystem(util::ILogger& logger) : logger_(logger) {}

const formats::MonsterEntry* CombatSystem::getMonsterDef(int monsterId) const
{
    auto it = monsterDefs_.find(monsterId);
    if (it != monsterDefs_.end())
    {
        return &it->second;
    }
    return nullptr;
}

void CombatSystem::loadMonsterData(const std::vector<formats::MonsterEntry>& monsters)
{
    monsterDefs_.clear();
    warnedUnknownMonsterIds_.clear();
    for (const auto& m : monsters)
    {
        monsterDefs_[m.id] = m;
    }
    logger_.info("Loaded " + std::to_string(monsters.size()) + " monster definitions");
}

void CombatSystem::setPartyHostilityByMonsterId(std::unordered_map<int, bool> hostilityByMonsterId)
{
    partyHostilityByMonsterId_ = std::move(hostilityByMonsterId);

    for (auto& monster : monsters_)
    {
        if (!monster.isAlive())
        {
            continue;
        }
        if (auto it = partyHostilityByMonsterId_.find(monster.monsterId);
            it != partyHostilityByMonsterId_.end())
        {
            monster.hostile = it->second;
            if (!monster.hostile)
            {
                monster.aiState = MonsterInstance::AIState::Standing;
                monster.targetCharacter = -1;
            }
        }
    }
}

int CombatSystem::spawnMonster(int monsterId, float x, float y, float z, int group)
{
    auto it = monsterDefs_.find(monsterId);
    const formats::MonsterEntry* def = nullptr;
    if (it != monsterDefs_.end())
    {
        def = &it->second;
    }
    else
    {
        if (warnedUnknownMonsterIds_.insert(monsterId).second)
        {
            logger_.warning("Unknown monster ID: " + std::to_string(monsterId) +
                            " (using fallback definition)");
        }
    }

    formats::MonsterEntry fallback;
    if (!def)
    {
        fallback.id = monsterId;
        fallback.name = "Unknown " + std::to_string(monsterId);
        fallback.level = std::max(1, monsterId);
        fallback.hitPoints = std::max(10, 15 + fallback.level * 2);
        fallback.armorClass = std::max(0, fallback.level / 2);
        fallback.experience = fallback.level * 8;
        fallback.speed = 120;
        fallback.recovery = 100;
        fallback.aiType = "Aggress";
        def = &fallback;
    }

    MonsterInstance inst;
    inst.monsterId = monsterId;
    inst.name = def->name;
    inst.currentHP = def->hitPoints;
    inst.maxHP = def->hitPoints;
    inst.armorClass = def->armorClass;
    inst.level = def->level;
    inst.experience = def->experience;
    inst.speed = def->speed;
    inst.recoveryTime = 0;
    inst.x = x;
    inst.y = y;
    inst.z = z;
    inst.group = group;
    inst.aiState = MonsterInstance::AIState::Standing;
    inst.personality = parsePersonality(def->aiType);
    inst.hostile = (inst.personality != MonsterInstance::Personality::Friendly);
    if (auto itHostility = partyHostilityByMonsterId_.find(monsterId);
        itHostility != partyHostilityByMonsterId_.end())
    {
        inst.hostile = itHostility->second;
    }
    if (inst.personality == MonsterInstance::Personality::Aggressive ||
        inst.personality == MonsterInstance::Personality::Suicidal)
    {
        inst.aggroRange *= 1.5f;
    }

    inst.resistFire = def->resistFire;
    inst.resistAir = def->resistAir;
    inst.resistWater = def->resistWater;
    inst.resistEarth = def->resistEarth;
    inst.resistMind = def->resistMind;
    inst.resistSpirit = def->resistSpirit;
    inst.resistBody = def->resistBody;
    inst.resistLight = def->resistLight;
    inst.resistDark = def->resistDark;
    inst.resistPhysical = def->resistPhysical;

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

    if (gameWorld_->runtimeConfig().noMonsters)
    {
        inCombat_ = false;
        return;
    }

    // Turn-based: the world is paused while awaiting player input. Monster
    // turns are resolved instantly in processMonsterTurn, not via per-frame
    // Victory check runs every frame in both modes. In turn-based mode the
    // monster-tick loop below is skipped, but combat must still end when the
    // last monster dies — otherwise the round loop rebuilds empty queues
    // forever and the player can never leave combat.
    if (inCombat_ && aliveMonsterCount() == 0)
    {
        inCombat_ = false;
        if (turnBased_)
        {
            // Tear down the queue so any stale turn state is cleared.
            turnQueue_.clear();
            awaitingPlayerInput_ = false;
        }
        return;
    }

    // Turn-based mode: monster turns are resolved synchronously in
    // processMonsterTurn, not via per-frame ticking.
    if (turnBased_)
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
            if (monster.recoveryTime > 0)
            {
                continue;
            }
        }

        if (isStateRecoverable(monster.aiState))
        {
            monster.aiState = MonsterInstance::AIState::Standing;
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
    int baseDamage = 1 + mightBonus; // Hand-to-hand base is 1 + might

    bool usingWeapon = false;
    if (inventory_)
    {
        const auto& charInv = inventory_->getInventory(characterIndex);
        const auto& mainHandItem = charInv.equipped[static_cast<size_t>(EquipSlot::MainHand)];

        if (mainHandItem.valid())
        {
            const auto* itemDef = inventory_->getItemDef(mainHandItem.itemId);
            if (itemDef && !itemDef->mod1.empty())
            {
                usingWeapon = true;
                int weaponDmg = rollDamage(itemDef->mod1) + itemDef->mod2;
                baseDamage = mightBonus + weaponDmg;
            }
        }
    }

    // Add level bonus? Original code added level. MM7 usually adds skill mastery bonus instead.
    // Let's keep a small level scaling if no weapon.
    if (!usingWeapon)
    {
        baseDamage += ch.level;
    }

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
        // std::from_chars doesn't parse leading '+', so skip it
        size_t bonusStart = plusPos;
        if (diceExpr[plusPos] == '+')
            bonusStart++;
        std::from_chars(diceExpr.data() + bonusStart, diceExpr.data() + diceExpr.size(), bonus);
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

int CombatSystem::calculateMonsterDamage(int baseDamage, DamageElement type,
                                         int characterIndex) const
{
    if (!gameWorld_ || characterIndex < 0 || characterIndex >= kPartySize)
        return baseDamage;

    if (gameWorld_->runtimeConfig().noDamage)
        return 0;

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

void CombatSystem::setMonsterTopic(int monsterIndex, uint16_t topic)
{
    auto* monster = getMonster(monsterIndex);
    if (!monster)
    {
        return;
    }
    monster->topic = topic;
}

void CombatSystem::setMonsterField(int monsterId, int fieldIndex, int value)
{
    if (fieldIndex < 0 || fieldIndex >= 8)
    {
        return;
    }

    for (auto& monster : monsters_)
    {
        if (monster.monsterId != monsterId)
        {
            continue;
        }
        monster.scriptFields[static_cast<size_t>(fieldIndex)] = value;
    }
}

void CombatSystem::setMonsterHostileByGroup(int group, bool hostile)
{
    bool changed = false;
    for (auto& monster : monsters_)
    {
        if (!monster.isAlive() || monster.group != group)
        {
            continue;
        }
        monster.hostile = hostile;
        monster.aiState =
            hostile ? MonsterInstance::AIState::Pursuing : MonsterInstance::AIState::Standing;
        if (hostile)
        {
            monster.targetCharacter = 0;
            for (int i = 0; i < kPartySize; i++)
            {
                if (gameWorld_ && gameWorld_->party().member(i).isConscious())
                {
                    monster.targetCharacter = i;
                    break;
                }
            }
        }
        else
        {
            monster.targetCharacter = -1;
        }
        changed = true;
    }

    if (changed && hostile)
    {
        inCombat_ = true;
    }
}

void CombatSystem::setMonsterHostileByIndex(int index, bool hostile)
{
    auto* monster = getMonster(index);
    if (!monster || !monster->isAlive())
    {
        return;
    }

    monster->hostile = hostile;
    monster->aiState =
        hostile ? MonsterInstance::AIState::Pursuing : MonsterInstance::AIState::Standing;
    if (hostile)
    {
        monster->targetCharacter = 0;
        for (int i = 0; i < kPartySize; i++)
        {
            if (gameWorld_ && gameWorld_->party().member(i).isConscious())
            {
                monster->targetCharacter = i;
                break;
            }
        }
        inCombat_ = true;
    }
    else
    {
        monster->targetCharacter = -1;
    }
}

void CombatSystem::replaceMonsterType(int oldMonsterId, int newMonsterId)
{
    auto defIt = monsterDefs_.find(newMonsterId);
    if (defIt == monsterDefs_.end())
    {
        logger_.warning("ReplaceMonster ignored: unknown new monster ID " +
                        std::to_string(newMonsterId));
        return;
    }

    const auto& def = defIt->second;
    for (auto& monster : monsters_)
    {
        if (!monster.isAlive() || monster.monsterId != oldMonsterId)
        {
            continue;
        }

        const float hpRatio = (monster.maxHP > 0) ? static_cast<float>(monster.currentHP) /
                                                        static_cast<float>(monster.maxHP)
                                                  : 1.0f;
        monster.monsterId = newMonsterId;
        monster.name = def.name;
        monster.maxHP = std::max(1, def.hitPoints);
        monster.currentHP = std::max(1, static_cast<int>(monster.maxHP * hpRatio));
        monster.armorClass = def.armorClass;
        monster.level = def.level;
        monster.experience = def.experience;
        monster.speed = def.speed;
        monster.personality = parsePersonality(def.aiType);
        if (auto itHostility = partyHostilityByMonsterId_.find(newMonsterId);
            itHostility != partyHostilityByMonsterId_.end())
        {
            monster.hostile = itHostility->second;
        }
        else
        {
            monster.hostile = (monster.personality != MonsterInstance::Personality::Friendly);
        }
        if (!monster.hostile)
        {
            monster.aiState = MonsterInstance::AIState::Standing;
            monster.targetCharacter = -1;
        }
        monster.resistFire = def.resistFire;
        monster.resistAir = def.resistAir;
        monster.resistWater = def.resistWater;
        monster.resistEarth = def.resistEarth;
        monster.resistMind = def.resistMind;
        monster.resistSpirit = def.resistSpirit;
        monster.resistBody = def.resistBody;
        monster.resistLight = def.resistLight;
        monster.resistDark = def.resistDark;
        monster.resistPhysical = def.resistPhysical;
    }
}

void CombatSystem::setMonsterAiByType(int monsterId, MonsterInstance::AIState state)
{
    for (auto& monster : monsters_)
    {
        if (!monster.isAlive() || monster.monsterId != monsterId)
        {
            continue;
        }
        monster.aiState = state;
        if (isAggressiveState(state))
        {
            monster.hostile = true;
        }
        if (state == MonsterInstance::AIState::Pursuing ||
            state == MonsterInstance::AIState::Attacking ||
            state == MonsterInstance::AIState::AttackingRanged)
        {
            inCombat_ = true;
        }
    }
}

void CombatSystem::setMonsterAiByGroup(int group, MonsterInstance::AIState state)
{
    for (auto& monster : monsters_)
    {
        if (!monster.isAlive() || monster.group != group)
        {
            continue;
        }
        monster.aiState = state;
        if (isAggressiveState(state))
        {
            monster.hostile = true;
        }
        if (state == MonsterInstance::AIState::Pursuing ||
            state == MonsterInstance::AIState::Attacking ||
            state == MonsterInstance::AIState::AttackingRanged)
        {
            inCombat_ = true;
        }
    }
}

void CombatSystem::updateMonsterAI(MonsterInstance& monster, float deltaMs)
{
    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();
    if (party.consciousCount() <= 0)
    {
        monster.aiState = MonsterInstance::AIState::Standing;
        monster.targetCharacter = -1;
        return;
    }

    const float partyX = party.worldX();
    const float partyY = party.worldY();
    const float partyZ = party.worldZ();
    const float dx = partyX - monster.x;
    const float dy = partyY - monster.y;
    const float dz = partyZ - monster.z;
    const float distSq = dx * dx + dy * dy + dz * dz;

    const float visibilityRange = gameWorld_->isIndoorMap() ? 10239.0f : 5631.0f;
    const float visibilitySq = visibilityRange * visibilityRange;
    if (monster.aiState != MonsterInstance::AIState::Fleeing && distSq > visibilitySq)
    {
        monster.aiState = MonsterInstance::AIState::Standing;
        monster.targetCharacter = -1;
        return;
    }

    if (monster.personality == MonsterInstance::Personality::Wimp && monster.maxHP > 0 &&
        monster.currentHP * 4 <= monster.maxHP)
    {
        monster.aiState = MonsterInstance::AIState::Fleeing;
    }

    if (!monster.hostile && isAggressiveState(monster.aiState))
    {
        monster.aiState = MonsterInstance::AIState::Standing;
        monster.targetCharacter = -1;
    }

    switch (monster.aiState)
    {
    case MonsterInstance::AIState::Standing:
    case MonsterInstance::AIState::Guarding:
    case MonsterInstance::AIState::Fidgeting:
    case MonsterInstance::AIState::Wandering:
    {
        if (!monster.hostile)
        {
            if (monster.aiState == MonsterInstance::AIState::Wandering ||
                monster.aiState == MonsterInstance::AIState::Fidgeting)
            {
                const float moveUnits =
                    std::max(4.0f, static_cast<float>(monster.speed) * (deltaMs / 1000.0f));
                const float randX = static_cast<float>(randomInt(-100, 100)) / 100.0f;
                const float randY = static_cast<float>(randomInt(-100, 100)) / 100.0f;
                monster.x += randX * moveUnits;
                monster.y += randY * moveUnits;
            }
            break;
        }

        if (distSq < monster.aggroRange * monster.aggroRange)
        {
            monster.aiState = MonsterInstance::AIState::Pursuing;
            int target = party.activeMemberIndex();
            if (target < 0 || target >= kPartySize || !party.member(target).isConscious())
            {
                target = pickFirstConscious(party);
            }
            if (target >= 0)
            {
                monster.targetCharacter = target;
            }

            if (monster.group != 0 && target >= 0)
            {
                for (auto& ally : monsters_)
                {
                    if (&ally == &monster || !ally.isAlive() || ally.group != monster.group)
                    {
                        continue;
                    }

                    ally.hostile = true;
                    if (ally.aiState == MonsterInstance::AIState::Standing ||
                        ally.aiState == MonsterInstance::AIState::Guarding ||
                        ally.aiState == MonsterInstance::AIState::Fidgeting ||
                        ally.aiState == MonsterInstance::AIState::Wandering)
                    {
                        ally.aiState = MonsterInstance::AIState::Pursuing;
                    }
                    ally.targetCharacter = target;
                }
            }
            inCombat_ = true;
        }
        else if (monster.aiState == MonsterInstance::AIState::Wandering ||
                 monster.aiState == MonsterInstance::AIState::Fidgeting)
        {
            const float moveUnits =
                std::max(4.0f, static_cast<float>(monster.speed) * (deltaMs / 1000.0f));
            const float randX = static_cast<float>(randomInt(-100, 100)) / 100.0f;
            const float randY = static_cast<float>(randomInt(-100, 100)) / 100.0f;
            monster.x += randX * moveUnits;
            monster.y += randY * moveUnits;
        }
        break;
    }
    case MonsterInstance::AIState::Pursuing:
    {
        if (monster.targetCharacter < 0 || monster.targetCharacter >= kPartySize ||
            !party.member(monster.targetCharacter).isConscious())
        {
            monster.targetCharacter = pickFirstConscious(party);
            if (monster.targetCharacter < 0)
            {
                monster.aiState = MonsterInstance::AIState::Standing;
                return;
            }
        }

        const float dist2d = std::sqrt(dx * dx + dy * dy);
        constexpr float kMeleeRange = 220.0f;
        if (dist2d <= kMeleeRange)
        {
            monster.aiState = MonsterInstance::AIState::Attacking;
            break;
        }

        const float unitsPerSecond = std::max(75.0f, static_cast<float>(monster.speed) * 16.0f);
        const float moveAmount = unitsPerSecond * (deltaMs / 1000.0f);
        if (dist2d > 0.001f)
        {
            const float inv = 1.0f / dist2d;
            monster.x += dx * inv * moveAmount;
            monster.y += dy * inv * moveAmount;
        }
        break;
    }

    case MonsterInstance::AIState::Attacking:
    case MonsterInstance::AIState::AttackingMelee2:
    case MonsterInstance::AIState::AttackingRanged:
        monsterAttack(monster);
        break;

    case MonsterInstance::AIState::Fleeing:
    {
        const float dist2d = std::sqrt(dx * dx + dy * dy);
        const float fleeRange = visibilityRange * 1.5f;
        if (dist2d > fleeRange)
        {
            monster.aiState = MonsterInstance::AIState::Dead;
            monster.currentHP = 0;
            break;
        }

        const float unitsPerSecond = std::max(80.0f, static_cast<float>(monster.speed) * 12.0f);
        const float moveAmount = unitsPerSecond * (deltaMs / 1000.0f);
        if (dist2d > 0.001f)
        {
            const float inv = 1.0f / dist2d;
            monster.x -= dx * inv * moveAmount;
            monster.y -= dy * inv * moveAmount;
        }
        break;
    }

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
            if (result.damage <= 0)
            {
                result.description = monster.name + " hits " + target.name + " but deals no damage";
            }
            else
            {
                result.description = monster.name + " hits " + target.name + " for " +
                                     std::to_string(result.damage) + " damage";
            }
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

void CombatSystem::awardMonsterKill(MonsterInstance& monster)
{
    // Mirror the kill-handling in playerAttack() so spell kills (resolved by
    // SpellSystem) and melee kills share the same XP + death-callback path.
    distributeXP(monster.experience);
    if (callbacks_.onMonsterKilled)
    {
        callbacks_.onMonsterKilled(monster, monster.experience);
    }
}

// -------- Turn-based combat --------

void CombatSystem::setTurnBased(bool tb)
{
    if (turnBased_ == tb)
        return;
    turnBased_ = tb;
    if (!tb)
    {
        turnQueue_.clear();
        awaitingPlayerInput_ = false;
        tbRound_ = 0;
        return;
    }
    if (inCombat_)
    {
        startTurnBasedRound();
    }
}

void CombatSystem::startTurnBasedRound()
{
    // NOTE: this initiative model is a simplification of the RE design in
    // docs/turn-based-combat.md §4. RuneHarbor computes initiative once per
    // round (speed*2 + randomInt(1,10), min 30 for players) and does a flat
    // ascending sort. The original uses a continuous-initiative countdown
    // (RecomputeInit fcn.0040652a / QueueAdvance fcn.00406457) that re-sorts
    // after each action, a per-monster-type TB recovery from the 0x5ccd10
    // table, a 32/15 (~2.13) recovery multiplier, and a haste-doubles-
    // initiative branch. Porting the full countdown model is a follow-up.
    ++tbRound_;
    turnQueue_.clear();
    tbQueueIdx_ = 0;

    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();
    for (int i = 0; i < kPartySize; i++)
    {
        if (party.member(i).isConscious())
        {
            TurnActor a;
            a.type = TurnActor::Type::Player;
            a.index = i;
            const int speed = party.member(i).effectiveStat(4); // Speed stat
            a.initiative = std::max(30, speed * 2 + (randomInt(1, 10)));
            turnQueue_.push_back(a);
        }
    }
    for (size_t i = 0; i < monsters_.size(); i++)
    {
        if (monsters_[i].isAlive() && monsters_[i].hostile)
        {
            TurnActor a;
            a.type = TurnActor::Type::Monster;
            a.index = static_cast<int>(i);
            a.initiative = monsters_[i].speed * 2 + randomInt(1, 10);
            turnQueue_.push_back(a);
        }
    }
    // Sort ascending by initiative; tie-break: monster before player.
    std::sort(turnQueue_.begin(), turnQueue_.end(),
              [](const TurnActor& a, const TurnActor& b)
              {
                  if (a.initiative != b.initiative)
                      return a.initiative < b.initiative;
                  // Same initiative: monster first (a < b if a is monster, b is player).
                  return a.type == TurnActor::Type::Monster && b.type == TurnActor::Type::Player;
              });

    // Process any leading monster turns until we reach a player.
    processMonsterTurn();
}

int CombatSystem::currentTurnPlayerIndex() const
{
    if (!turnBased_ || tbQueueIdx_ >= turnQueue_.size())
        return -1;
    const auto& actor = turnQueue_[tbQueueIdx_];
    return actor.type == TurnActor::Type::Player ? actor.index : -1;
}

void CombatSystem::processMonsterTurn()
{
    while (tbQueueIdx_ < turnQueue_.size())
    {
        auto& actor = turnQueue_[tbQueueIdx_];
        if (actor.type == TurnActor::Type::Player)
        {
            // Check the player is still conscious.
            if (gameWorld_ && gameWorld_->party().member(actor.index).isConscious())
            {
                awaitingPlayerInput_ = true;
                return;
            }
            // Skip unconscious player.
            ++tbQueueIdx_;
            continue;
        }
        // Monster turn: execute one AI step + attack.
        if (actor.index >= 0 && actor.index < static_cast<int>(monsters_.size()))
        {
            auto& m = monsters_[static_cast<size_t>(actor.index)];
            if (m.isAlive())
            {
                updateMonsterAI(m, 100.0f);
                if (m.aiState == MonsterInstance::AIState::Attacking ||
                    m.aiState == MonsterInstance::AIState::AttackingMelee2 ||
                    m.aiState == MonsterInstance::AIState::AttackingRanged)
                {
                    monsterAttack(m);
                }
            }
        }
        ++tbQueueIdx_;
    }
    // Queue exhausted — start a new round.
    startTurnBasedRound();
}

void CombatSystem::advanceQueue()
{
    awaitingPlayerInput_ = false;
    ++tbQueueIdx_;
    processMonsterTurn();
}

void CombatSystem::completePlayerTurn()
{
    if (!turnBased_)
        return;
    advanceQueue();
}

std::string CombatSystem::turnStatusText() const
{
    if (!turnBased_)
        return {};
    if (tbQueueIdx_ >= turnQueue_.size())
        return std::format("Round {} — starting...", tbRound_);
    const auto& actor = turnQueue_[tbQueueIdx_];
    if (actor.type == TurnActor::Type::Player && gameWorld_)
    {
        const auto& ch = gameWorld_->party().member(actor.index);
        return std::format("Round {} — {}'s turn", tbRound_, ch.name);
    }
    return std::format("Round {} — Monster turn", tbRound_);
}

} // namespace runeharbor::game
