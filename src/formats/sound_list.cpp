// SPDX-License-Identifier: MIT
#include "sound_list.hpp"

#include <format>

#include <cstring>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

SoundList::SoundList(util::ILogger& logger) : logger(logger) {}

bool SoundList::parse(const std::vector<uint8_t>& data)
{
    if (data.size() < 4)
    {
        logger.error("SoundList: data too small");
        return false;
    }

    uint32_t count = *reinterpret_cast<const uint32_t*>(data.data());
    if (data.size() < 4 + count * sizeof(SoundInfoRaw))
    {
        logger.error(
            std::format("SoundList: data size {} too small for {} entries", data.size(), count));
        return false;
    }

    sounds.clear();
    nameToId.clear();
    sounds.reserve(count);

    const SoundInfoRaw* rawEntries = reinterpret_cast<const SoundInfoRaw*>(data.data() + 4);

    for (uint32_t i = 0; i < count; i++)
    {
        const auto& raw = rawEntries[i];

        SoundEvent event;
        event.name = raw.name;
        event.soundId = raw.soundId;
        event.type = raw.type;
        event.flags = raw.flags;
        std::memcpy(event.dataIds.data(), raw.dataId, sizeof(raw.dataId));
        event.p3dSoundId = raw.p3dSoundId;
        event.isDecompressed = (raw.isDecompressed != 0);

        if (sounds.contains(event.soundId))
        {
            logger.debug(std::format("SoundList: Duplicate sound ID {}", event.soundId));
        }

        std::string lowerName = util::toLower(event.name);
        nameToId[lowerName] = event.soundId;
        sounds[event.soundId] = std::move(event);
    }

    logger.info(std::format("Parsed {} sound events from dsounds.bin", sounds.size()));
    return true;
}

const SoundEvent* SoundList::getSound(uint32_t soundId) const
{
    auto it = sounds.find(soundId);
    if (it != sounds.end())
    {
        return &it->second;
    }
    return nullptr;
}

const SoundEvent* SoundList::getSoundByName(const std::string& name) const
{
    std::string lowerName = util::toLower(name);
    auto it = nameToId.find(lowerName);
    if (it != nameToId.end())
    {
        return getSound(it->second);
    }
    return nullptr;
}

} // namespace runeharbor::formats
