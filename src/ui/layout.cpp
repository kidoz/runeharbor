// SPDX-License-Identifier: MIT
#include "layout.hpp"

namespace runeharbor::ui
{

void Layout::addElement(int x, int y, const std::string& textureName)
{
    elements.push_back({x, y, textureName});
}

void Layout::render(graphics::IRenderer& renderer, graphics::TextureManager& textures)
{
    for (const auto& element : elements)
    {
        auto texture = textures.getTexture(element.textureName);
        if (texture)
        {
            renderer.renderTexture(texture, element.x, element.y, 0, 0);
        }
    }
}

} // namespace runeharbor::ui
