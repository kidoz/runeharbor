// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../util/ilogger.hpp"

namespace runeharbor::game
{

class GameWorld;

/// Save file header
struct SaveHeader
{
    static constexpr uint32_t kMagic = 0x52484256; // "RHBV" (RuneHarBor saVe)
    static constexpr uint32_t kVersion = 1;

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
    bool save(const GameWorld& world, int slotIndex);

    /// Load game state from a slot into the given world
    bool load(GameWorld& world, int slotIndex);

    /// Get info about all save slots (for UI display)
    std::vector<SaveSlotInfo> listSlots() const;

    /// Check if a slot has a save file
    bool slotExists(int slotIndex) const;

    /// Delete a save slot
    bool deleteSlot(int slotIndex);

    /// Set the save directory (default: "saves/")
    void setSaveDirectory(const std::string& dir) { saveDir_ = dir; }

    /// Max number of save slots
    static constexpr int kMaxSlots = 10;

  private:
    std::string slotPath(int slotIndex) const;

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

    util::ILogger& logger_;
    std::string saveDir_ = "saves";
};

} // namespace runeharbor::game
