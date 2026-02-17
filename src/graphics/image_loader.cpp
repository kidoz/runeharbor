#include "image_loader.hpp"
#include <format>
#include <iostream>
#include <vector>

namespace runeharbor::graphics {

ImageLoader::ImageLoader(formats::ImageLODArchive& archive) : archive(archive) {}

std::unique_ptr<Image> ImageLoader::loadImage(const std::string& name) {
    auto info = archive.getFileInfo(name);
    if (!info) {
        return nullptr;
    }

    auto data = archive.extractFile(name);
    if (!data) {
        return nullptr;
    }

    const Palette& palette = loadPalette(info->paletteId);
    
    // Create image from raw data + palette
    return Image::fromPalettedData(*data, info->width, info->height, palette);
}

const Palette& ImageLoader::loadPalette(int paletteId) {
    if (paletteCache.contains(paletteId)) {
        return paletteCache.at(paletteId);
    }

    // Try common palette naming conventions
    std::string palName = std::format("pal{}", paletteId);
    auto data = archive.extractFile(palName);

    if (!data) {
        // Try padded with zeros if needed, e.g. PAL001
        palName = std::format("pal{:03d}", paletteId);
        data = archive.extractFile(palName);
    }

    if (!data) {
        // Try fully uppercase? Though archive is case-insensitive.
        // Maybe try "palette" prefix?
        // Fallback to default palette
        static Palette defaultPal = Palette::createDefaultPalette();
        // Cache the default so we don't try again
        paletteCache[paletteId] = defaultPal;
        return paletteCache[paletteId];
    }
    
    // Validate size (should be exactly 768 bytes for 256 RGB colors)
    if (data->size() < 768) {
        // Log warning?
        static Palette defaultPal = Palette::createDefaultPalette();
        paletteCache[paletteId] = defaultPal;
        return paletteCache[paletteId];
    }
    
    // If larger than 768, take the LAST 768 bytes just in case there's a header we didn't strip properly
    // (though ImageLODArchive should handle headers)
    // Palettes are raw RGB data.
    std::vector<uint8_t> rgbData;
    if (data->size() > 768) {
        rgbData.assign(data->end() - 768, data->end());
    } else {
        rgbData = *data;
    }
    
    try {
        Palette pal = Palette::fromRGBData(rgbData);
        paletteCache[paletteId] = pal;
        return paletteCache[paletteId];
    } catch (...) {
        static Palette defaultPal = Palette::createDefaultPalette();
        paletteCache[paletteId] = defaultPal;
        return paletteCache[paletteId];
    }
}

} // namespace runeharbor::graphics
