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

// AI hostility levels index into this aggression range table (RE: 0x4DF380)
// Distance at which a monster becomes aggressive toward the party
inline constexpr std::array<int, 5> kAggroRangeTable = {
    0,     // 0 = Friendly (no aggression)
    1024,  // 1 = Low hostility
    2560,  // 2 = Medium hostility
    5120,  // 3 = High hostility
    10240, // 4 = Maximum (always hostile)
};

// Hostility matrix dimensions (RE: 89×89 at 0x5C8B40, loaded from hostile.txt)
inline constexpr int kHostilityMatrixSize = 89;

// Max visible actors for AI targeting (RE: bubble-sorted list at 0x4F7458)
inline constexpr int kMaxVisibleActors = 30;

// Target ID encoding: (actorIndex << 3) | type  (RE: confirmed in decompiled code)
enum class TargetType : uint8_t
{
    None = 0,
    Actor = 3,
    Party = 4,
};

inline uint32_t encodeTargetId(int index, TargetType type)
{
    return static_cast<uint32_t>((index << 3) | static_cast<int>(type));
}

inline int decodeTargetIndex(uint32_t targetId)
{
    return static_cast<int>(targetId >> 3);
}

inline TargetType decodeTargetType(uint32_t targetId)
{
    return static_cast<TargetType>(targetId & 0x7);
}

// Live monster instance in combat
// RE: original Actor struct is 836 bytes (stride 0x344) at 0x5FEFFC
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

    // Position in world (RE: +0x66/+0x68/+0x6A as i16, stored as float for engine use)
    float x = 0, y = 0, z = 0;

    // Velocity for physics (RE: +0x6C, i16[3])
    float velocityX = 0, velocityY = 0, velocityZ = 0;

    // Facing angle (RE: +0x72, u16 0-2047, 0=East, 512=North)
    uint16_t facingAngle = 0;

    // Indoor sector tracking (RE: +0x7A, u16 sectorId for BLV)
    uint16_t sectorId = 0;

    // Sprite type (RE: +0x08, u16)
    uint16_t spriteType = 0;

    // AI state (matches MM7 binary values, RE: +0x88)
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

    // AI timer (RE: +0x90, time in current state)
    int aiTimer = 0;

    enum class Personality : uint8_t
    {
        Normal = 0,
        Wimp = 1,
        Aggressive = 2,
        Suicidal = 3,
        Friendly = 4,
    };
    Personality personality = Personality::Normal;

    // Hostility level 0-4, indexes into kAggroRangeTable (RE: +0x15)
    uint8_t hostilityLevel = 0;

    // Alliance/faction group for hostility matrix lookup (RE: +0x60)
    uint16_t allianceGroup = 0;

    // Team membership for group checks (RE: +0x2C4)
    int teamId = 0;

    // Target ID using encoded format (RE: +0x29C, bits 0-2=type, 3+=index)
    uint32_t targetId = 0;

    // Hostility override; 9999 = friendly (RE: +0x2C8)
    int hostilityOverride = 0;

    // Condition timestamps (RE: i64 timestamps, 0 = not active)
    int64_t charmTime = 0;    // RE: +0xE4
    int64_t deadTime = 0;     // RE: +0x164
    int64_t paralyzeTime = 0; // RE: +0x194

    // Flags (RE: +0x00 and +0x24)
    uint32_t flags = 0;         // Bit 15=hostile, 19=always-hostile
    uint32_t flagsExtended = 0; // Bit 19=permanently hostile (0x80000)

    // Legacy fields (kept for backward compat with existing combat code)
    int targetCharacter = -1;
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

    bool isPermanentlyHostile() const { return (flagsExtended & 0x80000) != 0; }

    bool isFriendlyOverride() const { return hostilityOverride == 9999; }

    int effectiveAggroRange() const
    {
        if (isFriendlyOverride())
            return 0;
        if (hostilityLevel < kAggroRangeTable.size())
            return kAggroRangeTable[hostilityLevel];
        return kAggroRangeTable.back();
    }
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

class Inventory;

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
    void setInventory(Inventory* inventory) { inventory_ = inventory; }
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
    void setInCombat(bool active)
    {
        // When combat begins (or ends) while turn-based mode is on, keep the
        // turn queue in sync: start a round on entry, tear it down on exit.
        // Without this, an ambush or hostility trigger that fires while TB is
        // already on would leave the queue empty and no turn would ever run.
        if (active && !inCombat_ && turnBased_)
        {
            inCombat_ = true;
            startTurnBasedRound();
        }
        else if (!active && turnBased_)
        {
            inCombat_ = false;
            turnQueue_.clear();
            awaitingPlayerInput_ = false;
        }
        else
        {
            inCombat_ = active;
        }
    }

    // Award XP for a monster killed by an external system (e.g. a spell kill
    // resolved by SpellSystem): distributes experience across conscious party
    // members and fires the onMonsterKilled UI callback, exactly as a melee
    // kill in playerAttack does. Public so SpellSystem's onMonsterKilled spell
    // callback can route through it without needing access to distributeXP.
    void awardMonsterKill(MonsterInstance& monster);

    // -------- Turn-based combat (RE: docs/turn-based-combat.md) --------
    bool isTurnBased() const { return turnBased_; }
    void setTurnBased(bool tb);
    bool awaitingPlayerInput() const { return turnBased_ && awaitingPlayerInput_; }
    int currentRound() const { return tbRound_; }
    // The character index whose turn it is (−1 if a monster's turn or RT mode).
    int currentTurnPlayerIndex() const;
    // Build the initiative queue and start round 1 (call on entering TB or
    // when combat starts while in TB).
    void startTurnBasedRound();
    // Called by InGameState after a player acts (attack/spell/pass). Advances
    // the queue, processes any consecutive monster turns, then either sets
    // awaitingPlayerInput_ for the next player or starts a new round.
    void completePlayerTurn();
    // Status text for the HUD ("Round 3 — Sir Knight's turn").
    std::string turnStatusText() const;

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
    Inventory* inventory_ = nullptr;
    CombatCallbacks callbacks_;

    std::unordered_map<int, formats::MonsterEntry> monsterDefs_;
    std::unordered_set<int> warnedUnknownMonsterIds_;
    std::unordered_map<int, bool> partyHostilityByMonsterId_;
    std::vector<MonsterInstance> monsters_;
    bool inCombat_ = false;

    // Turn-based state.
    struct TurnActor
    {
        enum class Type : uint8_t
        {
            Player,
            Monster
        };
        Type type = Type::Player;
        int index = 0;      // party member index (0–3) or monsters_ index
        int initiative = 0; // lower = acts first
    };
    bool turnBased_ = false;
    bool awaitingPlayerInput_ = false;
    int tbRound_ = 0;
    std::vector<TurnActor> turnQueue_;
    size_t tbQueueIdx_ = 0;

    void processMonsterTurn();
    void advanceQueue();
};

} // namespace runeharbor::game
