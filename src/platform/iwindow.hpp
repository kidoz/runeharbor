// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Forward declaration
struct SDL_Window;

namespace runeharbor::platform
{

struct WindowConfig
{
    std::string title = "RuneHarbor Engine";
    uint32_t width = 800;
    uint32_t height = 600;
    bool fullscreen = false;
    bool resizable = true;
    std::optional<int> windowX;
    std::optional<int> windowY;
};

/// Mouse button indices
enum class MouseButton
{
    Left = 0,
    Middle = 1,
    Right = 2
};

/// Current state of the mouse
struct MouseState
{
    int x = 0;
    int y = 0;
    bool leftButton = false;
    bool middleButton = false;
    bool rightButton = false;
};

struct WindowPosition
{
    int x = 0;
    int y = 0;
};

class IWindow
{
  public:
    virtual ~IWindow() = default;

    virtual bool initialize(const WindowConfig& config) = 0;
    virtual void shutdown() = 0;
    virtual void processEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual void swapBuffers() = 0;

    /// Get the underlying SDL window (for advanced use)
    virtual SDL_Window* getSDLWindow() = 0;

    /// Get the current mouse state (position and button states)
    virtual MouseState getMouseState() const = 0;

    /// Get current top-left window position in screen coordinates.
    virtual std::optional<WindowPosition> getWindowPosition() const = 0;

    /// Check if a mouse button was clicked this frame (pressed and released)
    virtual bool wasMouseClicked(MouseButton button = MouseButton::Left) const = 0;

    /// Check if a mouse button was just pressed this frame (transition from up to down)
    virtual bool wasMousePressed(MouseButton button = MouseButton::Left) const = 0;

    /// Reset per-frame input state (call at end of frame)
    virtual void resetFrameState() = 0;
};

} // namespace runeharbor::platform
