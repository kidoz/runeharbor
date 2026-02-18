// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
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

    // Position in world
    float x = 0, y = 0, z = 0;

    // AI state
    enum class AIState : uint8_t
    {
        Idle = 0,
        Pursuing,
        Attacking,
        Fleeing,
        Dead
    };
    AIState aiState = AIState::Idle;

    int targetCharacter = -1; // Which party member to attack (-1 = none)
    float aggroRange = 2048.0f;

    // Resistances (copied from MonsterEntry)
    int resistFire = 0;
    int resistAir = 0;
    int resistWater = 0;
    int resistEarth = 0;
    int resistMind = 0;
    int resistSpirit = 0;
    int resistBody = 0;
    int resistPhysical = 0;

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

// Damage types
enum class DamageType : uint8_t
{
    Physical = 0,
    Fire,
    Air,
    Water,
    Earth,
    Spirit,
    Mind,
    Body,
    Light,
    Dark,
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
    int spawnMonster(int monsterId, float x, float y, float z);

    // Remove all monsters
    void clearMonsters();

    // Access active monsters
    const std::vector<MonsterInstance>& getMonsters() const { return monsters_; }
    MonsterInstance* getMonster(int instanceIndex);

    // Update combat (called each frame with delta time in ms)
    void update(float deltaMs);

    // Player initiates attack on monster
    AttackResult playerAttack(int characterIndex, int monsterIndex);

    // Damage calculation
    static int rollDamage(const std::string& diceExpr);
    int calculateDamage(int baseDamage, DamageType type, const MonsterInstance& target) const;
    int calculateMonsterDamage(int baseDamage, DamageType type, int characterIndex) const;

    // Hit chance
    int hitChance(int attackerLevel, int attackerAccuracy, int defenderAC) const;
    bool rollHit(int chance) const;

    // Is combat active?
    bool inCombat() const { return inCombat_; }
    void setInCombat(bool active) { inCombat_ = active; }

    // Count alive monsters
    int aliveMonsterCount() const;

  private:
    void updateMonsterAI(MonsterInstance& monster, float deltaMs);
    void monsterAttack(MonsterInstance& monster);
    void distributeXP(int xp);

    util::ILogger& logger_;
    GameWorld* gameWorld_ = nullptr;
    CombatCallbacks callbacks_;

    std::unordered_map<int, formats::MonsterEntry> monsterDefs_;
    std::vector<MonsterInstance> monsters_;
    bool inCombat_ = false;
};

} // namespace runeharbor::game
