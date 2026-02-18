// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "../formats/sprite_lod_archive.hpp"
#include "../util/ilogger.hpp"
#include "image.hpp"
#include "palette.hpp"

namespace runeharbor::graphics
{

class SpriteLoader
{
  public:
    explicit SpriteLoader(formats::SpriteLODArchive& archive, util::ILogger& logger);

    // Load a sprite by name (e.g. "key01")
    // Requires an external palette (sprites usually use the game's global palette or specific UI
    // palette)
    std::unique_ptr<Image> loadSprite(const std::string& name, const Palette& palette);

  private:
    formats::SpriteLODArchive& archive;
    util::ILogger& logger;
};

} // namespace runeharbor::graphics
