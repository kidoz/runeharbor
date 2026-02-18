// SPDX-License-Identifier: MIT
#include "widgets.hpp"

#include <algorithm>

#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"

namespace runeharbor::ui
{

// ── Panel ────────────────────────────────────────────────────────────────────

void Panel::setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    bgR_ = r;
    bgG_ = g;
    bgB_ = b;
    bgA_ = a;
    hasBg_ = true;
}

void Panel::setBorderColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    borderR_ = r;
    borderG_ = g;
    borderB_ = b;
    borderA_ = a;
    hasBorder_ = true;
}

void Panel::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
{
    if (!visible_)
        return;

    if (bgTexture_)
    {
        renderer.renderTexture(bgTexture_, bounds_.x, bounds_.y, bounds_.width, bounds_.height);
    }
    else if (hasBg_)
    {
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, bgR_, bgG_,
                                bgB_, bgA_);
    }

    if (hasBorder_)
    {
        renderer.drawRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, borderR_, borderG_,
                          borderB_, borderA_);
    }

    for (auto& child : children_)
    {
        child->render(renderer, text);
    }
}

// ── Label ────────────────────────────────────────────────────────────────────

void Label::setColor(uint8_t r, uint8_t g, uint8_t b)
{
    colorR_ = r;
    colorG_ = g;
    colorB_ = b;
}

void Label::render(graphics::IRenderer& renderer, const graphics::DebugText& debugText)
{
    if (!visible_ || text_.empty())
        return;

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    debugText.drawText(sdl, bounds_.x, bounds_.y, scale_, colorR_, colorG_, colorB_, text_);
}

// ── Button ───────────────────────────────────────────────────────────────────

void Button::setTexture(void* normal, void* hover, void* pressed)
{
    texNormal_ = normal;
    texHover_ = hover;
    texPressed_ = pressed;
}

void Button::setNormalColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    normalColor_ = {r, g, b, a};
}

void Button::setHoverColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    hoverColor_ = {r, g, b, a};
}

void Button::setPressedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    pressedColor_ = {r, g, b, a};
}

void Button::setTextColor(uint8_t r, uint8_t g, uint8_t b)
{
    textColor_ = {r, g, b, 255};
}

bool Button::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;

    switch (event.type)
    {
    case UIEventType::MouseMove:
        hovered_ = containsPoint(event.mouseX, event.mouseY);
        return false; // Don't consume move events

    case UIEventType::MouseDown:
        if (containsPoint(event.mouseX, event.mouseY))
        {
            pressed_ = true;
            return true;
        }
        pressed_ = false;
        return false;

    case UIEventType::MouseUp:
        if (pressed_ && containsPoint(event.mouseX, event.mouseY))
        {
            pressed_ = false;
            if (onClick_)
                onClick_();
            return true;
        }
        pressed_ = false;
        return false;

    default:
        return false;
    }
}

void Button::render(graphics::IRenderer& renderer, const graphics::DebugText& debugText)
{
    if (!visible_)
        return;

    // Pick texture/color based on state
    void* tex = texNormal_;
    Color bg = normalColor_;
    if (pressed_ && texPressed_)
    {
        tex = texPressed_;
        bg = pressedColor_;
    }
    else if (hovered_ && texHover_)
    {
        tex = texHover_;
        bg = hoverColor_;
    }
    else if (hovered_)
    {
        bg = hoverColor_;
    }
    else if (pressed_)
    {
        bg = pressedColor_;
    }

    if (tex)
    {
        renderer.renderTexture(tex, bounds_.x, bounds_.y, bounds_.width, bounds_.height);
    }
    else
    {
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, bg.r, bg.g,
                                bg.b, bg.a);
    }

    // Draw text centered
    if (!text_.empty())
    {
        SDL_Renderer* sdl = renderer.getSDLRenderer();
        if (sdl)
        {
            int textW = debugText.measureTextWidth(text_, textScale_);
            int textH = debugText.lineHeight(textScale_);
            int tx = bounds_.x + (bounds_.width - textW) / 2;
            int ty = bounds_.y + (bounds_.height - textH) / 2;
            debugText.drawText(sdl, tx, ty, textScale_, textColor_.r, textColor_.g, textColor_.b,
                               text_);
        }
    }
}

// ── ListBox ──────────────────────────────────────────────────────────────────

void ListBox::setItems(const std::vector<std::string>& items)
{
    items_ = items;
    selectedIndex_ = items_.empty() ? -1 : 0;
    scrollOffset_ = 0;
}

void ListBox::addItem(const std::string& item)
{
    items_.push_back(item);
    if (selectedIndex_ < 0)
        selectedIndex_ = 0;
}

void ListBox::clearItems()
{
    items_.clear();
    selectedIndex_ = -1;
    scrollOffset_ = 0;
}

void ListBox::setSelectedIndex(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(items_.size()))
        selectedIndex_ = -1;
    else
        selectedIndex_ = idx;
}

const std::string* ListBox::selectedItem() const
{
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(items_.size()))
        return nullptr;
    return &items_[static_cast<size_t>(selectedIndex_)];
}

void ListBox::setTextColor(uint8_t r, uint8_t g, uint8_t b)
{
    textR_ = r;
    textG_ = g;
    textB_ = b;
}

void ListBox::setSelectionColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    selR_ = r;
    selG_ = g;
    selB_ = b;
    selA_ = a;
}

bool ListBox::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;

    int ih = itemHeight_ > 0 ? itemHeight_ : 12; // default row height

    switch (event.type)
    {
    case UIEventType::MouseDown:
        if (containsPoint(event.mouseX, event.mouseY))
        {
            int relY = event.mouseY - bounds_.y;
            int clickedRow = relY / ih + scrollOffset_;
            if (clickedRow >= 0 && clickedRow < static_cast<int>(items_.size()))
            {
                selectedIndex_ = clickedRow;
                if (onSelectionChanged_)
                    onSelectionChanged_(selectedIndex_);
            }
            return true;
        }
        return false;

    case UIEventType::KeyDown:
        if (!focused_)
            return false;
        if (event.scancode == SDL_SCANCODE_UP && selectedIndex_ > 0)
        {
            selectedIndex_--;
            if (selectedIndex_ < scrollOffset_)
                scrollOffset_ = selectedIndex_;
            if (onSelectionChanged_)
                onSelectionChanged_(selectedIndex_);
            return true;
        }
        if (event.scancode == SDL_SCANCODE_DOWN &&
            selectedIndex_ < static_cast<int>(items_.size()) - 1)
        {
            selectedIndex_++;
            int visible = visibleItemCount();
            if (selectedIndex_ >= scrollOffset_ + visible)
                scrollOffset_ = selectedIndex_ - visible + 1;
            if (onSelectionChanged_)
                onSelectionChanged_(selectedIndex_);
            return true;
        }
        return false;

    default:
        return false;
    }
}

void ListBox::render(graphics::IRenderer& renderer, const graphics::DebugText& debugText)
{
    if (!visible_)
        return;

    // Background
    renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 20, 20, 30, 200);

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    int ih = itemHeight_ > 0 ? itemHeight_ : debugText.lineHeight(textScale_) + 2;
    int visible = visibleItemCount();
    clampScroll();

    for (int i = 0; i < visible && (scrollOffset_ + i) < static_cast<int>(items_.size()); i++)
    {
        int itemIdx = scrollOffset_ + i;
        int y = bounds_.y + i * ih;

        // Selection highlight
        if (itemIdx == selectedIndex_)
        {
            renderer.drawFilledRect(bounds_.x, y, bounds_.width, ih, selR_, selG_, selB_, selA_);
        }

        // Text
        int textY = y + (ih - debugText.lineHeight(textScale_)) / 2;
        debugText.drawText(sdl, bounds_.x + 4, textY, textScale_, textR_, textG_, textB_,
                           items_[static_cast<size_t>(itemIdx)]);
    }

    // Border
    renderer.drawRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 100, 100, 120, 255);
}

int ListBox::visibleItemCount() const
{
    int ih = itemHeight_ > 0 ? itemHeight_ : 12;
    return bounds_.height > 0 ? bounds_.height / ih : 0;
}

void ListBox::clampScroll()
{
    int maxScroll = std::max(0, static_cast<int>(items_.size()) - visibleItemCount());
    scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll);
}

// ── ScrollBar ────────────────────────────────────────────────────────────────

void ScrollBar::setRange(int totalItems, int visibleItems)
{
    totalItems_ = std::max(0, totalItems);
    visibleItems_ = std::max(1, visibleItems);
    position_ = std::clamp(position_, 0, std::max(0, totalItems_ - visibleItems_));
}

void ScrollBar::setPosition(int pos)
{
    position_ = std::clamp(pos, 0, std::max(0, totalItems_ - visibleItems_));
}

void ScrollBar::setTrackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    trackR_ = r;
    trackG_ = g;
    trackB_ = b;
    trackA_ = a;
}

void ScrollBar::setThumbColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    thumbR_ = r;
    thumbG_ = g;
    thumbB_ = b;
    thumbA_ = a;
}

bool ScrollBar::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;

    switch (event.type)
    {
    case UIEventType::MouseDown:
        if (containsPoint(event.mouseX, event.mouseY))
        {
            auto thumb = thumbRect();
            if (thumb.contains(event.mouseX, event.mouseY))
            {
                dragging_ = true;
                dragStartY_ = event.mouseY;
                dragStartPos_ = position_;
            }
            else
            {
                // Click above/below thumb: page up/down
                if (event.mouseY < thumb.y)
                    setPosition(position_ - visibleItems_);
                else
                    setPosition(position_ + visibleItems_);
                if (onScroll_)
                    onScroll_(position_);
            }
            return true;
        }
        return false;

    case UIEventType::MouseMove:
        if (dragging_)
        {
            int trackH = bounds_.height;
            if (trackH <= 0 || totalItems_ <= visibleItems_)
                return true;

            int dy = event.mouseY - dragStartY_;
            int maxPos = totalItems_ - visibleItems_;
            int newPos = dragStartPos_ + dy * maxPos / trackH;
            setPosition(newPos);
            if (onScroll_)
                onScroll_(position_);
            return true;
        }
        return false;

    case UIEventType::MouseUp:
        if (dragging_)
        {
            dragging_ = false;
            return true;
        }
        return false;

    default:
        return false;
    }
}

void ScrollBar::render(graphics::IRenderer& renderer, const graphics::DebugText& /*text*/)
{
    if (!visible_)
        return;

    // Track
    renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, trackR_, trackG_,
                            trackB_, trackA_);

    // Thumb
    if (totalItems_ > visibleItems_)
    {
        auto thumb = thumbRect();
        uint8_t r = thumbR_, g = thumbG_, b = thumbB_, a = thumbA_;
        if (dragging_)
        {
            r = static_cast<uint8_t>(std::min(255, r + 40));
            g = static_cast<uint8_t>(std::min(255, g + 40));
            b = static_cast<uint8_t>(std::min(255, b + 40));
        }
        renderer.drawFilledRect(thumb.x, thumb.y, thumb.width, thumb.height, r, g, b, a);
    }
}

graphics::Rect ScrollBar::thumbRect() const
{
    if (totalItems_ <= 0 || totalItems_ <= visibleItems_)
        return {bounds_.x, bounds_.y, bounds_.width, bounds_.height};

    int trackH = bounds_.height;
    int thumbH = std::max(20, trackH * visibleItems_ / totalItems_);
    int maxPos = totalItems_ - visibleItems_;
    int thumbY = bounds_.y;
    if (maxPos > 0)
        thumbY += (trackH - thumbH) * position_ / maxPos;

    return {bounds_.x, thumbY, bounds_.width, thumbH};
}

} // namespace runeharbor::ui
