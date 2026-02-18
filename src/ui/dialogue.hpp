// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace runeharbor::graphics
{
class IRenderer;
class DebugText;
} // namespace runeharbor::graphics

namespace runeharbor::ui
{

/// A response choice in a dialogue
struct DialogueChoice
{
    int id = 0;
    std::string text;
};

/// NPC dialogue window.
/// Displays speaker name, dialogue text, portrait placeholder, and response choices.
/// Operates in screen coordinates (already scaled).
class DialogueWindow
{
  public:
    DialogueWindow() = default;

    /// Show a dialogue with NPC name, text, and optional choices
    void show(const std::string& npcName, const std::string& text,
              const std::vector<DialogueChoice>& choices = {});

    /// Close the dialogue
    void close();

    /// Is the dialogue visible?
    bool isOpen() const { return open_; }

    /// Set callback for when a choice is selected (passes choice ID)
    void setOnChoice(std::function<void(int)> cb) { onChoice_ = std::move(cb); }

    /// Set callback for when dialogue is dismissed (no choices / single OK)
    void setOnDismiss(std::function<void()> cb) { onDismiss_ = std::move(cb); }

    /// Handle mouse click (screen coords). Returns true if consumed.
    bool handleClick(int mouseX, int mouseY);

    /// Handle key press. Returns true if consumed.
    bool handleKey(int scancode);

    /// Render the dialogue window
    void render(graphics::IRenderer& renderer, const graphics::DebugText& debugText, int viewportW,
                int viewportH);

    /// Set NPC portrait texture (non-owning)
    void setPortraitTexture(void* tex, int w, int h);

  private:
    struct ChoiceRect
    {
        int x, y, w, h;
        int choiceId;
    };

    bool open_ = false;
    std::string npcName_;
    std::string text_;
    std::vector<DialogueChoice> choices_;
    int hoveredChoice_ = -1;
    int selectedChoice_ = 0;

    // Portrait (non-owning)
    void* portrait_ = nullptr;
    int portraitW_ = 0;
    int portraitH_ = 0;

    // Cached layout (rebuilt on render)
    std::vector<ChoiceRect> choiceRects_;

    std::function<void(int)> onChoice_;
    std::function<void()> onDismiss_;
};

} // namespace runeharbor::ui
