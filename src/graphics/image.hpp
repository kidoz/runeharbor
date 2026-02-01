// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "palette.hpp"

namespace runeharbor::graphics
{

/// Represents an RGBA image (32-bit color)
class Image
{
  public:
    Image(uint32_t width, uint32_t height);

    /// Convert from 8-bit paletted data to RGBA
    /// @param palettedData Raw 8-bit paletted pixel data
    /// @param width Image width
    /// @param height Image height
    /// @param palette Color palette for conversion
    /// @return New Image with RGBA data
    ///
    /// Note: If palettedData contains mipmaps, only the first width×height bytes
    ///       are used (main texture level 0).
    static std::unique_ptr<Image> fromPalettedData(const std::vector<uint8_t>& palettedData,
                                                   uint32_t width, uint32_t height,
                                                   const Palette& palette);

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

    /// Get RGBA data (4 bytes per pixel: R, G, B, A)
    const std::vector<uint8_t>& getRGBAData() const { return rgbaData; }

    /// Get mutable RGBA data
    std::vector<uint8_t>& getRGBAData() { return rgbaData; }

    /// Get pointer to RGBA data
    const uint8_t* data() const { return rgbaData.data(); }

    /// Get size of RGBA data in bytes
    size_t dataSize() const { return rgbaData.size(); }

  private:
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> rgbaData; // 4 bytes per pixel (RGBA)
};

} // namespace runeharbor::graphics
