// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "widgets.hpp"

namespace runeharbor::ui
{

/// Small modal choice menu used by utility spells (Town Portal destinations,
/// Lloyd's Beacon set/recall). Selections are reported via takeSelection() so
/// the owning state can perform the actual travel.
class ChoiceListWidget : public Widget
{
  public:
    struct Choice
    {
        std::string label;
        int id = 0;
    };

    void open(std::string title, std::vector<Choice> choices);

    /// Returns the chosen id and clears the pending selection, or -1.
    int takeSelection();
    int selectedIndex() const { return selected_; }
    int selectedId() const;
    bool hasChoices() const { return !choices_.empty(); }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;
    bool handleEvent(const UIEvent& event) override;

  private:
    void moveSelection(int delta);
    void confirmSelection();

    std::string title_;
    std::vector<Choice> choices_;
    int selected_ = 0;
    int pendingId_ = -1;
    std::vector<graphics::Rect> rowRects_;
    graphics::Rect cancelRect_;
};

} // namespace runeharbor::ui
