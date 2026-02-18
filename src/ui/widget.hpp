// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../graphics/primitives.hpp"
#include "../platform/iwindow.hpp"

namespace runeharbor::graphics
{
class IRenderer;
class DebugText;
} // namespace runeharbor::graphics

namespace runeharbor::ui
{

/// Input event types for UI widgets
enum class UIEventType : uint8_t
{
    MouseMove,
    MouseDown,
    MouseUp,
    KeyDown,
};

/// Input event passed to widgets
struct UIEvent
{
    UIEventType type = UIEventType::MouseMove;
    int mouseX = 0; // Screen coordinates
    int mouseY = 0;
    platform::MouseButton mouseButton = platform::MouseButton::Left;
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
};

/// Base class for all UI widgets.
/// Widgets work in screen coordinates (already scaled by the game state).
class Widget
{
  public:
    virtual ~Widget() = default;

    // Bounds
    void setBounds(int x, int y, int w, int h) { bounds_ = {x, y, w, h}; }
    void setBounds(const graphics::Rect& r) { bounds_ = r; }
    const graphics::Rect& bounds() const { return bounds_; }

    // Visibility and enabled state
    bool visible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }
    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }

    // Hit testing
    bool containsPoint(int x, int y) const { return bounds_.contains(x, y); }

    // Event handling (return true if event was consumed)
    virtual bool handleEvent(const UIEvent& event);

    // Rendering
    virtual void render(graphics::IRenderer& renderer, const graphics::DebugText& text) = 0;

    // Child management
    Widget* addChild(std::unique_ptr<Widget> child);
    Widget* childAt(int index);
    int childCount() const { return static_cast<int>(children_.size()); }

    // Focus
    bool focused() const { return focused_; }
    void setFocused(bool f) { focused_ = f; }

    // ID for lookup
    const std::string& id() const { return id_; }
    void setId(const std::string& id) { id_ = id; }

    // Find child by ID (recursive)
    Widget* findById(const std::string& id);

  protected:
    graphics::Rect bounds_;
    bool visible_ = true;
    bool enabled_ = true;
    bool focused_ = false;
    std::string id_;
    Widget* parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;
};

} // namespace runeharbor::ui
