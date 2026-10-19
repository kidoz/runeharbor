// SPDX-License-Identifier: MIT
#include "save_load_widget.hpp"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <format>

#include <cstring>

#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

namespace
{
std::string fixedString(const char* data, size_t size)
{
    return std::string(data, ::strnlen(data, size));
}

std::string truncate(std::string value, size_t limit)
{
    if (value.size() > limit)
    {
        value.resize(limit);
    }
    return value;
}
} // namespace

void SaveLoadWidget::open(Mode mode)
{
    mode_ = mode;
    selected_ = 0;
    pending_ = {};
    overwriteArmed_ = false;
    statusMessage_.clear();
    refresh();
    setVisible(true);
}

void SaveLoadWidget::refresh()
{
    slots_ = saveGame_ ? saveGame_->listSlots() : std::vector<game::SaveSlotInfo>{};
    if (slots_.empty())
    {
        selected_ = 0;
    }
    else
    {
        selected_ = std::clamp(selected_, 0, static_cast<int>(slots_.size()) - 1);
    }
}

SaveLoadWidget::Action SaveLoadWidget::takeAction()
{
    const Action action = pending_;
    pending_ = {};
    return action;
}

bool SaveLoadWidget::slotExists(int index) const
{
    return index >= 0 && index < entryCount() && slots_[static_cast<size_t>(index)].exists;
}

int SaveLoadWidget::pageStart() const
{
    return (selected_ / kVisibleRows) * kVisibleRows;
}

void SaveLoadWidget::moveSelection(int delta)
{
    const int count = entryCount();
    if (count <= 0)
    {
        return;
    }
    int target = (selected_ + delta) % count;
    if (target < 0)
    {
        target += count;
    }
    if (target != selected_)
    {
        overwriteArmed_ = false;
        statusMessage_.clear();
    }
    selected_ = target;
}

void SaveLoadWidget::confirmSelection()
{
    if (slots_.empty())
    {
        statusMessage_ = "No save slots available.";
        return;
    }
    if (mode_ == Mode::Load && !slotExists(selected_))
    {
        statusMessage_ = "No save in this slot.";
        return;
    }
    if (mode_ == Mode::Save && slotExists(selected_) && !overwriteArmed_)
    {
        overwriteArmed_ = true;
        statusMessage_ =
            std::format("Slot {} has a save. Press Enter again to overwrite.", selected_ + 1);
        return;
    }
    overwriteArmed_ = false;
    statusMessage_.clear();
    pending_ = {mode_ == Mode::Save ? ActionType::Save : ActionType::Load, selected_};
}

bool SaveLoadWidget::handleEvent(const UIEvent& event)
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
        case SDL_SCANCODE_PAGEUP:
            moveSelection(-kVisibleRows);
            return true;
        case SDL_SCANCODE_PAGEDOWN:
            moveSelection(kVisibleRows);
            return true;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            confirmSelection();
            return true;
        case SDL_SCANCODE_ESCAPE:
            setVisible(false);
            return true;
        default:
            break;
        }

        int pageOffset = -1;
        if (event.scancode >= SDL_SCANCODE_1 && event.scancode <= SDL_SCANCODE_9)
        {
            pageOffset = event.scancode - SDL_SCANCODE_1;
        }
        else if (event.scancode == SDL_SCANCODE_0)
        {
            pageOffset = 9;
        }
        if (pageOffset >= 0)
        {
            const int target = pageStart() + pageOffset;
            if (target < entryCount())
            {
                if (target != selected_)
                {
                    overwriteArmed_ = false;
                    statusMessage_.clear();
                }
                selected_ = target;
            }
            return true;
        }
        return false;
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
                const int target = pageStart() + static_cast<int>(i);
                if (target < entryCount())
                {
                    if (target != selected_)
                    {
                        overwriteArmed_ = false;
                        statusMessage_.clear();
                    }
                    selected_ = target;
                }
                return true;
            }
        }
        if (confirmRect_.contains(event.mouseX, event.mouseY))
        {
            confirmSelection();
            return true;
        }
        if (cancelRect_.contains(event.mouseX, event.mouseY))
        {
            setVisible(false);
            return true;
        }
        // Modal: swallow clicks on the rest of the screen.
        return true;
    }

    return false;
}

void SaveLoadWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
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

    const int panelW = std::min(bounds_.width - 32, 520);
    const int panelH = std::min(bounds_.height - 32, 420);
    const int px = bounds_.x + (bounds_.width - panelW) / 2;
    const int py = bounds_.y + (bounds_.height - panelH) / 2;

    renderer.drawFilledRect(px, py, panelW, panelH, 30, 26, 20, 245);
    renderer.drawRect(px, py, panelW, panelH, 130, 108, 60, 255);
    renderer.drawRect(px + 1, py + 1, panelW - 2, panelH - 2, 90, 78, 44, 210);

    const bool saveMode = mode_ == Mode::Save;
    text.drawText(sdl, px + 20, py + 14, 2, 255, 220, 120, saveMode ? "Save Game" : "Load Game");
    text.drawText(sdl, px + panelW - 150, py + 22, 1, 170, 170, 160,
                  saveMode ? "F5: close" : "F8: close");

    // Column header.
    text.drawText(sdl, px + 20, py + 52, 1, 170, 160, 120, "Slot  Party / Level        Location");
    renderer.drawFilledRect(px + 16, py + 70, panelW - 32, 1, 110, 90, 50, 220);

    rowRects_.clear();
    rowRects_.reserve(kVisibleRows);
    const int rowH = 22;
    const int rowsTop = py + 78;
    const int start = pageStart();
    for (int i = 0; i < kVisibleRows; ++i)
    {
        const int slotIndex = start + i;
        const int rowY = rowsTop + i * rowH;
        rowRects_.push_back({px + 16, rowY, panelW - 32, rowH});

        if (slotIndex >= entryCount())
        {
            continue;
        }

        const auto& slot = slots_[static_cast<size_t>(slotIndex)];
        const bool selected = slotIndex == selected_;
        if (selected)
        {
            renderer.drawFilledRect(px + 16, rowY, panelW - 32, rowH, 80, 70, 30, 220);
        }

        std::string label;
        if (slot.exists)
        {
            const std::string leader =
                truncate(fixedString(slot.header.partyLeader, sizeof(slot.header.partyLeader)), 14);
            const std::string map =
                truncate(fixedString(slot.header.mapName, sizeof(slot.header.mapName)), 16);
            label = std::format("{:02d}    {:<14}  Lv {:>2}  {}", slotIndex + 1, leader,
                                slot.header.partyLevel, map);
        }
        else
        {
            label = std::format("{:02d}    (empty)", slotIndex + 1);
        }

        const int r = selected ? 250 : (slot.exists ? 205 : 130);
        const int g = selected ? 230 : (slot.exists ? 195 : 130);
        const int b = selected ? 170 : (slot.exists ? 165 : 135);
        text.drawText(sdl, px + 24, rowY + 4, 1, r, g, b, label);
    }

    // Page and status line.
    const int page = (entryCount() > 0 ? selected_ / kVisibleRows : 0) + 1;
    text.drawText(sdl, px + 20, py + panelH - 64, 1, 170, 170, 150,
                  std::format("Page {} of {}   Up/Dn: select   PgUp/PgDn: page", page,
                              (entryCount() + kVisibleRows - 1) / kVisibleRows));
    if (!statusMessage_.empty())
    {
        text.drawText(sdl, px + 20, py + panelH - 46, 1, 230, 150, 130, statusMessage_);
    }

    // Buttons.
    const int buttonY = py + panelH - 32;
    cancelRect_ = {px + panelW - 20 - 90, buttonY, 90, 24};
    confirmRect_ = {cancelRect_.x - 8 - 100, buttonY, 100, 24};

    auto drawButton = [&](const graphics::Rect& rect, const char* label, bool primary)
    {
        renderer.drawFilledRect(rect.x, rect.y, rect.width, rect.height, primary ? 75 : 50,
                                primary ? 60 : 42, primary ? 25 : 22, 230);
        renderer.drawRect(rect.x, rect.y, rect.width, rect.height, 130, 108, 60, 255);
        const int labelScale = 1;
        const int labelWidth = text.charWidth(labelScale) * static_cast<int>(::strlen(label));
        text.drawText(sdl, rect.x + (rect.width - labelWidth) / 2, rect.y + 5, labelScale,
                      primary ? 255 : 200, primary ? 230 : 190, primary ? 150 : 170, label);
    };
    drawButton(confirmRect_, saveMode ? "Save" : "Load", true);
    drawButton(cancelRect_, "Cancel", false);
}

} // namespace runeharbor::ui
