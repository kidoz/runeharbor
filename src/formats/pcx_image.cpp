// SPDX-License-Identifier: MIT
#include "pcx_image.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <format>

namespace runeharbor::formats
{

namespace
{
struct PcxHeader
{
    uint8_t manufacturer;
    uint8_t version;
    uint8_t encoding;
    uint8_t bitsPerPixel;
    uint16_t xMin;
    uint16_t yMin;
    uint16_t xMax;
    uint16_t yMax;
    uint16_t hDpi;
    uint16_t vDpi;
    uint8_t palette16[48];
    uint8_t reserved;
    uint8_t colorPlanes;
    uint16_t bytesPerLine;
    uint16_t paletteType;
    uint16_t hScreenSize;
    uint16_t vScreenSize;
    uint8_t filler[54];
};

static_assert(sizeof(PcxHeader) == 128, "PCX header must be 128 bytes");

uint16_t read16(const uint16_t value)
{
    return value;
}
} // namespace

std::optional<PcxImage> decodePCX(const std::vector<uint8_t>& data, util::ILogger& logger)
{
    if (data.size() < sizeof(PcxHeader))
    {
        logger.error("PCX decode failed: data too small for header");
        return std::nullopt;
    }

    PcxHeader header;
    std::memcpy(&header, data.data(), sizeof(PcxHeader));

    if (header.manufacturer != 0x0A || header.encoding != 1)
    {
        logger.error("PCX decode failed: unsupported manufacturer or encoding");
        return std::nullopt;
    }

    if (header.bitsPerPixel != 8 || header.colorPlanes != 1)
    {
        logger.error("PCX decode failed: only 8-bit, single-plane PCX is supported");
        return std::nullopt;
    }

    const uint16_t xMin = read16(header.xMin);
    const uint16_t yMin = read16(header.yMin);
    const uint16_t xMax = read16(header.xMax);
    const uint16_t yMax = read16(header.yMax);

    if (xMax < xMin || yMax < yMin)
    {
        logger.error("PCX decode failed: invalid bounds");
        return std::nullopt;
    }

    const uint16_t width = static_cast<uint16_t>(xMax - xMin + 1);
    const uint16_t height = static_cast<uint16_t>(yMax - yMin + 1);
    const uint16_t bytesPerLine = read16(header.bytesPerLine);

    if (width == 0 || height == 0 || bytesPerLine == 0)
    {
        logger.error("PCX decode failed: invalid dimensions");
        return std::nullopt;
    }

    size_t decodedTarget = static_cast<size_t>(bytesPerLine) * height;
    std::vector<uint8_t> decoded;
    decoded.reserve(decodedTarget);

    size_t srcPos = sizeof(PcxHeader);
    while (decoded.size() < decodedTarget && srcPos < data.size())
    {
        uint8_t byte = data[srcPos++];
        if ((byte & 0xC0) == 0xC0)
        {
            uint8_t count = byte & 0x3F;
            if (srcPos >= data.size())
            {
                break;
            }
            uint8_t value = data[srcPos++];
            decoded.insert(decoded.end(), count, value);
        }
        else
        {
            decoded.push_back(byte);
        }
    }

    if (decoded.size() < decodedTarget)
    {
        logger.error("PCX decode failed: RLE stream too short");
        return std::nullopt;
    }

    std::vector<uint8_t> indices;
    indices.resize(static_cast<size_t>(width) * height);

    for (uint16_t y = 0; y < height; y++)
    {
        const size_t srcOffset = static_cast<size_t>(y) * bytesPerLine;
        const size_t dstOffset = static_cast<size_t>(y) * width;
        std::copy_n(decoded.begin() + srcOffset, width, indices.begin() + dstOffset);
    }

    graphics::Palette palette = graphics::Palette::createDefaultPalette();
    if (data.size() >= 769 && data[data.size() - 769] == 0x0C)
    {
        std::vector<uint8_t> paletteData(data.end() - 768, data.end());
        try
        {
            palette = graphics::Palette::fromRGBData(paletteData);
        }
        catch (const std::exception& ex)
        {
            logger.warning(std::format("PCX palette load failed: {}", ex.what()));
        }
    }
    else
    {
        logger.warning("PCX palette marker not found; using default palette");
    }

    PcxImage result;
    result.width = width;
    result.height = height;
    result.indices = std::move(indices);
    result.palette = palette;
    return result;
}

} // namespace runeharbor::formats
