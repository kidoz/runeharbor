// SPDX-License-Identifier: MIT
#include "../util/string_utils.hpp"
#include <format>

#include "../util/ilogger.hpp"
#include "sprite_parser.hpp"

namespace runeharbor::formats
{

namespace
{
#pragma pack(push, 1)
struct MM7SpriteHeader
{
    uint16_t width;
    uint16_t height;
    uint16_t frameCount;
    uint16_t paletteData;
    uint16_t paletteIndex;
    uint32_t unknown;
};

struct MM7SpriteFrameInfo
{
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint32_t dataSize;
    uint32_t unknown;
};
#pragma pack(pop)
} // namespace

SpriteParser::SpriteParser(util::ILogger& logger) : logger(logger) {}

Sprite SpriteParser::parse(const std::vector<uint8_t>& data)
{
    Sprite sprite;

    if (data.size() < sizeof(MM7SpriteHeader))
    {
        logger.debug("Sprite data is smaller than sprite header");
        return sprite;
    }

    const uint8_t* p = data.data();
    const MM7SpriteHeader* header = reinterpret_cast<const MM7SpriteHeader*>(p);
    p += sizeof(MM7SpriteHeader);

    sprite.width = header->width;
    sprite.height = header->height;
    sprite.paletteIndex = header->paletteIndex;

    // After the unknown data block, there is a 768-byte palette.
    if (static_cast<size_t>(data.data() + data.size() - p) < 768)
    {
        logger.debug("Not enough data for sprite palette");
        return sprite;
    }
    std::vector<uint8_t> paletteData(768);
    std::memcpy(paletteData.data(), p, 768);
    sprite.palette = graphics::Palette::fromRGBData(paletteData);
    p += 768;

    for (int i = 0; i < header->frameCount; i++)
    {
        if (static_cast<size_t>(data.data() + data.size() - p) < sizeof(MM7SpriteFrameInfo))
        {
            logger.debug("Not enough data for sprite frame info");
            return sprite;
        }
        const MM7SpriteFrameInfo* frameInfo = reinterpret_cast<const MM7SpriteFrameInfo*>(p);
        p += sizeof(MM7SpriteFrameInfo);

        SpriteFrame frame;
        frame.left = frameInfo->left;
        frame.top = frameInfo->top;
        frame.width = frameInfo->width;
        frame.height = frameInfo->height;

        if (static_cast<size_t>(data.data() + data.size() - p) < frameInfo->dataSize)
        {
            logger.debug("Not enough data for sprite frame data");
            return sprite;
        }
        frame.data.resize(frameInfo->dataSize);
        std::memcpy(frame.data.data(), p, frameInfo->dataSize);
        p += frameInfo->dataSize;

        sprite.frames.push_back(frame);
    }

    return sprite;
}

} // namespace runeharbor::formats
