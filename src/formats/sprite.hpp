// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

#include "../graphics/palette.hpp"

namespace runeharbor::formats
{

struct SpriteFrame
{
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    std::vector<uint8_t> data;
};

struct Sprite
{
    uint16_t width;
    uint16_t height;
    std::vector<SpriteFrame> frames;
    graphics::Palette palette;
    uint16_t paletteIndex;
};

} // namespace runeharbor::formats
