// SPDX-License-Identifier: MIT
#include "inventory_widget.hpp"

#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

InventoryWidget::InventoryWidget() {}

void InventoryWidget::render(graphics::IRenderer& renderer, [[maybe_unused]] const graphics::DebugText& text)
{
    if (!visible_) return;

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl) return;

    // Draw inventory frame
    if (bgTexture_)
    {
        graphics::Rect src = {0, 0, bgW_, bgH_};
        graphics::Rect dst = {bounds_.x, bounds_.y, bounds_.width, bounds_.height};
        renderer.drawTexture(bgTexture_, src, dst);
    }
    else
    {
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 40, 30, 20, 240);
    }

    // Draw character body
    if (bodyTexture_ && gameWorld_ && activeCharacterIndex_ >= 0)
    {
        // Body usually sits around x=10, y=20 relative to the inventory window in MM7
        int scaleW = bounds_.width * bodyW_ / 460;
        int scaleH = bounds_.height * bodyH_ / 344;
        
        int drawX = bounds_.x + (bounds_.width * 16 / 460); // approximate MM7 layout
        int drawY = bounds_.y + (bounds_.height * 16 / 344);
        
        graphics::Rect src = {0, 0, bodyW_, bodyH_};
        graphics::Rect dst = {drawX, drawY, scaleW, scaleH};
        renderer.drawTexture(bodyTexture_, src, dst);
    }
}

bool InventoryWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_) return false;
    // Just swallow clicks for now
    if (event.type == UIEventType::MouseDown && bounds_.contains(event.mouseX, event.mouseY))
    {
        return true;
    }
    return false;
}

} // namespace runeharbor::ui