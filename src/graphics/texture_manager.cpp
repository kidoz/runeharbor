// SPDX-License-Identifier: MIT
#include "texture_manager.hpp"

#include <iostream>

namespace runeharbor::graphics
{

TextureManager::TextureManager(IRenderer& renderer, ImageLoader& imageLoader)
    : renderer(renderer), imageLoader(imageLoader)
{
}

void TextureManager::setSpriteLoader(SpriteLoader* loader)
{
    spriteLoader = loader;
}

TextureManager::~TextureManager()
{
    clear();
}

TextureHandle TextureManager::getTexture(const std::string& name)
{
    // 1. Check Cache
    auto it = textureCache.find(name);
    if (it != textureCache.end())
    {
        return it->second;
    }

    // 2. Load Image (from LOD)
    auto image = imageLoader.loadImage(name);
    if (!image)
    {
        return nullptr;
    }

    // 3. Create GPU Texture
    void* texture = renderer.createTexture(*image);
    if (texture)
    {
        textureCache[name] = texture;
    }

    return texture;
}

TextureHandle TextureManager::getSprite(const std::string& name, const Palette& palette)
{
    if (!spriteLoader)
    {
        return nullptr;
    }

    // Cache key: "SPRITE:name" (to distinguish from bitmaps if names collide)
    std::string cacheKey = "SPRITE:" + name;

    // 1. Check Cache
    auto it = textureCache.find(cacheKey);
    if (it != textureCache.end())
    {
        return it->second;
    }

    // 2. Load Sprite (from SPRITES.LOD)
    auto image = spriteLoader->loadSprite(name, palette);
    if (!image)
    {
        return nullptr;
    }

    // 3. Create GPU Texture
    void* texture = renderer.createTexture(*image);
    if (texture)
    {
        textureCache[cacheKey] = texture;
    }

    return texture;
}

void TextureManager::unloadTexture(const std::string& name)
{
    auto it = textureCache.find(name);
    if (it != textureCache.end())
    {
        renderer.destroyTexture(it->second);
        textureCache.erase(it);
    }
}

void TextureManager::clear()
{
    for (auto& pair : textureCache)
    {
        renderer.destroyTexture(pair.second);
    }
    textureCache.clear();
}

} // namespace runeharbor::graphics
