#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "image.hpp"
#include "palette.hpp"
#include "../formats/image_lod_archive.hpp"

namespace runeharbor::graphics {

class ImageLoader {
public:
    explicit ImageLoader(formats::ImageLODArchive& archive);

    // Load an image by name (e.g. "A1b")
    // This will automatically load the associated palette.
    std::unique_ptr<Image> loadImage(const std::string& name);

    // Load a palette by ID (e.g. 132 -> "pal132")
    const Palette& loadPalette(int paletteId);

private:
    formats::ImageLODArchive& archive;
    std::unordered_map<int, Palette> paletteCache;
};

} // namespace runeharbor::graphics
