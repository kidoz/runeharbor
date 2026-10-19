// SPDX-License-Identifier: MIT
#include "combat.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <format>
#include <optional>
#include <random>
#include <string_view>

#include <cctype>
#include <cmath>

#include "../util/string_utils.hpp"
#include "game_world.hpp"
#include "inventory.hpp"
#include "spells.hpp"

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

/// Fast 3D distance approximation used by projectile attacks
/// (docs/combat-system.md §4.2): max + min/4 + mid*11/32.
float approximateDistance(float ax, float ay, float az, float bx, float by, float bz)
{
    const float dx = std::abs(ax - bx);
    const float dy = std::abs(ay - by);
    const float dz = std::abs(az - bz);
    const float maxD = std::max({dx, dy, dz});
    const float minD = std::min({dx, dy, dz});
    const float midD = dx + dy + dz - maxD - minD;
    return maxD + minD * 0.25f + midD * (11.0f / 32.0f);
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

std::string stripQuotes(std::string_view raw)
{
    const size_t first = raw.find_first_not_of(" \t\"'");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const size_t last = raw.find_last_not_of(" \t\"'");
    return std::string(raw.substr(first, last - first + 1));
}

std::string normalizedToken(std::string_view raw)
{
    std::string result;
    result.reserve(raw.size());
    for (char c : raw)
    {
        const auto uc = static_cast<unsigned char>(c);
        if (c == '"' || c == '\'' || std::isspace(uc))
        {
            continue;
        }
        result.push_back(static_cast<char>(std::tolower(uc)));
    }
    return result;
}

DamageElement damageElementFromName(std::string_view raw)
{
    const std::string name = normalizedToken(raw);
    if (name == "fire")
        return DamageElement::Fire;
    if (name == "air")
        return DamageElement::Air;
    if (name == "water")
        return DamageElement::Water;
    if (name == "earth")
        return DamageElement::Earth;
    if (name == "mind")
        return DamageElement::Mind;
    if (name == "spirit")
        return DamageElement::Spirit;
    if (name == "body")
        return DamageElement::Body;
    if (name == "light")
        return DamageElement::Light;
    if (name == "dark")
        return DamageElement::Dark;
    if (name == "ener")
        return DamageElement::Light; // Energy is approximated with Light resistance.
    return DamageElement::Physical;
}

bool parseShotCount(std::string_view special, int& shots)
{
    const std::string text = stripQuotes(special);
    if (text.size() < 6 || util::toLower(text.substr(0, 4)) != "shot")
    {
        return false;
    }
    const size_t comma = text.find(',');
    if (comma == std::string::npos)
    {
        return false;
    }
    const size_t x = text.find('x', comma);
    if (x == std::string::npos)
    {
        return false;
    }
    int value = 0;
    const char* begin = text.data() + x + 1;
    const char* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr == begin || value <= 0 || value > 20)
    {
        return false;
    }
    shots = value;
    return true;
}

bool parseExplode(std::string_view special, std::string& dice, DamageElement& element)
{
    const std::string text = stripQuotes(special);
    if (text.size() < 8 || util::toLower(text.substr(0, 8)) != "explode,")
    {
        return false;
    }
    const auto parts = util::splitString(text, ',', '"');
    if (parts.size() < 2)
    {
        return false;
    }
    dice = util::trim(parts[1]);
    element = parts.size() > 2 ? damageElementFromName(parts[2]) : DamageElement::Physical;
    return !dice.empty();
}

bool parseSpellSpec(std::string_view raw, std::string& name, SkillMastery& mastery, int& level)
{
    const std::string text = stripQuotes(raw);
    if (text.empty() || text == "0")
    {
        return false;
    }
    const auto parts = util::splitString(text, ',', '"');
    if (parts.empty())
    {
        return false;
    }
    name = util::trim(parts[0]);
    if (name.empty() || name == "0")
    {
        return false;
    }

    mastery = SkillMastery::Normal;
    if (parts.size() > 1)
    {
        const std::string token = util::toLower(util::trim(parts[1]));
        if (!token.empty())
        {
            switch (token[0])
            {
            case 'e':
                mastery = SkillMastery::Expert;
                break;
            case 'm':
                mastery = SkillMastery::Master;
                break;
            case 'g':
                mastery = SkillMastery::GrandMaster;
                break;
            default:
                mastery = SkillMastery::Normal;
                break;
            }
        }
    }

    level = 1;
    if (parts.size() > 2)
    {
        int value = 0;
        const std::string token = util::trim(parts[2]);
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (ec == std::errc{} && ptr != token.data())
        {
            level = std::clamp(value, 1, 60);
        }
    }
    return true;
}

bool isBeneficialSpellName(std::string_view name)
{
    const std::string lower = util::toLower(std::string(name));
    static constexpr std::string_view kBeneficial[] = {
        "cure",   "heal",   "bless",  "protection",   "power",   "haste",
        "shield", "skin",   "resist", "regeneration", "heroism", "luck",
        "speed",  "day of", "armor",  "might",        "grace",   "endurance",
    };
    for (std::string_view token : kBeneficial)
    {
        if (lower.find(token) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

std::optional<ConditionIndex> conditionForBonus(std::string_view bonus)
{
    const std::string name = normalizedToken(bonus);
    if (name == "disease1")
        return ConditionIndex::Disease1;
    if (name == "disease2")
        return ConditionIndex::Disease2;
    if (name == "disease3")
        return ConditionIndex::Disease3;
    if (name == "poison1")
        return ConditionIndex::Poison1;
    if (name == "poison2")
        return ConditionIndex::Poison2;
    if (name == "poison3")
        return ConditionIndex::Poison3;
    if (name == "afraid")
        return ConditionIndex::Afraid;
    if (name == "insane")
        return ConditionIndex::Insane;
    if (name == "asleep")
        return ConditionIndex::Asleep;
    if (name == "paralyze")
        return ConditionIndex::Paralyzed;
    if (name == "uncon")
        return ConditionIndex::Unconscious;
    if (name == "dead")
        return ConditionIndex::Dead;
    if (name == "errad" || name == "eradicate")
        return ConditionIndex::Eradicated;
    if (name == "stone")
        return ConditionIndex::Stoned;
    if (name == "curse" || name == "cursex2")
        return ConditionIndex::Cursed;
    if (name == "weak")
        return ConditionIndex::Weak;
    if (name == "drunk")
        return ConditionIndex::Drunk;
    return std::nullopt;
}

struct TreasureSpec
{
    int chancePercent = 100;
    std::string dice;
    int itemLevel = 0;
    std::string itemType;
};

TreasureSpec parseTreasure(std::string_view raw)
{
    TreasureSpec spec;
    std::string text = stripQuotes(raw);
    if (text.empty() || text == "0")
    {
        return spec;
    }

    size_t digits = 0;
    while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits])))
    {
        ++digits;
    }
    if (digits > 0 && digits < text.size() && text[digits] == '%')
    {
        int chance = 0;
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + digits, chance);
        if (ec == std::errc{})
        {
            spec.chancePercent = std::clamp(chance, 0, 100);
        }
        text = text.substr(digits + 1);
    }

    std::string itemPart;
    const size_t plusL = text.find("+L");
    const size_t plusl = text.find("+l");
    if (plusL != std::string::npos || plusl != std::string::npos)
    {
        const size_t split = plusL != std::string::npos ? plusL : plusl;
        spec.dice = text.substr(0, split);
        itemPart = text.substr(split + 2);
    }
    else if (!text.empty() && (text[0] == 'L' || text[0] == 'l'))
    {
        itemPart = text.substr(1);
    }
    else
    {
        spec.dice = text;
    }

    if (!itemPart.empty())
    {
        size_t levelDigits = 0;
        while (levelDigits < itemPart.size() &&
               std::isdigit(static_cast<unsigned char>(itemPart[levelDigits])))
        {
            ++levelDigits;
        }
        if (levelDigits > 0)
        {
            int level = 0;
            const auto [ptr, ec] =
                std::from_chars(itemPart.data(), itemPart.data() + levelDigits, level);
            if (ec == std::errc{})
            {
                spec.itemLevel = std::clamp(level, 1, 20);
            }
        }
        spec.itemType = itemPart.substr(levelDigits);
    }

    return spec;
}

} // namespace

CombatSystem::CombatSystem(util::ILogger& logger) : logger_(logger) {}

std::optional<ConditionIndex> CombatSystem::conditionFromBonusName(std::string_view bonus)
{
    return conditionForBonus(bonus);
}

DamageElement CombatSystem::damageElementFromMonsterType(std::string_view type)
{
    return damageElementFromName(type);
}

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

std::vector<SavedMonsterState> CombatSystem::snapshotAliveMonsters() const
{
    std::vector<SavedMonsterState> states;
    states.reserve(monsters_.size());
    for (const auto& monster : monsters_)
    {
        if (!monster.isAlive())
        {
            continue;
        }

        SavedMonsterState state;
        state.monsterId = monster.monsterId;
        state.x = monster.x;
        state.y = monster.y;
        state.z = monster.z;
        state.velocityX = monster.velocityX;
        state.velocityY = monster.velocityY;
        state.velocityZ = monster.velocityZ;
        state.facingAngle = monster.facingAngle;
        state.currentHP = monster.currentHP;
        state.maxHP = monster.maxHP;
        state.group = monster.group;
        state.topic = monster.topic;
        state.aiState = static_cast<uint8_t>(monster.aiState);
        state.hostilityLevel = monster.hostilityLevel;
        state.hostilityOverride = monster.hostilityOverride;
        state.flags = monster.flags;
        state.flagsExtended = monster.flagsExtended;
        state.targetCharacter = monster.targetCharacter;
        state.targetId = monster.targetId;
        state.scriptFields = monster.scriptFields;
        states.push_back(state);
    }
    return states;
}

void CombatSystem::restoreMonsters(const std::vector<SavedMonsterState>& states)
{
    monsters_.clear();
    inCombat_ = false;

    for (const auto& state : states)
    {
        const int index = spawnMonster(state.monsterId, state.x, state.y, state.z, state.group);
        auto* monster = getMonster(index);
        if (monster == nullptr)
        {
            continue;
        }

        monster->velocityX = state.velocityX;
        monster->velocityY = state.velocityY;
        monster->velocityZ = state.velocityZ;
        monster->facingAngle = state.facingAngle;
        monster->currentHP = std::max(1, state.currentHP);
        monster->maxHP = std::max(monster->currentHP, state.maxHP);
        monster->topic = state.topic;
        monster->aiState = static_cast<MonsterInstance::AIState>(state.aiState);
        monster->hostilityLevel = state.hostilityLevel;
        monster->hostilityOverride = state.hostilityOverride;
        monster->flags = state.flags;
        monster->flagsExtended = state.flagsExtended;
        monster->targetCharacter = state.targetCharacter;
        monster->targetId = state.targetId;
        monster->scriptFields = state.scriptFields;
        monster->deathHandled = false;
    }
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
    if (!ch.canAct())
    {
        result.description = ch.name + " is unable to act";
        return result;
    }

    auto* monster = getMonster(monsterIndex);
    if (!monster || !monster->isAlive())
    {
        result.description = "No valid target";
        return result;
    }

    const float distance = approximateDistance(party.worldX(), party.worldY(), party.worldZ(),
                                               monster->x, monster->y, monster->z);

    bool hasMeleeWeapon = false;
    bool hasBow = false;
    if (inventory_)
    {
        const auto& charInv = inventory_->getInventory(characterIndex);
        const auto& mainHand = charInv.equipped[static_cast<size_t>(EquipSlot::MainHand)];
        if (mainHand.valid() && !mainHand.broken)
        {
            const auto* def = inventory_->getItemDef(mainHand.itemId);
            hasMeleeWeapon = def != nullptr && !def->mod1.empty();
        }
        const auto& bow = charInv.equipped[static_cast<size_t>(EquipSlot::Bow)];
        if (bow.valid() && !bow.broken)
        {
            hasBow = inventory_->getItemDef(bow.itemId) != nullptr;
        }
    }

    if (distance > kMeleeReach)
    {
        if (hasBow)
        {
            return playerRangedAttack(characterIndex, monsterIndex);
        }
        result.description = ch.name + " cannot reach " + monster->name;
        return result;
    }

    // At melee range the bow is only used when no melee weapon is ready.
    if (!hasMeleeWeapon && hasBow)
    {
        return playerRangedAttack(characterIndex, monsterIndex);
    }

    // Calculate hit chance based on accuracy vs AC. Effective accuracy includes
    // timed buffs (Bless, Heroism, ...).
    int accuracy = ch.effectiveStat(4);
    int chance = hitChance(ch.level, accuracy, monster->armorClass);

    if (!rollHit(chance))
    {
        result.hit = false;
        result.description = ch.name + " misses " + monster->name;
        return result;
    }

    result.hit = true;

    // Base damage from might + weapon
    int mightBonus = std::max(0, (ch.effectiveStat(0) - 10) / 2);
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
        result.description =
            ch.name + " kills " + monster->name + " (" + std::to_string(result.damage) + " damage)";
        killMonster(*monster);
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

AttackResult CombatSystem::playerRangedAttack(int characterIndex, int monsterIndex)
{
    AttackResult result;

    if (!gameWorld_ || !inventory_)
        return result;

    auto& party = gameWorld_->party();
    if (characterIndex < 0 || characterIndex >= kPartySize)
        return result;

    auto& ch = party.member(characterIndex);
    if (!ch.canAct())
    {
        result.description = ch.name + " is unable to act";
        return result;
    }

    auto* monster = getMonster(monsterIndex);
    if (!monster || !monster->isAlive())
    {
        result.description = "No valid target";
        return result;
    }

    const auto& charInv = inventory_->getInventory(characterIndex);
    const auto& bow = charInv.equipped[static_cast<size_t>(EquipSlot::Bow)];
    if (!bow.valid() || bow.broken)
    {
        result.description = ch.name + " has no bow ready";
        return result;
    }
    const auto* bowDef = inventory_->getItemDef(bow.itemId);
    if (bowDef == nullptr)
    {
        result.description = ch.name + " has no bow ready";
        return result;
    }

    const float distance = approximateDistance(party.worldX(), party.worldY(), party.worldZ(),
                                               monster->x, monster->y, monster->z);
    if (distance > kRangedMaxRange)
    {
        result.description = monster->name + " is out of range";
        return result;
    }

    // RE (docs/combat-system.md §4.4): ranged damage is the weapon dice roll
    // plus Master-rank training bonus; melee stat bonuses do not apply.
    const auto& bowSkill = ch.skillLevels[static_cast<size_t>(SkillId::Bow)];
    int baseDamage = rollDamage(bowDef->mod1) + bowDef->mod2;
    if (bowSkill.mastery >= SkillMastery::Master)
    {
        baseDamage += bowSkill.effective() * 2;
    }

    // Bow training also improves the attack roll.
    const int accuracy = ch.effectiveStat(4) + bowSkill.effective() * 2;
    const int chance = hitChance(ch.level, accuracy, monster->armorClass);
    if (!rollHit(chance))
    {
        result.hit = false;
        result.description = ch.name + "'s shot misses " + monster->name;
        return result;
    }

    result.hit = true;
    result.damage = calculateDamage(std::max(1, baseDamage), DamageElement::Physical, *monster);
    monster->currentHP -= result.damage;

    if (monster->currentHP <= 0)
    {
        result.description = ch.name + " shoots down " + monster->name + " (" +
                             std::to_string(result.damage) + " damage)";
        killMonster(*monster);
    }
    else
    {
        result.description = ch.name + " shoots " + monster->name + " for " +
                             std::to_string(result.damage) + " damage";
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
        resistance = ch.fireResistance + ch.timedResistanceBonus(0);
        break;
    case DamageElement::Air:
        resistance = ch.airResistance + ch.timedResistanceBonus(1);
        break;
    case DamageElement::Water:
        resistance = ch.waterResistance + ch.timedResistanceBonus(2);
        break;
    case DamageElement::Earth:
        resistance = ch.earthResistance + ch.timedResistanceBonus(3);
        break;
    case DamageElement::Mind:
        resistance = ch.mindResistance + ch.timedResistanceBonus(4);
        break;
    case DamageElement::Body:
        resistance = ch.bodyResistance + ch.timedResistanceBonus(5);
        break;
    case DamageElement::Physical:
        // Approximation: MM7 treats armor class as a hit-avoidance/damage-
        // reduction hybrid (see docs/combat-system.md). RuneHarbor reuses the
        // elemental-resistance pipeline for physical damage, so AC here acts
        // as a flat damage-reduction percentage rather than its RE role.
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
    // RuneHarbor approximation, NOT the RE pipeline. The original hit model
    // (DamageMonsterFromParty 0x00439463, see docs/combat-system.md) folds in
    // weapon skill, mastery, and knockback; this simplified formula captures
    // the accuracy-vs-AC tradeoff and level advantage with a 5-95% clamp.
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
    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();
    int targetIndex = monster.targetCharacter;
    if (targetIndex < 0 || targetIndex >= kPartySize || !party.member(targetIndex).isConscious())
    {
        targetIndex = pickFirstConscious(party);
        monster.targetCharacter = targetIndex;
        if (targetIndex < 0)
        {
            return;
        }
    }
    auto& target = party.member(targetIndex);

    const auto it = monsterDefs_.find(monster.monsterId);
    const formats::MonsterEntry* def = it != monsterDefs_.end() ? &it->second : nullptr;

    const float dx = party.worldX() - monster.x;
    const float dy = party.worldY() - monster.y;
    const float dz = party.worldZ() - monster.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    constexpr float kMeleeRange = 256.0f;
    constexpr float kRangedRange = 5631.0f;

    // Out of melee reach: use a shot special or a spell attack if available,
    // otherwise close the distance. This prevents melee hits from landing at
    // arbitrary range.
    if (distance > kMeleeRange)
    {
        int shots = 0;
        if (def && parseShotCount(def->special, shots) && distance <= kRangedRange)
        {
            monsterRangedAttack(monster, shots);
            return;
        }
        if (distance <= kRangedRange && tryMonsterSpellAttack(monster, targetIndex, distance))
        {
            monster.recoveryTime = std::max(500, def ? def->recovery * 100 : 1000);
            monster.aiState = MonsterInstance::AIState::Standing;
            return;
        }
        monster.aiState = MonsterInstance::AIState::Pursuing;
        return;
    }

    if (tryMonsterSpellAttack(monster, targetIndex, distance))
    {
        monster.recoveryTime = std::max(500, def ? def->recovery * 100 : 1000);
        monster.aiState = MonsterInstance::AIState::Standing;
        return;
    }

    // Choose between the two physical attack definitions. attack2 is often an
    // alternate elemental attack rather than a guaranteed second hit, so pick
    // one per action instead of doubling damage every round.
    const formats::MonsterAttack* attack = nullptr;
    DamageElement element = DamageElement::Physical;
    if (def)
    {
        const bool hasAttack1 = !def->attack1.damage.empty() && def->attack1.damage != "0";
        const bool hasAttack2 = !def->attack2.damage.empty() && def->attack2.damage != "0";
        if (hasAttack2 && (!hasAttack1 || randomInt(0, 1) == 0))
        {
            attack = &def->attack2;
        }
        else if (hasAttack1)
        {
            attack = &def->attack1;
        }
    }

    const std::string damageExpr =
        (attack != nullptr && !attack->damage.empty() && attack->damage != "0") ? attack->damage
                                                                                : "1d4";
    if (attack != nullptr)
    {
        element = damageElementFromName(attack->type);
    }

    AttackResult result;
    const int chance = hitChance(monster.level, monster.level * 3, target.armorClass);

    if (!rollHit(chance))
    {
        result.hit = false;
        result.description = monster.name + " misses " + target.name;
    }
    else
    {
        result.hit = true;
        const int baseDmg = rollDamage(damageExpr);
        result.damage = calculateMonsterDamage(baseDmg, element, targetIndex);
        target.takeDamage(result.damage);

        std::string bonusSuffix;
        applyMonsterBonus(monster, target, bonusSuffix);

        if (target.hitPoints <= 0)
        {
            result.description = monster.name + " knocks out " + target.name + " (" +
                                 std::to_string(result.damage) + " damage)" + bonusSuffix;

            if (callbacks_.onCharacterDowned)
            {
                callbacks_.onCharacterDowned(targetIndex);
            }
        }
        else if (result.damage <= 0)
        {
            result.description =
                monster.name + " hits " + target.name + " but deals no damage" + bonusSuffix;
        }
        else
        {
            result.description = monster.name + " hits " + target.name + " for " +
                                 std::to_string(result.damage) + " damage" + bonusSuffix;
        }
    }

    if (callbacks_.onMonsterAttack)
    {
        callbacks_.onMonsterAttack(monster, targetIndex, result);
    }

    // Recovery time from monster stats
    monster.recoveryTime = std::max(500, def ? def->recovery * 100 : 1000);
    monster.aiState = MonsterInstance::AIState::Standing;
}

void CombatSystem::monsterRangedAttack(MonsterInstance& monster, int shots)
{
    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();
    int targetIndex = monster.targetCharacter;
    if (targetIndex < 0 || targetIndex >= kPartySize || !party.member(targetIndex).isConscious())
    {
        targetIndex = pickFirstConscious(party);
        monster.targetCharacter = targetIndex;
        if (targetIndex < 0)
        {
            return;
        }
    }
    auto& target = party.member(targetIndex);

    const auto it = monsterDefs_.find(monster.monsterId);
    const formats::MonsterEntry* def = it != monsterDefs_.end() ? &it->second : nullptr;

    const formats::MonsterAttack* attack = nullptr;
    if (def)
    {
        if (!def->attack1.damage.empty() && def->attack1.damage != "0")
        {
            attack = &def->attack1;
        }
        else if (!def->attack2.damage.empty() && def->attack2.damage != "0")
        {
            attack = &def->attack2;
        }
    }
    const std::string damageExpr =
        (attack != nullptr && !attack->damage.empty() && attack->damage != "0") ? attack->damage
                                                                                : "1d4";
    const DamageElement element =
        attack != nullptr ? damageElementFromName(attack->type) : DamageElement::Physical;

    AttackResult result;
    int hits = 0;
    for (int shot = 0; shot < shots; ++shot)
    {
        const int chance = hitChance(monster.level, monster.level * 2, target.armorClass);
        if (!rollHit(chance))
        {
            continue;
        }
        const int baseDmg = rollDamage(damageExpr);
        const int damage = calculateMonsterDamage(baseDmg, element, targetIndex);
        target.takeDamage(damage);
        result.damage += damage;
        ++hits;
    }

    result.hit = hits > 0;
    if (hits > 0)
    {
        std::string bonusSuffix;
        applyMonsterBonus(monster, target, bonusSuffix);
        if (target.hitPoints <= 0)
        {
            result.description = monster.name + " shoots " + target.name + " (" +
                                 std::to_string(result.damage) + " damage)" + bonusSuffix;
            if (callbacks_.onCharacterDowned)
            {
                callbacks_.onCharacterDowned(targetIndex);
            }
        }
        else
        {
            result.description = monster.name + " hits " + target.name + " with " +
                                 std::to_string(hits) + " shot" + (hits == 1 ? "" : "s") + " for " +
                                 std::to_string(result.damage) + " damage" + bonusSuffix;
        }
    }
    else
    {
        result.description = monster.name + " shoots at " + target.name + " and misses";
    }

    if (callbacks_.onMonsterAttack)
    {
        callbacks_.onMonsterAttack(monster, targetIndex, result);
    }

    monster.recoveryTime = std::max(500, def ? def->recovery * 100 : 1000);
    monster.aiState = MonsterInstance::AIState::Standing;
}

bool CombatSystem::tryMonsterSpellAttack(MonsterInstance& monster, int targetCharacter,
                                         float distance)
{
    if (!spellSystem_ || !gameWorld_ || distance > 5631.0f)
    {
        return false;
    }

    const auto it = monsterDefs_.find(monster.monsterId);
    if (it == monsterDefs_.end())
    {
        return false;
    }

    const formats::MonsterSpellAttack* choices[] = {&it->second.spellAttack1,
                                                    &it->second.spellAttack2};
    for (const auto* choice : choices)
    {
        if (choice->usePercent <= 0 || choice->spellMasterySkill.empty() ||
            choice->spellMasterySkill == "0")
        {
            continue;
        }
        if (randomInt(1, 100) > choice->usePercent)
        {
            continue;
        }

        std::string name;
        SkillMastery mastery = SkillMastery::Normal;
        int level = 1;
        if (!parseSpellSpec(choice->spellMasterySkill, name, mastery, level))
        {
            continue;
        }

        const auto spellId = spellSystem_->findSpellIdByName(name);
        if (!spellId)
        {
            logger_.warning("Unknown monster spell: " + name);
            continue;
        }

        AttackResult result;
        result.hit = true;
        if (isBeneficialSpellName(name))
        {
            const int heal = std::max(1, level * 2 + monster.level / 2);
            monster.currentHP = std::min(monster.maxHP, monster.currentHP + heal);
            result.damage = 0;
            result.description =
                monster.name + " casts " + name + " and recovers " + std::to_string(heal) + " HP";
        }
        else
        {
            const auto cast =
                spellSystem_->castMonsterSpell(*spellId, targetCharacter, level, mastery);
            result.damage = cast.damage;
            const std::string& targetName = gameWorld_->party().member(targetCharacter).name;
            result.description = monster.name + " casts " + name + " on " + targetName + " for " +
                                 std::to_string(cast.damage) + " damage";
        }

        if (callbacks_.onMonsterAttack)
        {
            callbacks_.onMonsterAttack(monster, targetCharacter, result);
        }
        return true;
    }

    return false;
}

void CombatSystem::applyMonsterBonus(MonsterInstance& monster, Character& target,
                                     std::string& description)
{
    if (!gameWorld_)
    {
        return;
    }
    const auto it = monsterDefs_.find(monster.monsterId);
    if (it == monsterDefs_.end())
    {
        return;
    }
    const std::string bonus = util::trim(it->second.bonus);
    if (bonus.empty() || bonus == "0")
    {
        return;
    }

    const std::string name = normalizedToken(bonus);
    int chance = std::clamp(20 + monster.level / 2, 10, 60);
    if (name == "dead" || name == "errad" || name == "stone")
    {
        chance = 5;
    }
    if (randomInt(1, 100) > chance)
    {
        return;
    }

    if (const auto condition = conditionForBonus(bonus))
    {
        target.setCondition(*condition, gameWorld_->calendar().totalTicks);
        description += " (" + bonus + ")";
        return;
    }

    if (name == "drainsp")
    {
        target.spellPoints = std::max(0, target.spellPoints - std::max(1, monster.level));
        description += " (spell points drained)";
    }
    else if (name == "agex2")
    {
        target.age += 2;
        description += " (aged)";
    }
    else if (name == "stealx2")
    {
        auto& party = gameWorld_->party();
        const int stolen = std::min(party.gold(), std::max(1, monster.level * 5));
        party.spendGold(stolen);
        description += " (" + std::to_string(stolen) + " gold stolen)";
    }
    else if (name == "brkarmor" || name == "brkweapon" || name == "brkitem")
    {
        description += " (equipment damaged)";
    }
}

void CombatSystem::killMonster(MonsterInstance& monster)
{
    if (monster.deathHandled)
    {
        return;
    }
    monster.deathHandled = true;
    monster.currentHP = 0;
    monster.aiState = MonsterInstance::AIState::Dead;

    distributeXP(monster.experience);
    explodeOnDeath(monster);
    dropTreasure(monster);

    if (callbacks_.onMonsterKilled)
    {
        callbacks_.onMonsterKilled(monster, monster.experience);
    }
}

void CombatSystem::explodeOnDeath(MonsterInstance& monster)
{
    if (!gameWorld_)
    {
        return;
    }
    const auto it = monsterDefs_.find(monster.monsterId);
    if (it == monsterDefs_.end())
    {
        return;
    }

    std::string dice;
    DamageElement element = DamageElement::Physical;
    if (!parseExplode(it->second.special, dice, element))
    {
        return;
    }
    const int baseDamage = rollDamage(dice);
    if (baseDamage <= 0)
    {
        return;
    }

    constexpr float kExplosionRadius = 512.0f;
    auto& party = gameWorld_->party();
    const float dx = party.worldX() - monster.x;
    const float dy = party.worldY() - monster.y;
    const float dz = party.worldZ() - monster.z;
    if (dx * dx + dy * dy + dz * dz > kExplosionRadius * kExplosionRadius)
    {
        return;
    }

    for (int i = 0; i < kPartySize; ++i)
    {
        auto& ch = party.member(i);
        if (!ch.isConscious())
        {
            continue;
        }
        const int damage = calculateMonsterDamage(baseDamage, element, i);
        ch.takeDamage(damage);

        AttackResult result;
        result.hit = true;
        result.damage = damage;
        result.description = monster.name + " explodes, hitting " + ch.name + " for " +
                             std::to_string(damage) + " damage";
        if (callbacks_.onMonsterAttack)
        {
            callbacks_.onMonsterAttack(monster, i, result);
        }
        if (ch.hitPoints <= 0 && callbacks_.onCharacterDowned)
        {
            callbacks_.onCharacterDowned(i);
        }
    }
}

void CombatSystem::dropTreasure(MonsterInstance& monster)
{
    if (!gameWorld_ || !inventory_)
    {
        return;
    }
    const auto it = monsterDefs_.find(monster.monsterId);
    if (it == monsterDefs_.end())
    {
        return;
    }

    const TreasureSpec spec = parseTreasure(it->second.treasure);
    if (spec.dice.empty() && spec.itemLevel <= 0)
    {
        return;
    }
    if (spec.chancePercent <= 0 || randomInt(1, 100) > spec.chancePercent)
    {
        return;
    }

    const int gold = spec.dice.empty() ? 0 : rollDamage(spec.dice);
    int itemId = 0;
    if (spec.itemLevel > 0)
    {
        const std::string type = util::toLower(spec.itemType);
        std::vector<const formats::ItemEntry*> candidates;
        for (const auto& item : inventory_->itemTable())
        {
            if (item.value <= 0)
            {
                continue;
            }
            if (!type.empty() && type != "misc")
            {
                const std::string group = util::toLower(item.skillGroup);
                const std::string stat = util::toLower(item.equipStat);
                const std::string name = util::toLower(item.name);
                if (group.find(type) == std::string::npos && stat.find(type) == std::string::npos &&
                    name.find(type) == std::string::npos)
                {
                    continue;
                }
            }
            candidates.push_back(&item);
        }

        if (!candidates.empty())
        {
            std::sort(candidates.begin(), candidates.end(),
                      [](const formats::ItemEntry* a, const formats::ItemEntry* b)
                      { return a->value < b->value; });
            const float fraction =
                std::clamp(static_cast<float>(spec.itemLevel) / 12.0f, 0.0f, 1.0f);
            size_t index =
                static_cast<size_t>(fraction * static_cast<float>(candidates.size() - 1));
            const int spread = std::max<int>(1, static_cast<int>(candidates.size()) / 10);
            const int jittered = static_cast<int>(index) + randomInt(-spread, spread);
            index = static_cast<size_t>(
                std::clamp(jittered, 0, static_cast<int>(candidates.size()) - 1));
            itemId = candidates[index]->id;
        }
    }

    if (gold <= 0 && itemId <= 0)
    {
        return;
    }

    const std::string& mapName = gameWorld_->currentMap();
    if (mapName.empty())
    {
        return;
    }

    std::vector<SpawnedMapItem> items;
    if (const auto* existing = gameWorld_->getSpawnedMapItems(mapName))
    {
        items = *existing;
    }
    const int64_t now = gameWorld_->calendar().totalTicks;
    if (gold > 0)
    {
        items.push_back({0, gold, monster.x + randomInt(-32, 32), monster.y + randomInt(-32, 32),
                         monster.z, now});
    }
    if (itemId > 0)
    {
        items.push_back({itemId, 1, monster.x + randomInt(-32, 32), monster.y + randomInt(-32, 32),
                         monster.z, now});
    }
    gameWorld_->setSpawnedMapItems(mapName, std::move(items));
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
    // Spell kills (resolved by SpellSystem) and melee kills share the same
    // XP + explosion + loot + death-callback path.
    killMonster(monster);
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

int CombatSystem::playerActionSpeed(int charIdx) const
{
    // RE uses Player_CalculateSpellDamage (fcn.0048e19b) as a generic player
    // action-speed computer, clamped to a minimum of 30 (docs/turn-based-
    // combat.md §4/§6). That function folds in weapon speed, mastery, and the
    // Speed stat; RuneHarbor doesn't yet model weapon/mastery speed, so this
    // approximates it from the Speed stat alone. Porting the full pipeline is
    // a follow-up.
    if (!gameWorld_ || charIdx < 0 || charIdx >= kPartySize)
        return kMinPlayerInitiative;
    // Stats index 5 is Speed (4 is Accuracy); haste buffs add to this value.
    const int speed = gameWorld_->party().member(charIdx).effectiveStat(5);
    return std::max(kMinPlayerInitiative, speed * 2);
}

int CombatSystem::monsterTypeRecovery(int monsterIdx) const
{
    // RE: per-monster-type TB recovery from the 0x5ccd10 table (stride 0x58),
    // accessed via the actor's type field. RuneHarbor stores this as the
    // MonsterEntry.recovery field; fall back to a mid-range default for
    // unknown ids so they still get a sane schedule slot.
    if (monsterIdx < 0 || monsterIdx >= static_cast<int>(monsters_.size()))
        return 30;
    const auto it = monsterDefs_.find(monsters_[static_cast<size_t>(monsterIdx)].monsterId);
    if (it == monsterDefs_.end())
        return 30;
    return std::max(1, it->second.recovery);
}

bool CombatSystem::turnActorLess(const TurnActor& a, const TurnActor& b)
{
    // RE QueueSort (fcn.00404544): ascending by initiative; on a tie, a monster
    // (type 3 in the binary) sorts before a player (type 4); otherwise by index.
    if (a.initiative != b.initiative)
        return a.initiative < b.initiative;
    if (a.type != b.type)
        return a.type == TurnActor::Type::Monster && b.type == TurnActor::Type::Player;
    return a.index < b.index;
}

void CombatSystem::pruneQueue()
{
    // Drop actors that can no longer act. The queue is the live schedule, so
    // dead/fled/unconscious entries are removed rather than skipped by index.
    auto canAct = [&](const TurnActor& actor) -> bool
    {
        if (actor.type == TurnActor::Type::Player)
        {
            return gameWorld_ && gameWorld_->party().member(actor.index).canAct();
        }
        if (actor.index >= 0 && actor.index < static_cast<int>(monsters_.size()))
        {
            const auto& m = monsters_[static_cast<size_t>(actor.index)];
            return m.isAlive() && m.hostile;
        }
        return false;
    };
    std::erase_if(turnQueue_, [&](const TurnActor& a) { return !canAct(a); });
}

void CombatSystem::startTurnBasedRound()
{
    // Builds a fresh continuous-initiative queue for the round. Unlike a flat
    // one-action-per-actor schedule, this queue persists across actions: after
    // each action advanceQueue() recomputes the actor's initiative and re-sorts,
    // so faster actors surface again within the same round (docs/turn-based-
    // combat.md §4 "continuous-initiative flavour").
    ++tbRound_;
    turnQueue_.clear();
    tbActedThisRound_.clear();
    awaitingPlayerInput_ = false;

    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();
    for (int i = 0; i < kPartySize; i++)
    {
        if (party.member(i).canAct())
        {
            TurnActor a;
            a.type = TurnActor::Type::Player;
            a.index = i;
            a.baseRecovery = playerActionSpeed(i);
            // TODO(characters-buffs): set a.hasted from an active haste timer
            // once the buff/condition system exposes one. RE: an active timer
            // doubles recovery (delays the actor) per RecomputeInit (§4).
            a.hasted = false;
            a.initiative = a.baseRecovery * (a.hasted ? 2 : 1) + randomInt(1, 10);
            turnQueue_.push_back(a);
        }
    }
    for (int i = 0; i < static_cast<int>(monsters_.size()); i++)
    {
        if (monsters_[static_cast<size_t>(i)].isAlive() &&
            monsters_[static_cast<size_t>(i)].hostile)
        {
            TurnActor a;
            a.type = TurnActor::Type::Monster;
            a.index = i;
            // Per-type recovery scaled by the TB multiplier (32/15 ≈ 2.1333).
            a.baseRecovery = std::max(1, static_cast<int>(monsterTypeRecovery(i) * kTbRecoveryMul));
            // TODO(monsters-buffs): haste flag, as above.
            a.hasted = false;
            a.initiative = a.baseRecovery * (a.hasted ? 2 : 1) + randomInt(1, 10);
            turnQueue_.push_back(a);
        }
    }
    std::sort(turnQueue_.begin(), turnQueue_.end(), turnActorLess);

    // Resolve any leading monster turns until a player is up.
    processMonsterTurn();
}

int CombatSystem::currentTurnPlayerIndex() const
{
    // The head of the sorted queue is the current actor.
    if (!turnBased_ || turnQueue_.empty())
        return -1;
    const auto& actor = turnQueue_.front();
    return actor.type == TurnActor::Type::Player ? actor.index : -1;
}

void CombatSystem::processMonsterTurn()
{
    // Head-driven loop: the front of the sorted queue is whoever acts now. Run
    // consecutive monster turns; stop at the first conscious player (await
    // input). Dead/fled entries that surface at the head are pruned.
    while (!turnQueue_.empty())
    {
        pruneQueue();
        if (turnQueue_.empty())
            break;

        TurnActor& actor = turnQueue_.front();
        if (actor.type == TurnActor::Type::Player)
        {
            if (gameWorld_ && gameWorld_->party().member(actor.index).canAct())
            {
                awaitingPlayerInput_ = true;
                return;
            }
            // Unconscious player: drop and continue.
            turnQueue_.erase(turnQueue_.begin());
            continue;
        }

        // Defeat guard: if no conscious party member remains, monsters have no
        // one left to fight — stop the turn loop (the RT path mirrors this in
        // updateMonsterAI). Without this, a fully-downed party would spin here
        // forever since no player will ever take the turn.
        if (gameWorld_ && gameWorld_->party().consciousCount() <= 0)
        {
            awaitingPlayerInput_ = false;
            return;
        }

        // Round bound: each monster acts at most once per round. Once every
        // alive hostile monster has acted, the round ends — this keeps the
        // "fast actor may act again" property across rounds while preventing an
        // infinite same-round loop when, e.g., every player is skipped.
        if (tbActedThisRound_.count(actor.index))
        {
            // All monsters have acted if the set covers every alive hostile one.
            bool allMonstersActed = true;
            for (int i = 0; i < static_cast<int>(monsters_.size()); ++i)
            {
                if (monsters_[static_cast<size_t>(i)].isAlive() &&
                    monsters_[static_cast<size_t>(i)].hostile && !tbActedThisRound_.count(i))
                {
                    allMonstersActed = false;
                    break;
                }
            }
            if (allMonstersActed)
            {
                // Round complete — start the next.
                startTurnBasedRound();
                return;
            }
            // Otherwise this monster already acted but others haven't; advance
            // past it so a waiting monster (or the next player) surfaces.
            advanceQueue();
            continue;
        }

        // Monster turn: one AI step + attack, then mark acted and advance.
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
        tbActedThisRound_.insert(actor.index);
        advanceQueue();
    }
    // Queue exhausted (all actors dead/fled/unconscious) — start a new round.
    startTurnBasedRound();
}

void CombatSystem::advanceQueue()
{
    // RE QueueAdvance (fcn.00406457): the head just acted. Recompute its
    // initiative from base recovery, run a countdown pass that subtracts the
    // elapsed initiative from every entry (so the next-lowest becomes ready),
    // then re-sort. Pruning happens in processMonsterTurn on the next pass.
    awaitingPlayerInput_ = false;
    if (turnQueue_.empty())
    {
        startTurnBasedRound();
        return;
    }

    TurnActor& head = turnQueue_.front();
    const int elapsed = head.initiative;
    head.initiative = head.baseRecovery * (head.hasted ? 2 : 1) + randomInt(1, 10);

    // Countdown: every other entry's remaining initiative drops by `elapsed`,
    // clamped at 0 (an actor at 0 initiative is ready now). This is what lets a
    // fast actor whose initiative fell below others' act again this round.
    for (size_t i = 1; i < turnQueue_.size(); ++i)
    {
        turnQueue_[i].initiative = std::max(0, turnQueue_[i].initiative - elapsed);
    }
    std::sort(turnQueue_.begin(), turnQueue_.end(), turnActorLess);
}

void CombatSystem::completePlayerTurn()
{
    if (!turnBased_)
        return;
    // The player's action is resolved by the caller (playerAttack/spell/pass);
    // here we just advance the queue, which runs subsequent monster turns.
    advanceQueue();
    processMonsterTurn();
}

std::string CombatSystem::turnStatusText() const
{
    if (!turnBased_)
        return {};
    if (turnQueue_.empty())
        return std::format("Round {} — starting...", tbRound_);
    const auto& actor = turnQueue_.front();
    if (actor.type == TurnActor::Type::Player && gameWorld_)
    {
        const auto& ch = gameWorld_->party().member(actor.index);
        return std::format("Round {} — {}'s turn", tbRound_, ch.name);
    }
    return std::format("Round {} — Monster turn", tbRound_);
}

} // namespace runeharbor::game
