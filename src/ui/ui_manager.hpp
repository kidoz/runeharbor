// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "widget.hpp"

namespace runeharbor::graphics
{
class IRenderer;
class DebugText;
} // namespace runeharbor::graphics

namespace runeharbor::ui
{

/// Top-level UI manager: owns the widget tree, dispatches events, renders.
class UIManager
{
  public:
    UIManager() = default;

    /// Add a root-level widget and return a non-owning pointer
    Widget* addWidget(std::unique_ptr<Widget> widget);

    /// Remove all widgets
    void clear();

    /// Dispatch an input event to all widgets (front-to-back)
    bool handleEvent(const UIEvent& event);

    /// Render all widgets in order
    void render(graphics::IRenderer& renderer, const graphics::DebugText& text);

    /// Find a widget by ID across all roots
    Widget* findById(const std::string& id);

    /// Number of root widgets
    int widgetCount() const { return static_cast<int>(widgets_.size()); }

    /// Set which widget has keyboard focus
    void setFocus(Widget* widget);

    /// Get currently focused widget
    Widget* focusedWidget() const { return focused_; }

  private:
    std::vector<std::unique_ptr<Widget>> widgets_;
    Widget* focused_ = nullptr;
};

} // namespace runeharbor::ui
