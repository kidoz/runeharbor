// SPDX-License-Identifier: MIT
#include "image.hpp"

namespace runeharbor::graphics
{

Image::Image(uint32_t width, uint32_t height) : width(width), height(height)
{
    rgbaData.resize(width * height * 4, 0);
}

std::expected<std::unique_ptr<Image>, util::Error>
Image::fromPalettedData(const std::vector<uint8_t>& palettedData, uint32_t width, uint32_t height,
                        const Palette& palette)
{
    if (width == 0 || height == 0)
    {
        return std::unexpected(util::Error("Image dimensions must be greater than zero"));
    }

    size_t requiredSize = width * height;

    if (palettedData.size() < requiredSize)
    {
        return std::unexpected(util::Error("Paletted data too small for specified dimensions"));
    }

    auto image = std::make_unique<Image>(width, height);

    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            size_t srcIndex = y * width + x;
            size_t dstIndex = srcIndex * 4;

            uint8_t colorIndex = palettedData[srcIndex];
            const auto& color = palette.getColor(colorIndex);

            image->rgbaData[dstIndex + 0] = color.r;
            image->rgbaData[dstIndex + 1] = color.g;
            image->rgbaData[dstIndex + 2] = color.b;
            image->rgbaData[dstIndex + 3] = color.a;
        }
    }

    return image;
}

std::expected<std::unique_ptr<Image>, util::Error>
Image::fromRGBAData(const std::vector<uint8_t>& rgbaData, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return std::unexpected(util::Error("Image dimensions must be greater than zero"));
    }

    size_t requiredSize = static_cast<size_t>(width) * height * 4;
    if (rgbaData.size() < requiredSize)
    {
        return std::unexpected(util::Error("RGBA data too small for specified dimensions"));
    }

    auto image = std::make_unique<Image>(width, height);
    std::copy_n(rgbaData.begin(), requiredSize, image->rgbaData.begin());
    return image;
}

} // namespace runeharbor::graphics
