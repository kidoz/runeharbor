// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include "irenderer.hpp"
#include "image_loader.hpp"
#include "sprite_loader.hpp"

namespace runeharbor::graphics
{

using TextureHandle = void*;

class TextureManager
{
public:
    // Only ImageLoader is required initially; SpriteLoader is optional
    TextureManager(IRenderer& renderer, ImageLoader& imageLoader);
    
    // Set optional SpriteLoader
    void setSpriteLoader(SpriteLoader* spriteLoader);

    ~TextureManager();

    // Loads a bitmap/icon texture (using ImageLoader)
    TextureHandle getTexture(const std::string& name);

    // Loads a sprite texture (using SpriteLoader)
    // Requires a palette to be provided (usually loaded from BITMAPS.LOD via ImageLoader)
    TextureHandle getSprite(const std::string& name, const Palette& palette);

    void unloadTexture(const std::string& name);
    void clear();

private:
    IRenderer& renderer;
    ImageLoader& imageLoader;
    SpriteLoader* spriteLoader = nullptr;
    
    std::unordered_map<std::string, TextureHandle> textureCache;
};

} // namespace runeharbor::graphics
