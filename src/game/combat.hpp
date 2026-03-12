// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../formats/monsters_parser.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::game
{

class GameWorld;

// Live monster instance in combat
struct MonsterInstance
{
    int monsterId = 0;    // Reference to MonsterEntry
    std::string name;     // Display name
    int currentHP = 0;    // Current hit points
    int maxHP = 0;        // Max hit points
    int armorClass = 0;   // AC
    int level = 0;        // Level for XP calc
    int experience = 0;   // XP reward on kill
    int speed = 0;        // Movement/action speed
    int recoveryTime = 0; // Current recovery countdown (ms)
    int group = 0;        // Map/script group id (for event opcodes)
    uint16_t topic = 0;   // Script dialog topic id

    // Position in world
    float x = 0, y = 0, z = 0;

    // AI state (matches MM7 binary values)
    enum class AIState : uint8_t
    {
        Standing = 0,
        Wandering = 1,
        Guarding = 2,
        Fidgeting = 3,
        Fleeing = 4,
        Dead = 5,
        Pursuing = 6,
        Attacking = 7,
        AttackingRanged = 8,
        AttackingMelee2 = 9,
        Stunned = 11,
        CastingSpell1 = 12,
        CastingSpell2 = 13,
        Paralyzed = 17,
        CastingSpell3 = 18,
        Stoned = 19,
    };
    AIState aiState = AIState::Standing;

    enum class Personality : uint8_t
    {
        Normal = 0,
        Wimp = 1,
        Aggressive = 2,
        Suicidal = 3,
        Friendly = 4,
    };
    Personality personality = Personality::Normal;

    int targetCharacter = -1; // Which party member to attack (-1 = none)
    float aggroRange = 2048.0f;
    bool hostile = true;

    // Resistances (copied from MonsterEntry)
    int resistFire = 0;
    int resistAir = 0;
    int resistWater = 0;
    int resistEarth = 0;
    int resistMind = 0;
    int resistSpirit = 0;
    int resistBody = 0;
    int resistLight = 0;
    int resistDark = 0;
    int resistPhysical = 0;
    std::array<int, 8> scriptFields = {};

    bool isAlive() const { return aiState != AIState::Dead && currentHP > 0; }
};

// Result of an attack roll
struct AttackResult
{
    bool hit = false;
    int damage = 0;
    bool critical = false;
    std::string description;
};

// Damage element types (matches MM7 binary layout: Fire=0 through Physical=10)
enum class DamageElement : uint8_t
{
    Fire = 0,
    Air = 1,
    Water = 2,
    Earth = 3,
    Spirit = 4,
    Unused = 5,
    Body = 6,
    Mind = 7,
    Light = 8,
    Dark = 9,
    Physical = 10,
};

// Combat event callbacks for UI
struct CombatCallbacks
{
    std::function<void(int characterIndex, const MonsterInstance& target,
                       const AttackResult& result)>
        onCharacterAttack;
    std::function<void(const MonsterInstance& attacker, int characterIndex,
                       const AttackResult& result)>
        onMonsterAttack;
    std::function<void(const MonsterInstance& monster, int xpReward)> onMonsterKilled;
    std::function<void(int characterIndex)> onCharacterDowned;
};

class CombatSystem
{
  public:
    explicit CombatSystem(util::ILogger& logger);

    // Set references
    void setGameWorld(GameWorld* world) { gameWorld_ = world; }
    void setCallbacks(const CombatCallbacks& callbacks) { callbacks_ = callbacks; }

    // Load monster definitions
    void loadMonsterData(const std::vector<formats::MonsterEntry>& monsters);

    // Spawn a monster into the active combat arena
    int spawnMonster(int monsterId, float x, float y, float z, int group = 0);

    // Remove all monsters
    void clearMonsters();

    // Access active monsters
    const std::vector<MonsterInstance>& getMonsters() const { return monsters_; }
    MonsterInstance* getMonster(int instanceIndex);
    
    // Get monster definition by original ID (e.g. objectType from spawn point)
    const formats::MonsterEntry* getMonsterDef(int monsterId) const;

    // Update combat (called each frame with delta time in ms)
    void update(float deltaMs);

    // Player initiates attack on monster
    AttackResult playerAttack(int characterIndex, int monsterIndex);

    // Damage calculation
    static int rollDamage(const std::string& diceExpr);
    int calculateDamage(int baseDamage, DamageElement type, const MonsterInstance& target) const;
    int calculateMonsterDamage(int baseDamage, DamageElement type, int characterIndex) const;

    // Hit chance
    int hitChance(int attackerLevel, int attackerAccuracy, int defenderAC) const;
    bool rollHit(int chance) const;

    // Is combat active?
    bool inCombat() const { return inCombat_; }
    void setInCombat(bool active) { inCombat_ = active; }

    // Count alive monsters
    int aliveMonsterCount() const;

    // Event-driven runtime mutations.
    void setPartyHostilityByMonsterId(std::unordered_map<int, bool> hostilityByMonsterId);
    void setMonsterTopic(int monsterIndex, uint16_t topic);
    void setMonsterField(int monsterId, int fieldIndex, int value);
    void setMonsterHostileByGroup(int group, bool hostile);
    void setMonsterHostileByIndex(int index, bool hostile);
    void replaceMonsterType(int oldMonsterId, int newMonsterId);
    void setMonsterAiByType(int monsterId, MonsterInstance::AIState state);
    void setMonsterAiByGroup(int group, MonsterInstance::AIState state);

  private:
    void updateMonsterAI(MonsterInstance& monster, float deltaMs);
    void monsterAttack(MonsterInstance& monster);
    void distributeXP(int xp);

    util::ILogger& logger_;
    GameWorld* gameWorld_ = nullptr;
    CombatCallbacks callbacks_;

    std::unordered_map<int, formats::MonsterEntry> monsterDefs_;
    std::unordered_set<int> warnedUnknownMonsterIds_;
    std::unordered_map<int, bool> partyHostilityByMonsterId_;
    std::vector<MonsterInstance> monsters_;
    bool inCombat_ = false;
};

} // namespace runeharbor::game
