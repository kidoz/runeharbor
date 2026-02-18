// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

// Helper struct for attack definitions
struct MonsterAttack
{
    std::string type;
    std::string damage; // e.g., "2D8+10"
    std::string miss;
    int attPercent = 0; // for physical attacks
};

// Helper struct for spell attack definitions
struct MonsterSpellAttack
{
    int usePercent = 0;
    std::string spellMasterySkill; // e.g., "Light Bolt,M,8"
};

struct MonsterEntry
{
    int id = 0;
    std::string name;
    std::string picture;
    int level = 0;
    int hitPoints = 0;
    int armorClass = 0;
    int experience = 0;
    std::string treasure;
    int quest = 0; // 0 or 1
    bool canFly = false;
    std::string moveType; // e.g., "Free", "Short", "Med", "Long", "stand"
    std::string aiType;   // e.g., "Aggress", "Normal", "Suicidal", "Wimp"
    int haste = 0;
    int speed = 0;
    int recovery = 0;
    std::string preferences; // e.g., "AR", "CP", "S", "DR"
    std::string bonus; // e.g., "Disease1", "BrkArmor", "Afraid", "Insane", "DrainSP", "Uncon",
                       // "Agex2", "Cursex2", "Dead", "Errad"

    MonsterAttack attack1;
    MonsterAttack attack2; // Second physical attack might be empty

    MonsterSpellAttack spellAttack1;
    MonsterSpellAttack spellAttack2; // Second spell attack might be empty

    // Resistances (percentage values)
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

    std::string special; // e.g., "shot,x3", "explode,5D8,light", "Summon,air,Dragon C"
};

class MonstersParser
{
  public:
    explicit MonstersParser(util::ILogger& logger);
    bool parse(const std::vector<uint8_t>& data);
    const std::vector<MonsterEntry>& getMonsters() const { return monsters; }

  private:
    util::ILogger& logger;
    std::vector<MonsterEntry> monsters;
    // Will use runeharbor::util::splitString from string_utils.hpp
};

} // namespace runeharbor::formats
