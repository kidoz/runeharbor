// SPDX-License-Identifier: MIT
#include "choice_list_widget.hpp"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <utility>

#include <cstring>

#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

void ChoiceListWidget::open(std::string title, std::vector<Choice> choices)
{
    title_ = std::move(title);
    choices_ = std::move(choices);
    selected_ = 0;
    pendingId_ = -1;
    rowRects_.clear();
    setVisible(true);
}

int ChoiceListWidget::takeSelection()
{
    const int id = pendingId_;
    pendingId_ = -1;
    return id;
}

int ChoiceListWidget::selectedId() const
{
    if (choices_.empty())
    {
        return -1;
    }
    const int index = std::clamp(selected_, 0, static_cast<int>(choices_.size()) - 1);
    return choices_[static_cast<size_t>(index)].id;
}

void ChoiceListWidget::moveSelection(int delta)
{
    const int count = static_cast<int>(choices_.size());
    if (count <= 0)
    {
        return;
    }
    int target = (selected_ + delta) % count;
    if (target < 0)
    {
        target += count;
    }
    selected_ = target;
}

void ChoiceListWidget::confirmSelection()
{
    if (choices_.empty())
    {
        return;
    }
    const int index = std::clamp(selected_, 0, static_cast<int>(choices_.size()) - 1);
    pendingId_ = choices_[static_cast<size_t>(index)].id;
    setVisible(false);
}

bool ChoiceListWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
    {
        return false;
    }

    if (event.type == UIEventType::KeyDown)
    {
        switch (event.scancode)
        {
        case SDL_SCANCODE_UP:
            moveSelection(-1);
            return true;
        case SDL_SCANCODE_DOWN:
            moveSelection(1);
            return true;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            confirmSelection();
            return true;
        case SDL_SCANCODE_ESCAPE:
            pendingId_ = -1;
            setVisible(false);
            return true;
        default:
            return false;
        }
    }

    if (event.type == UIEventType::MouseDown)
    {
        if (!bounds_.contains(event.mouseX, event.mouseY))
        {
            return false;
        }
        for (size_t i = 0; i < rowRects_.size(); i++)
        {
            if (rowRects_[i].contains(event.mouseX, event.mouseY))
            {
                selected_ = static_cast<int>(i);
                confirmSelection();
                return true;
            }
        }
        if (cancelRect_.contains(event.mouseX, event.mouseY))
        {
            pendingId_ = -1;
            setVisible(false);
            return true;
        }
        // Modal: swallow clicks on the rest of the screen.
        return true;
    }

    return false;
}

void ChoiceListWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
{
    if (!visible_)
    {
        return;
    }

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
    {
        return;
    }

    const int panelW = std::min(bounds_.width - 32, 440);
    const int rowHeight = 24;
    const int listHeight = static_cast<int>(choices_.size()) * rowHeight;
    const int panelH = std::min(bounds_.height - 32, 130 + listHeight);
    const int px = bounds_.x + (bounds_.width - panelW) / 2;
    const int py = bounds_.y + (bounds_.height - panelH) / 2;

    renderer.drawFilledRect(px, py, panelW, panelH, 30, 26, 20, 245);
    renderer.drawRect(px, py, panelW, panelH, 130, 108, 60, 255);
    renderer.drawRect(px + 1, py + 1, panelW - 2, panelH - 2, 90, 78, 44, 210);

    text.drawText(sdl, px + 20, py + 14, 2, 255, 220, 120, title_);
    renderer.drawFilledRect(px + 16, py + 50, panelW - 32, 1, 110, 90, 50, 220);

    rowRects_.clear();
    rowRects_.reserve(choices_.size());
    const int rowsTop = py + 60;
    for (size_t i = 0; i < choices_.size(); i++)
    {
        const int rowY = rowsTop + static_cast<int>(i) * rowHeight;
        rowRects_.push_back({px + 16, rowY, panelW - 32, rowHeight});
        const bool selected = static_cast<int>(i) == selected_;
        if (selected)
        {
            renderer.drawFilledRect(px + 16, rowY, panelW - 32, rowHeight, 80, 70, 30, 220);
        }
        text.drawText(sdl, px + 28, rowY + 5, 1, selected ? 250 : 205, selected ? 230 : 195,
                      selected ? 170 : 165, choices_[i].label);
    }

    if (choices_.empty())
    {
        text.drawText(sdl, px + 28, rowsTop + 5, 1, 200, 160, 150, "(no options available)");
    }

    text.drawText(sdl, px + 20, py + panelH - 62, 1, 170, 170, 150,
                  "Up/Dn: select   Enter: choose   Esc: cancel");

    cancelRect_ = {px + panelW - 20 - 90, py + panelH - 36, 90, 24};
    renderer.drawFilledRect(cancelRect_.x, cancelRect_.y, cancelRect_.width, cancelRect_.height, 50,
                            42, 22, 230);
    renderer.drawRect(cancelRect_.x, cancelRect_.y, cancelRect_.width, cancelRect_.height, 130, 108,
                      60, 255);
    const char* cancelLabel = "Cancel";
    const int labelWidth = text.charWidth(1) * static_cast<int>(::strlen(cancelLabel));
    text.drawText(sdl, cancelRect_.x + (cancelRect_.width - labelWidth) / 2, cancelRect_.y + 5, 1,
                  200, 190, 170, cancelLabel);
}

} // namespace runeharbor::ui
