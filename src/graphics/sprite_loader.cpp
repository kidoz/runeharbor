// SPDX-License-Identifier: MIT
#include "sprite_loader.hpp"

#include <iostream>

#include "sprite_decoder.hpp"

namespace runeharbor::graphics
{

SpriteLoader::SpriteLoader(formats::SpriteLODArchive& archive, util::ILogger& logger)
    : archive(archive), logger(logger)
{
}

std::unique_ptr<Image> SpriteLoader::loadSprite(const std::string& name, const Palette& palette)
{
    // Extract raw data
    auto data = archive.extractFile(name);
    if (!data)
    {
        logger.error("Failed to extract sprite: " + name);
        return nullptr;
    }

    // Decode using SpriteDecoder
    return SpriteDecoder::decode(*data, palette, logger);
}

} // namespace runeharbor::graphics
