// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "widget.hpp"

namespace runeharbor::ui
{

// ── Panel ────────────────────────────────────────────────────────────────────
/// Container widget with optional background color or texture.
class Panel : public Widget
{
  public:
    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void setBorderColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void setBackgroundTexture(void* tex) { bgTexture_ = tex; }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;

  private:
    uint8_t bgR_ = 0, bgG_ = 0, bgB_ = 0, bgA_ = 0;
    uint8_t borderR_ = 0, borderG_ = 0, borderB_ = 0, borderA_ = 0;
    bool hasBg_ = false;
    bool hasBorder_ = false;
    void* bgTexture_ = nullptr;
};

// ── Label ────────────────────────────────────────────────────────────────────
/// Text display widget.
class Label : public Widget
{
  public:
    void setText(const std::string& t) { text_ = t; }
    const std::string& text() const { return text_; }
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setScale(int s) { scale_ = s; }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;

  private:
    std::string text_;
    uint8_t colorR_ = 255, colorG_ = 255, colorB_ = 255;
    int scale_ = 1;
};

// ── Button ───────────────────────────────────────────────────────────────────
/// Clickable button with text or texture, hover/pressed visual states.
class Button : public Widget
{
  public:
    void setText(const std::string& t) { text_ = t; }
    void setTextScale(int s) { textScale_ = s; }
    void setTexture(void* normal, void* hover = nullptr, void* pressed = nullptr);

    // Colors
    void setNormalColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 200);
    void setHoverColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 220);
    void setPressedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void setTextColor(uint8_t r, uint8_t g, uint8_t b);

    // Callback
    void setOnClick(std::function<void()> cb) { onClick_ = std::move(cb); }

    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressed_; }

    bool handleEvent(const UIEvent& event) override;
    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;

  private:
    std::string text_;
    int textScale_ = 1;

    // Textures (non-owning)
    void* texNormal_ = nullptr;
    void* texHover_ = nullptr;
    void* texPressed_ = nullptr;

    // Background colors per state
    struct Color
    {
        uint8_t r, g, b, a;
    };
    Color normalColor_ = {60, 60, 80, 200};
    Color hoverColor_ = {80, 80, 110, 220};
    Color pressedColor_ = {40, 40, 60, 255};
    Color textColor_ = {255, 255, 255, 255};

    bool hovered_ = false;
    bool pressed_ = false;
    std::function<void()> onClick_;
};

// ── ListBox ──────────────────────────────────────────────────────────────────
/// Scrollable list of selectable text items.
class ListBox : public Widget
{
  public:
    void setItems(const std::vector<std::string>& items);
    void addItem(const std::string& item);
    void clearItems();

    int selectedIndex() const { return selectedIndex_; }
    void setSelectedIndex(int idx);
    const std::string* selectedItem() const;

    void setTextScale(int s) { textScale_ = s; }
    void setItemHeight(int h) { itemHeight_ = h; }
    void setTextColor(uint8_t r, uint8_t g, uint8_t b);
    void setSelectionColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 180);

    // Callback when selection changes
    void setOnSelectionChanged(std::function<void(int)> cb) { onSelectionChanged_ = std::move(cb); }

    int itemCount() const { return static_cast<int>(items_.size()); }
    int scrollOffset() const { return scrollOffset_; }

    bool handleEvent(const UIEvent& event) override;
    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;

  private:
    int visibleItemCount() const;
    void clampScroll();

    std::vector<std::string> items_;
    int selectedIndex_ = -1;
    int scrollOffset_ = 0;
    int textScale_ = 1;
    int itemHeight_ = 0; // 0 = auto from text scale
    uint8_t textR_ = 255, textG_ = 255, textB_ = 255;
    uint8_t selR_ = 80, selG_ = 80, selB_ = 140, selA_ = 180;
    std::function<void(int)> onSelectionChanged_;
};

// ── ScrollBar ────────────────────────────────────────────────────────────────
/// Vertical scrollbar control.
class ScrollBar : public Widget
{
  public:
    void setRange(int totalItems, int visibleItems);
    void setPosition(int pos);
    int position() const { return position_; }

    void setOnScroll(std::function<void(int)> cb) { onScroll_ = std::move(cb); }

    void setTrackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 100);
    void setThumbColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 200);

    bool handleEvent(const UIEvent& event) override;
    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;

  private:
    graphics::Rect thumbRect() const;

    int totalItems_ = 0;
    int visibleItems_ = 1;
    int position_ = 0;
    bool dragging_ = false;
    int dragStartY_ = 0;
    int dragStartPos_ = 0;
    uint8_t trackR_ = 40, trackG_ = 40, trackB_ = 50, trackA_ = 100;
    uint8_t thumbR_ = 120, thumbG_ = 120, thumbB_ = 150, thumbA_ = 200;
    std::function<void(int)> onScroll_;
};

} // namespace runeharbor::ui
