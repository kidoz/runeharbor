// SPDX-License-Identifier: MIT
#pragma once

#include <vector>
#include <cstdint>

#include "sprite.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

class SpriteParser
{
public:
    explicit SpriteParser(util::ILogger& logger);

    // Parses a sprite from a raw buffer
    Sprite parse(const std::vector<uint8_t>& data);

private:
    util::ILogger& logger;
};

} // namespace runeharbor::formats
