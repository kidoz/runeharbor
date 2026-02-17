#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../graphics/irenderer.hpp"
#include "../graphics/texture_manager.hpp"

namespace runeharbor::ui
{

struct UIElement
{
    int x;
    int y;
    std::string textureName;
    bool isSprite = false;
    // For sprites, we need a palette. For MVP, we assume global palette or store it here?
    // Let's keep it simple: sprites use the palette passed to render() or just name reference.
    // TextureManager::getSprite needs a palette.
    // Let's assume for Layout, we only handle static images for now, or we store the palette pointer?
    // Or we assume textures are already loaded in TextureManager?
    // TextureManager::getTexture returns a handle. Maybe Layout should store handles?
    // But roadmap says "textureName".
    // Let's stick to textureName and assume standard textures for UI.
    // Sprites in UI usually are just icons (which are standard textures in ICONS.LOD).
    // Actual game sprites (monsters) are not UI.
};

class Layout
{
public:
    void addElement(int x, int y, const std::string& textureName);
    
    // For MVP, we render using TextureManager which loads on demand
    void render(graphics::IRenderer& renderer, graphics::TextureManager& textures);

private:
    std::vector<UIElement> elements;
};

} // namespace runeharbor::ui
