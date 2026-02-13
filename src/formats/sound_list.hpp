// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

/**
 * Sound Event definition from dsounds.bin
 */
#pragma pack(push, 1)
struct SoundInfoRaw
{
    char name[32];           // Event name (e.g. "campfire")
    uint32_t soundId;        // Unique ID used in game logic
    uint32_t type;           // Sound type
    uint32_t flags;          // Sound flags (e.g. 3D)
    uint32_t dataId[17];     // Related data IDs
    uint32_t p3dSoundId;     // 3D sound specific ID (MM7+)
    uint32_t isDecompressed; // Compression flag (MM7+)
};
#pragma pack(pop)

struct SoundEvent
{
    std::string name;
    uint32_t soundId = 0;
    uint32_t type = 0;
    uint32_t flags = 0;
    std::array<uint32_t, 17> dataIds{};
    uint32_t p3dSoundId = 0;
    bool isDecompressed = false;
};

class SoundList
{
  public:
    explicit SoundList(util::ILogger& logger);

    bool parse(const std::vector<uint8_t>& data);
    
    const SoundEvent* getSound(uint32_t soundId) const;
    const SoundEvent* getSoundByName(const std::string& name) const;
    
    const std::unordered_map<uint32_t, SoundEvent>& getAllSounds() const { return sounds; }

  private:
    util::ILogger& logger;
    std::unordered_map<uint32_t, SoundEvent> sounds;
    std::unordered_map<std::string, uint32_t> nameToId;
};

} // namespace runeharbor::formats
