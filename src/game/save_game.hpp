// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::game
{

class GameWorld;
class Inventory;
class QuestLog;

/// Save file header
struct SaveHeader
{
    static constexpr uint32_t kMagic = 0x52484256; // "RHBV" (RuneHarBor saVe)
    // Version history:
    //   1  — core (party, characters, map, vars)
    //   2  — activeMember, RuntimeConfig, visited/generated maps
    //   3  — generated-content respawn fields
    //   4  — sky colors
    //   5  — saved per-map states
    //   6  — map transitions
    //   7  — runtime gammas + shade distances
    //   8  — spawned map items
    //   9  — transition arrival override flag
    //  10  — indoor decoration-hidden flags
    //  11  — party height / eye level
    //  12  — per-character spellbook (knownSpells) + quickbar, party bankGold
    static constexpr uint32_t kVersion = 12;

    uint32_t magic = kMagic;
    uint32_t version = kVersion;
    uint64_t timestamp = 0;    // Real-world save time (Unix epoch seconds)
    uint64_t gameTime = 0;     // In-game time (calendar ticks, 128/sec)
    char mapName[64] = {};     // Current map name
    char partyLeader[32] = {}; // First character's name (for display)
    int partyLevel = 0;        // Average party level (for display)
};

/// Save slot info (for save/load screen)
struct SaveSlotInfo
{
    int slotIndex = -1;
    bool exists = false;
    SaveHeader header;
    std::string filePath;
};

/// Save/Load system: serializes and deserializes game state.
class SaveGame
{
  public:
    explicit SaveGame(util::ILogger& logger);

    /// Save current game state to a slot (0-based index)
    bool save(const GameWorld& world, int slotIndex,
              const std::vector<uint8_t>* eventRuntimeState = nullptr,
              const Inventory* inventory = nullptr, const QuestLog* questLog = nullptr);

    /// Load game state from a slot into the given world
    bool load(GameWorld& world, int slotIndex, std::vector<uint8_t>* eventRuntimeState = nullptr,
              Inventory* inventory = nullptr, QuestLog* questLog = nullptr);

    /// Load game state from autosave.mm7
    bool loadAutosave(GameWorld& world, std::vector<uint8_t>* eventRuntimeState = nullptr,
                      Inventory* inventory = nullptr, QuestLog* questLog = nullptr);

    /// Get info about all save slots (for UI display)
    std::vector<SaveSlotInfo> listSlots() const;

    /// Read autosave header metadata when available.
    std::optional<SaveHeader> autosaveHeader() const;

    /// Check if a slot has a save file
    bool slotExists(int slotIndex) const;

    /// Check if autosave.mm7 exists
    bool autosaveExists() const;

    /// Delete a save slot
    bool deleteSlot(int slotIndex);

    /// Set the save directory (default: "saves/")
    void setSaveDirectory(const std::string& dir) { saveDir_ = dir; }

    /// Max number of save slots
    static constexpr int kMaxSlots = 40;

  private:
    std::string slotPath(int slotIndex) const;
    std::string legacySlotPath(int slotIndex) const;
    std::string autosavePath() const;
    std::string resolveExistingSlotPath(int slotIndex) const;

    // Serialization helpers
    void writeU8(std::vector<uint8_t>& buf, uint8_t val);
    void writeU16(std::vector<uint8_t>& buf, uint16_t val);
    void writeU32(std::vector<uint8_t>& buf, uint32_t val);
    void writeU64(std::vector<uint8_t>& buf, uint64_t val);
    void writeI32(std::vector<uint8_t>& buf, int32_t val);
    void writeFloat(std::vector<uint8_t>& buf, float val);
    void writeString(std::vector<uint8_t>& buf, const std::string& str);

    bool readU8(const uint8_t*& ptr, const uint8_t* end, uint8_t& val);
    bool readU16(const uint8_t*& ptr, const uint8_t* end, uint16_t& val);
    bool readU32(const uint8_t*& ptr, const uint8_t* end, uint32_t& val);
    bool readU64(const uint8_t*& ptr, const uint8_t* end, uint64_t& val);
    bool readI32(const uint8_t*& ptr, const uint8_t* end, int32_t& val);
    bool readFloat(const uint8_t*& ptr, const uint8_t* end, float& val);
    bool readString(const uint8_t*& ptr, const uint8_t* end, std::string& str);

    bool serializeWorld(const GameWorld& world, std::vector<uint8_t>& out);
    bool deserializeWorld(GameWorld& world, const std::vector<uint8_t>& data);

    // Quest-log companion blob ("RHQL"): each quest's runtime state + journal
    // entries. Stored separately (like inventory.bin) because GameWorld does not
    // own the QuestLog.
    std::vector<uint8_t> serializeQuestLog(const QuestLog& questLog);
    bool deserializeQuestLog(const std::vector<uint8_t>& data, QuestLog& questLog);

    util::ILogger& logger_;
    std::string saveDir_ = "saves";
};

} // namespace runeharbor::game
