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
    std::vector<uint8_t> indices;    // width * height palette indices (paletted mode)
    std::vector<uint8_t> rgbaPixels; // width * height * 4 RGBA data (24-bit mode)
    graphics::Palette palette;

    bool is24Bit() const { return !rgbaPixels.empty(); }
};

/// Decode a PCX file with RLE compression.
/// Supports 8-bit paletted (1 plane) and 24-bit RGB (3 planes).
std::optional<PcxImage> decodePCX(const std::vector<uint8_t>& data, util::ILogger& logger);

} // namespace runeharbor::formats
