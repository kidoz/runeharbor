// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../formats/spells_parser.hpp"
#include "../util/ilogger.hpp"
#include "character.hpp"

namespace runeharbor::game
{

class GameWorld;
struct MonsterInstance;

// MM7 spell schools (matches binary values, gap at 4-5)
enum class SpellSchool : uint8_t
{
    Fire = 0,
    Air = 1,
    Water = 2,
    Earth = 3,
    Default = 4, // unused/special
    Magic = 5,   // unused/special
    Spirit = 6,
    Mind = 7,
    Body = 8,
    Light = 9,
    Dark = 10,
    Count = 11
};

// Maps spell school to the corresponding SkillId
inline SkillId schoolToSkill(SpellSchool school)
{
    // Schools 0-3 map directly to SkillId::Fire+offset
    // Schools 6-10 map to Spirit/Mind/Body/Light/Dark skills
    int s = static_cast<int>(school);
    if (s <= 3)
        return static_cast<SkillId>(static_cast<int>(SkillId::Fire) + s);
    if (s >= 6 && s <= 10)
        return static_cast<SkillId>(static_cast<int>(SkillId::Spirit) + (s - 6));
    return SkillId::Fire; // fallback for Default/Magic
}

// Spell target type
enum class SpellTarget : uint8_t
{
    Self = 0,        // Caster only
    SingleAlly,      // One party member
    AllAllies,       // Entire party
    SingleEnemy,     // One monster
    AllEnemies,      // All visible monsters
    AreaOfEffect,    // Position-based area
    SingleAnyTarget, // Any single target
};

// Runtime spell info (enriched from SpellEntry)
struct SpellInfo
{
    int spellId = 0;
    std::string name;
    std::string shortName;
    SpellSchool school = SpellSchool::Fire;
    int level = 0; // Required skill level (1-12)
    SpellTarget target = SpellTarget::SingleEnemy;

    // Mana cost per mastery level
    int manaCostNormal = 0;
    int manaCostExpert = 0;
    int manaCostMaster = 0;
    int manaCostGM = 0;

    // Effect descriptions
    std::string normalEffect;
    std::string expertEffect;
    std::string masterEffect;
    std::string grandMasterEffect;

    // Which resistance applies
    std::string resistance;
};

// Result of casting a spell
struct SpellResult
{
    bool success = false;
    bool resisted = false;
    int damage = 0;
    int healing = 0;
    std::string description;
};

// Callbacks for spell visual/audio effects
struct SpellCallbacks
{
    std::function<void(int spellId, int casterIndex, const SpellResult& result)> onSpellCast;
    std::function<void(int spellId, const std::string& reason)> onSpellFailed;
    // Fired when a damage spell kills a monster, so the host (CombatSystem) can
    // award XP and run the same death pipeline as a melee kill. Without this,
    // spell kills would set the monster Dead but grant no XP and skip the
    // onMonsterKilled UI callback.
    std::function<void(MonsterInstance& monster, int xp)> onMonsterKilled;
};

class SpellSystem
{
  public:
    explicit SpellSystem(util::ILogger& logger);

    void setGameWorld(GameWorld* world) { gameWorld_ = world; }
    void setCallbacks(const SpellCallbacks& callbacks) { callbacks_ = callbacks; }

    // Load spell definitions
    void loadSpellData(const std::vector<formats::SpellEntry>& spells);

    // Spell info lookup
    const SpellInfo* getSpell(int spellId) const;
    const std::vector<SpellInfo>& getAllSpells() const { return spells_; }

    // Check if a character can cast a spell
    bool canCast(int characterIndex, int spellId) const;

    // Get the mana cost for a character casting a spell (based on mastery)
    int getManaCost(int characterIndex, int spellId) const;

    // Get the effective mastery level for a character's spell school
    SkillMastery getEffectiveMastery(int characterIndex, SpellSchool school) const;
    int getEffectiveSkillLevel(int characterIndex, SpellSchool school) const;

    // Cast a damage spell on a monster
    SpellResult castDamageSpell(int characterIndex, int spellId, MonsterInstance* target);

    // Cast a healing spell on a party member
    SpellResult castHealSpell(int characterIndex, int spellId, int targetCharIndex);

    // Cast a buff spell on self or party
    SpellResult castBuffSpell(int characterIndex, int spellId);

    // Event/script spell cast path (opcode-driven): applies effect directly to party targets
    // without requiring caster skill/mana checks.
    SpellResult castScriptSpell(int spellId, int power, int targetCharIndex);

    // Get spells available to a character (based on learned skills)
    std::vector<int> getAvailableSpells(int characterIndex) const;

  private:
    int calculateSpellDamage(int spellId, int casterLevel, SkillMastery mastery) const;
    int calculateHealing(int spellId, int casterLevel, SkillMastery mastery) const;
    SpellSchool determineSchool(int spellId) const;

    util::ILogger& logger_;
    GameWorld* gameWorld_ = nullptr;
    SpellCallbacks callbacks_;

    std::vector<SpellInfo> spells_;
    std::unordered_map<int, size_t> spellIndex_; // spellId -> index in spells_
};

} // namespace runeharbor::game
