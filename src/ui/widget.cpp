// SPDX-License-Identifier: MIT
#include "widget.hpp"

namespace runeharbor::ui
{

bool Widget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;

    // Dispatch to children in reverse order (top-most first)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it)
    {
        if ((*it)->handleEvent(event))
            return true;
    }
    return false;
}

Widget* Widget::addChild(std::unique_ptr<Widget> child)
{
    child->parent_ = this;
    children_.push_back(std::move(child));
    return children_.back().get();
}

Widget* Widget::childAt(int index)
{
    if (index < 0 || index >= static_cast<int>(children_.size()))
        return nullptr;
    return children_[static_cast<size_t>(index)].get();
}

Widget* Widget::findById(const std::string& searchId)
{
    if (id_ == searchId)
        return this;
    for (auto& child : children_)
    {
        Widget* found = child->findById(searchId);
        if (found)
            return found;
    }
    return nullptr;
}

} // namespace runeharbor::ui
