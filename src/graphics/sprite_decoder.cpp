#include "sprite_decoder.hpp"
#include <cstring>
#include <algorithm>

namespace runeharbor::graphics
{

std::unique_ptr<Image> SpriteDecoder::decode(const std::vector<uint8_t>& data, 
                                            const Palette& palette, 
                                            util::ILogger& logger)
{
    // Format: [width:2][height:2][centerX:2][centerY:2][paletteId:2][lineInfo:height*8][pixels...]
    if (data.size() < 10)
    {
        logger.error("Sprite data too small for header");
        return nullptr;
    }

    const uint8_t* rawData = data.data();
    uint16_t width = *reinterpret_cast<const uint16_t*>(rawData);
    uint16_t height = *reinterpret_cast<const uint16_t*>(rawData + 2);
    // int16_t centerX = *reinterpret_cast<const int16_t*>(rawData + 4);
    // int16_t centerY = *reinterpret_cast<const int16_t*>(rawData + 6);
    // uint16_t paletteId = *reinterpret_cast<const uint16_t*>(rawData + 8);

    size_t lineInfoSize = height * 8;
    size_t headerSize = 10 + lineInfoSize;
    if (data.size() < headerSize)
    {
        logger.error("Sprite data too small for line info");
        return nullptr;
    }

    const uint8_t* lineInfo = rawData + 10;
    const uint8_t* pixelData = rawData + headerSize;
    size_t pixelDataSize = data.size() - headerSize;

    // Create image
    auto image = std::make_unique<Image>(width, height);
    auto& rgbaData = image->getRGBAData();

    // Clear to transparent
    std::fill(rgbaData.begin(), rgbaData.end(), 0);

    for (uint16_t y = 0; y < height; y++)
    {
        // Read line info (8 bytes per line)
        const uint8_t* lineEntry = lineInfo + y * 8;
        uint16_t xStart = *reinterpret_cast<const uint16_t*>(lineEntry);
        uint16_t xEnd = *reinterpret_cast<const uint16_t*>(lineEntry + 2);
        uint32_t dataOffset = *reinterpret_cast<const uint32_t*>(lineEntry + 4);

        // Check for empty line (0xFFFF marker)
        if (xStart == 0xFFFF)
        {
            continue;
        }

        // Validate bounds
        if (xStart >= width || xEnd > width || xStart > xEnd)
        {
            continue;
        }

        if (dataOffset >= pixelDataSize)
        {
            continue;
        }

        // Copy pixels for this line segment
        uint16_t pixelCount = xEnd - xStart;
        const uint8_t* srcPixels = pixelData + dataOffset;

        for (uint16_t i = 0; i < pixelCount && (xStart + i) < width; i++)
        {
            if (dataOffset + i >= pixelDataSize)
            {
                break;
            }

            uint8_t paletteIndex = srcPixels[i];
            size_t pixelOffset = (y * width + xStart + i) * 4;

            const auto& color = palette.getColor(paletteIndex);

            // Store RGBA
            // Note: Palette alpha is 255 by default. 
            // If the sprite pixel exists here, it is opaque.
            // Transparent parts are skipped by the RLE-like structure (xStart/xEnd segments).
            rgbaData[pixelOffset + 0] = color.r;
            rgbaData[pixelOffset + 1] = color.g;
            rgbaData[pixelOffset + 2] = color.b;
            rgbaData[pixelOffset + 3] = 255;
        }
    }

    return image;
}

} // namespace runeharbor::graphics
