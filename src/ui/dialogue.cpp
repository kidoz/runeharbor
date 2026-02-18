// SPDX-License-Identifier: MIT
#include "dialogue.hpp"

#include <SDL3/SDL_scancode.h>

#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"

namespace runeharbor::ui
{

namespace
{
// Dialogue window layout (proportional to viewport)
constexpr float kWindowWidthRatio = 0.6f;
constexpr float kWindowHeightRatio = 0.45f;
constexpr int kPadding = 12;
constexpr int kPortraitSize = 64;
constexpr int kTextScale = 1;
constexpr int kNameScale = 2;
constexpr int kChoiceScale = 1;

// Word-wrap text into lines of maxWidth pixels
std::vector<std::string> wordWrap(const std::string& text, int maxWidthChars)
{
    std::vector<std::string> lines;
    if (text.empty() || maxWidthChars <= 0)
        return lines;

    std::string current;
    int col = 0;

    for (size_t i = 0; i < text.size(); i++)
    {
        char c = text[i];
        if (c == '\n')
        {
            lines.push_back(current);
            current.clear();
            col = 0;
            continue;
        }

        current += c;
        col++;

        if (col >= maxWidthChars && c == ' ')
        {
            lines.push_back(current);
            current.clear();
            col = 0;
        }
    }
    if (!current.empty())
        lines.push_back(current);

    return lines;
}
} // namespace

void DialogueWindow::show(const std::string& npcName, const std::string& text,
                          const std::vector<DialogueChoice>& choices)
{
    npcName_ = npcName;
    text_ = text;
    choices_ = choices;
    open_ = true;
    hoveredChoice_ = -1;
    selectedChoice_ = 0;
    choiceRects_.clear();
}

void DialogueWindow::close()
{
    open_ = false;
    npcName_.clear();
    text_.clear();
    choices_.clear();
    choiceRects_.clear();
}

bool DialogueWindow::handleClick(int mouseX, int mouseY)
{
    if (!open_)
        return false;

    // Check if clicked on a choice
    for (const auto& cr : choiceRects_)
    {
        if (mouseX >= cr.x && mouseX < cr.x + cr.w && mouseY >= cr.y && mouseY < cr.y + cr.h)
        {
            if (onChoice_)
                onChoice_(cr.choiceId);
            close();
            return true;
        }
    }

    // If no choices, clicking anywhere dismisses
    if (choices_.empty())
    {
        if (onDismiss_)
            onDismiss_();
        close();
        return true;
    }

    return true; // Consume click even if missed choices (modal)
}

bool DialogueWindow::handleKey(int scancode)
{
    if (!open_)
        return false;

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        if (onDismiss_)
            onDismiss_();
        close();
        return true;
    }

    if (choices_.empty())
    {
        if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_SPACE)
        {
            if (onDismiss_)
                onDismiss_();
            close();
            return true;
        }
        return true;
    }

    // Navigate choices
    if (scancode == SDL_SCANCODE_UP && selectedChoice_ > 0)
    {
        selectedChoice_--;
        return true;
    }
    if (scancode == SDL_SCANCODE_DOWN && selectedChoice_ < static_cast<int>(choices_.size()) - 1)
    {
        selectedChoice_++;
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_SPACE)
    {
        if (selectedChoice_ >= 0 && selectedChoice_ < static_cast<int>(choices_.size()))
        {
            if (onChoice_)
                onChoice_(choices_[static_cast<size_t>(selectedChoice_)].id);
            close();
        }
        return true;
    }

    return true; // Modal - consume all keys
}

void DialogueWindow::setPortraitTexture(void* tex, int w, int h)
{
    portrait_ = tex;
    portraitW_ = w;
    portraitH_ = h;
}

void DialogueWindow::render(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                            int viewportW, int viewportH)
{
    if (!open_)
        return;

    // Calculate window dimensions
    int winW = static_cast<int>(viewportW * kWindowWidthRatio);
    int winH = static_cast<int>(viewportH * kWindowHeightRatio);
    int winX = (viewportW - winW) / 2;
    int winY = (viewportH - winH) / 2;

    // Dim background
    renderer.drawFilledRect(0, 0, viewportW, viewportH, 0, 0, 0, 120);

    // Window background
    renderer.drawFilledRect(winX, winY, winW, winH, 20, 20, 35, 230);
    renderer.drawRect(winX, winY, winW, winH, 120, 100, 60, 255);
    renderer.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, 80, 70, 40, 200);

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    int contentX = winX + kPadding;
    int contentY = winY + kPadding;
    int textAreaX = contentX;

    // Portrait
    if (portrait_)
    {
        renderer.renderTexture(portrait_, contentX, contentY, kPortraitSize, kPortraitSize);
        renderer.drawRect(contentX, contentY, kPortraitSize, kPortraitSize, 100, 100, 120, 255);
        textAreaX = contentX + kPortraitSize + kPadding;
    }
    else
    {
        // Placeholder portrait
        renderer.drawFilledRect(contentX, contentY, kPortraitSize, kPortraitSize, 40, 40, 55, 200);
        renderer.drawRect(contentX, contentY, kPortraitSize, kPortraitSize, 80, 80, 100, 255);
        debugText.drawText(sdl, contentX + 12, contentY + 24, 1, 100, 100, 120, "NPC");
        textAreaX = contentX + kPortraitSize + kPadding;
    }

    // NPC name
    if (!npcName_.empty())
    {
        debugText.drawText(sdl, textAreaX, contentY, kNameScale, 255, 220, 120, npcName_);
        contentY += debugText.lineHeight(kNameScale) + 4;
    }
    else
    {
        contentY += kPortraitSize + kPadding;
    }

    // Separator line
    int sepY = contentY + (portrait_ ? 0 : 0);
    if (portrait_)
        sepY = winY + kPadding + kPortraitSize + kPadding / 2;
    renderer.drawFilledRect(winX + kPadding, sepY, winW - kPadding * 2, 1, 80, 70, 40, 200);
    contentY = sepY + kPadding / 2;

    // Dialogue text (word-wrapped)
    int textAreaW = winW - kPadding * 2;
    int charW = debugText.charWidth(kTextScale);
    int maxChars = charW > 0 ? textAreaW / charW : 80;
    auto lines = wordWrap(text_, maxChars);

    for (const auto& line : lines)
    {
        if (contentY + debugText.lineHeight(kTextScale) > winY + winH - kPadding)
            break; // Out of space
        debugText.drawText(sdl, winX + kPadding, contentY, kTextScale, 220, 220, 220, line);
        contentY += debugText.lineHeight(kTextScale);
    }

    // Choices
    choiceRects_.clear();
    if (!choices_.empty())
    {
        contentY += kPadding;
        renderer.drawFilledRect(winX + kPadding, contentY - 2, winW - kPadding * 2, 1, 60, 60, 80,
                                200);
        contentY += 4;

        for (int i = 0; i < static_cast<int>(choices_.size()); i++)
        {
            int choiceY = contentY;
            int choiceH = debugText.lineHeight(kChoiceScale) + 4;

            if (choiceY + choiceH > winY + winH - kPadding)
                break;

            bool selected = (i == selectedChoice_);
            if (selected)
            {
                renderer.drawFilledRect(winX + kPadding, choiceY, winW - kPadding * 2, choiceH, 60,
                                        60, 90, 180);
            }

            std::string prefix = selected ? "> " : "  ";
            uint8_t r = selected ? 255 : 180;
            uint8_t g = selected ? 240 : 180;
            uint8_t b = selected ? 180 : 200;
            debugText.drawText(sdl, winX + kPadding + 4, choiceY + 2, kChoiceScale, r, g, b,
                               prefix + choices_[static_cast<size_t>(i)].text);

            choiceRects_.push_back({winX + kPadding, choiceY, winW - kPadding * 2, choiceH,
                                    choices_[static_cast<size_t>(i)].id});
            contentY += choiceH;
        }
    }
    else
    {
        // "Click to continue" hint
        int hintY = winY + winH - kPadding - debugText.lineHeight(1);
        debugText.drawText(sdl, winX + winW / 2 - 60, hintY, 1, 140, 140, 160,
                           "[Click to continue]");
    }
}

} // namespace runeharbor::ui
