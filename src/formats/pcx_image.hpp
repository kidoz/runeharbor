// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "../graphics/palette.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::formats
{

struct PcxImage
{
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> indices; // width * height palette indices
    graphics::Palette palette;
};

/// Decode an 8-bit PCX file with RLE compression and 256-color palette.
std::optional<PcxImage> decodePCX(const std::vector<uint8_t>& data, util::ILogger& logger);

} // namespace runeharbor::formats
