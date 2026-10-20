// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "character.hpp"

namespace runeharbor::game
{

// Party alignment (affects quest availability and NPC reactions)
enum class Alignment : uint8_t
{
    Good = 0,
    Neutral,
    Evil,
};

static constexpr int kPartySize = 4;
// MM7 allows two hired NPCs alongside the four player characters.
static constexpr int kMaxHiredNpcs = 2;

// A hireling in the party roster. Names/profession text are stored so the save
// remains readable even if the NPC data tables change.
struct HiredNpc
{
    int npcId = 0;
    int professionId = 0;
    std::string name;
    std::string professionName;
    int hireCost = 0;
};

class Party
{
  public:
    Party();

    // Character access
    Character& member(int index);
    const Character& member(int index) const;
    Character& operator[](int index) { return member(index); }
    const Character& operator[](int index) const { return member(index); }

    // Party resources
    int gold() const { return gold_; }
    void setGold(int amount) { gold_ = amount; }
    void addGold(int amount) { gold_ += amount; }
    bool spendGold(int amount);

    // Bank: gold stored separately (safe from loss on death).
    int bankGold() const { return bankGold_; }
    void setBankGold(int amount) { bankGold_ = amount; }
    void depositGold(int amount);
    void withdrawGold(int amount);

    int food() const { return food_; }
    void setFood(int amount) { food_ = amount; }
    void addFood(int amount) { food_ += amount; }
    bool consumeFood(int amount);

    // Alignment & reputation
    Alignment alignment() const { return alignment_; }
    void setAlignment(Alignment a) { alignment_ = a; }
    int reputation() const { return reputation_; }
    void adjustReputation(int delta) { reputation_ += delta; }

    // Party-level queries
    bool isPartyAlive() const;
    int aliveCount() const;
    int consciousCount() const;

    // Active player slot (used by event system mode 4). -1 means "no active member".
    int activeMemberIndex() const { return activeMemberIndex_; }
    void setActiveMemberIndex(int index);

    // Position in world
    float worldX() const { return worldX_; }
    float worldY() const { return worldY_; }
    float worldZ() const { return worldZ_; }
    float velocityX() const { return velocityX_; }
    void setVelocityX(float vx) { velocityX_ = vx; }
    float velocityY() const { return velocityY_; }
    void setVelocityY(float vy) { velocityY_ = vy; }
    float velocityZ() const { return velocityZ_; }
    void setVelocityZ(float vz) { velocityZ_ = vz; }
    void setWorldPosition(float x, float y, float z);

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    void setOrientation(float yaw, float pitch);

    // Legacy saved clock field. GameWorld::calendar() is authoritative for
    // gameplay (events, spells, day/night, rest, travel); this value is kept
    // only so older save payloads still round-trip.
    uint64_t gameTime() const { return gameTime_; }
    void advanceTime(uint64_t ticks) { gameTime_ += ticks; }
    void setGameTime(uint64_t ticks) { gameTime_ = ticks; }

    // Current map
    const std::string& currentMap() const { return currentMap_; }
    void setCurrentMap(const std::string& name) { currentMap_ = name; }

    // Initialize a default party (4 characters with MM7 defaults)
    void initDefault();

    // Recalculate all derived stats for all members
    void recalculateAll();

    // Hired NPCs (NPC dialogue "Hire" / "Dismiss").
    const std::vector<HiredNpc>& hiredNpcs() const { return hiredNpcs_; }
    bool hireNpc(HiredNpc npc);
    bool dismissNpc(int npcId);
    bool isNpcHired(int npcId) const;
    void clearHiredNpcs() { hiredNpcs_.clear(); }

    // Core Gameplay actions
    void awardExperience(int amount);
    bool rest(int hours);

    // Random encounter check (called after rest or while traveling). Returns
    // true if an ambush should occur (~15% chance).
    bool checkRandomEncounter();

  private:
    std::array<Character, kPartySize> members_;
    int gold_ = 200;
    int bankGold_ = 0;
    int food_ = 7;
    Alignment alignment_ = Alignment::Neutral;
    int reputation_ = 0;

    float worldX_ = 0.0f;
    float worldY_ = 0.0f;
    float worldZ_ = 0.0f;
    float velocityX_ = 0.0f;
    float velocityY_ = 0.0f;
    float velocityZ_ = 0.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    uint64_t gameTime_ = 0;
    std::string currentMap_;
    std::vector<HiredNpc> hiredNpcs_;
    int activeMemberIndex_ = 0;
};

} // namespace runeharbor::game
