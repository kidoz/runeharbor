#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include "image.hpp"
#include "palette.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::graphics
{

class SpriteDecoder
{
public:
    // Decodes a sprite from SPRITES.LOD raw data format into an RGBA Image.
    // data: The raw file content extracted from SPRITES.LOD
    // palette: The palette to use for color lookup
    static std::unique_ptr<Image> decode(const std::vector<uint8_t>& data, 
                                        const Palette& palette, 
                                        util::ILogger& logger);
};

} // namespace runeharbor::graphics
