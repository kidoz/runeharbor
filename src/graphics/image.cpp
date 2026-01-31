// SPDX-License-Identifier: MIT
#include "image.hpp"
#include <stdexcept>

namespace runeharbor::graphics
{

Image::Image(uint32_t width, uint32_t height) : width(width), height(height)
{
    if (width == 0 || height == 0)
    {
        throw std::invalid_argument("Image dimensions must be greater than zero");
    }

    // Allocate RGBA data (4 bytes per pixel)
    rgbaData.resize(width * height * 4, 0);
}

std::unique_ptr<Image> Image::fromPalettedData(const std::vector<uint8_t>& palettedData,
                                                 uint32_t width, uint32_t height,
                                                 const Palette& palette)
{
    if (width == 0 || height == 0)
    {
        throw std::invalid_argument("Image dimensions must be greater than zero");
    }

    // Calculate required size for main texture (level 0, no mipmaps)
    size_t requiredSize = width * height;

    if (palettedData.size() < requiredSize)
    {
        throw std::invalid_argument("Paletted data too small for specified dimensions");
    }

    auto image = std::make_unique<Image>(width, height);

    // Convert each paletted pixel to RGBA
    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            size_t srcIndex = y * width + x;
            size_t dstIndex = srcIndex * 4;

            // Read palette index
            uint8_t colorIndex = palettedData[srcIndex];

            // Look up color in palette
            const auto& color = palette.getColor(colorIndex);

            // Write RGBA
            image->rgbaData[dstIndex + 0] = color.r;
            image->rgbaData[dstIndex + 1] = color.g;
            image->rgbaData[dstIndex + 2] = color.b;
            image->rgbaData[dstIndex + 3] = color.a;
        }
    }

    return image;
}

} // namespace runeharbor::graphics
