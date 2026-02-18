// SPDX-License-Identifier: MIT
#include "ui_manager.hpp"

namespace runeharbor::ui
{

Widget* UIManager::addWidget(std::unique_ptr<Widget> widget)
{
    widgets_.push_back(std::move(widget));
    return widgets_.back().get();
}

void UIManager::clear()
{
    focused_ = nullptr;
    widgets_.clear();
}

bool UIManager::handleEvent(const UIEvent& event)
{
    // Dispatch in reverse order (top-most widget first)
    for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it)
    {
        if ((*it)->handleEvent(event))
            return true;
    }
    return false;
}

void UIManager::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
{
    for (auto& widget : widgets_)
    {
        if (widget->visible())
            widget->render(renderer, text);
    }
}

Widget* UIManager::findById(const std::string& id)
{
    for (auto& widget : widgets_)
    {
        Widget* found = widget->findById(id);
        if (found)
            return found;
    }
    return nullptr;
}

void UIManager::setFocus(Widget* widget)
{
    if (focused_ == widget)
        return;
    if (focused_)
        focused_->setFocused(false);
    focused_ = widget;
    if (focused_)
        focused_->setFocused(true);
}

} // namespace runeharbor::ui
