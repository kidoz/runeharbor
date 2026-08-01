// SPDX-License-Identifier: MIT
#include "journal_widget.hpp"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <format>

#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

JournalWidget::JournalWidget() {}

namespace
{
// Word-wrap a string to a max character width (mirrors the dialogue helper).
std::vector<std::string> wrapText(const std::string& text, int maxWidthChars)
{
    std::vector<std::string> lines;
    std::string word;
    std::string line;
    for (char c : text)
    {
        if (c == ' ' || c == '\n')
        {
            if (!line.empty() && static_cast<int>(line.size() + word.size() + 1) > maxWidthChars)
            {
                lines.push_back(line);
                line.clear();
            }
            if (!line.empty())
                line += ' ';
            line += word;
            word.clear();
            if (c == '\n')
            {
                lines.push_back(line);
                line.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        if (!line.empty() && static_cast<int>(line.size() + word.size() + 1) > maxWidthChars)
        {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty())
            line += ' ';
        line += word;
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}
} // namespace

std::string JournalWidget::statusLabel(game::QuestState state) const
{
    switch (state)
    {
    case game::QuestState::Active:
        return "(active)";
    case game::QuestState::Completed:
        return "(completed)";
    case game::QuestState::Failed:
        return "(failed)";
    case game::QuestState::Unknown:
        return "";
    }
    return "";
}

void JournalWidget::rebuildRows()
{
    rows_.clear();
    if (questLog_ == nullptr)
    {
        return;
    }
    // Active quests first, then completed, then failed (matches a typical
    // journal reading order; MM7 shows a flat acquired list).
    auto append = [&](const std::vector<const game::QuestEntry*>& entries, bool active)
    {
        for (const auto* e : entries)
        {
            QuestRow row;
            row.entry = e;
            row.active = active;
            const std::string owner = e->owner.empty() ? "" : (e->owner + ": ");
            row.label = owner + e->text;
            rows_.push_back(std::move(row));
        }
    };
    append(questLog_->getActiveQuests(), true);
    append(questLog_->getCompletedQuests(), false);
    // Failed quests (no dedicated accessor — filter getAllQuests).
    for (const auto* e : questLog_->getAllQuests())
    {
        if (e->state == game::QuestState::Failed)
        {
            QuestRow row;
            row.entry = e;
            row.active = false;
            row.label = (e->owner.empty() ? "" : e->owner + ": ") + e->text;
            rows_.push_back(std::move(row));
        }
    }
}

void JournalWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
{
    if (!visible_)
        return;

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    // Chrome.
    if (bgTexture_)
    {
        SDL_FRect dst = {static_cast<float>(bounds_.x), static_cast<float>(bounds_.y),
                         static_cast<float>(bounds_.width), static_cast<float>(bounds_.height)};
        SDL_RenderTexture(sdl, static_cast<SDL_Texture*>(bgTexture_), nullptr, &dst);
    }
    else
    {
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 30, 26, 20,
                                240);
    }
    renderer.drawRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 130, 108, 60, 255);
    renderer.drawRect(bounds_.x + 1, bounds_.y + 1, bounds_.width - 2, bounds_.height - 2, 90, 78,
                      44, 210);

    int x = bounds_.x + 20;
    int y = bounds_.y + 16;

    text.drawText(sdl, x, y, 2, 255, 220, 120, "Journal");
    y += 30;

    // Tab buttons: Quests / Autonotes / Awards.
    rowY_.clear();
    const char* tabLabels[] = {"Quests", "Autonotes", "Awards"};
    for (int t = 0; t < 3; t++)
    {
        const int tabW = 72;
        const int tabX = x + t * (tabW + 6);
        const bool active = (static_cast<int>(activeTab_) == t);
        renderer.drawFilledRect(tabX, y, tabW, 20, active ? 80 : 40, active ? 70 : 35,
                                active ? 30 : 20, 220);
        renderer.drawRect(tabX, y, tabW, 20, 130, 108, 60, 255);
        text.drawText(sdl, tabX + 8, y + 3, 1, active ? 255 : 180, active ? 230 : 180,
                      active ? 120 : 150, tabLabels[t]);
    }
    y += 26;
    renderer.drawFilledRect(bounds_.x + 16, y, bounds_.width - 32, 1, 110, 90, 50, 220);
    y += 6;

    const int listW = bounds_.width * 2 / 5;
    const int detailX = bounds_.x + listW + 32;
    const int detailW = bounds_.width - listW - 60;
    const int maxListY = bounds_.y + bounds_.height - 30;

    if (activeTab_ == JournalTab::Quests)
    {
        rebuildRows();
        if (questLog_ == nullptr)
        {
            text.drawText(sdl, x, y, 1, 200, 200, 200, "(quest log unavailable)");
        }
        else if (rows_.empty())
        {
            text.drawText(sdl, x, y, 1, 200, 200, 200,
                          "No quests yet. Explore and talk to NPCs to find them.");
        }
        else
        {
            const std::string counts =
                std::format("{} active   {} completed", questLog_->activeQuestCount(),
                            questLog_->completedQuestCount());
            text.drawText(sdl, x, y, 1, 200, 200, 160, counts);
            y += rowHeight_ + 2;
            if (selected_ >= static_cast<int>(rows_.size()))
                selected_ = 0;
            for (int i = 0; i < static_cast<int>(rows_.size()); i++)
            {
                if (y + rowHeight_ > maxListY)
                    break;
                const auto& row = rows_[i];
                if (i == selected_)
                    renderer.drawFilledRect(bounds_.x + 16, y - 1, listW, rowHeight_, 80, 70, 30,
                                            220);
                text.drawText(sdl, x, y, 1, row.active ? 240 : 170, row.active ? 230 : 170,
                              row.active ? 170 : 150,
                              row.label.size() > 34 ? row.label.substr(0, 33) + "..." : row.label);
                rowY_.push_back(y);
                y += rowHeight_;
            }
            // Detail pane.
            if (selected_ >= 0 && selected_ < static_cast<int>(rows_.size()))
            {
                const auto& row = rows_[selected_];
                int dy = bounds_.y + 72;
                const std::string heading =
                    std::format("{} {}", row.entry->text, statusLabel(row.entry->state));
                for (const auto& ln : wrapText(heading, detailW / 8))
                {
                    text.drawText(sdl, detailX, dy, 1, 230, 230, 230, ln);
                    dy += rowHeight_;
                }
                if (!row.entry->owner.empty())
                {
                    dy += 4;
                    text.drawText(sdl, detailX, dy, 1, 180, 180, 200,
                                  std::format("Given by: {}", row.entry->owner));
                }
            }
        }
    }
    else if (activeTab_ == JournalTab::Autonotes)
    {
        if (autonoteCatalog_ == nullptr || autonoteCatalog_->empty())
        {
            text.drawText(sdl, x, y, 1, 200, 200, 200, "(no autonotes available)");
        }
        else
        {
            if (selected_ >= static_cast<int>(autonoteCatalog_->size()))
                selected_ = 0;
            for (size_t i = 0; i < autonoteCatalog_->size(); i++)
            {
                if (y + rowHeight_ > maxListY)
                    break;
                const auto& note = (*autonoteCatalog_)[i];
                if (static_cast<int>(i) == selected_)
                    renderer.drawFilledRect(bounds_.x + 16, y - 1, listW, rowHeight_, 80, 70, 30,
                                            220);
                std::string label = note.autonoteText;
                if (label.size() > 34)
                    label = label.substr(0, 33) + "...";
                text.drawText(sdl, x, y, 1, static_cast<int>(i) == selected_ ? 250 : 200,
                              static_cast<int>(i) == selected_ ? 230 : 200,
                              static_cast<int>(i) == selected_ ? 170 : 170, label);
                rowY_.push_back(y);
                y += rowHeight_;
            }
            // Detail pane.
            if (selected_ >= 0 && selected_ < static_cast<int>(autonoteCatalog_->size()))
            {
                int dy = bounds_.y + 72;
                for (const auto& ln :
                     wrapText((*autonoteCatalog_)[selected_].autonoteText, detailW / 8))
                {
                    text.drawText(sdl, detailX, dy, 1, 230, 230, 230, ln);
                    dy += rowHeight_;
                }
            }
        }
    }
    else // Awards
    {
        if (awardCatalog_ == nullptr || awardCatalog_->empty())
        {
            text.drawText(sdl, x, y, 1, 200, 200, 200, "(no awards available)");
        }
        else
        {
            if (selected_ >= static_cast<int>(awardCatalog_->size()))
                selected_ = 0;
            for (size_t i = 0; i < awardCatalog_->size(); i++)
            {
                if (y + rowHeight_ > maxListY)
                    break;
                const auto& award = (*awardCatalog_)[i];
                if (static_cast<int>(i) == selected_)
                    renderer.drawFilledRect(bounds_.x + 16, y - 1, listW, rowHeight_, 80, 70, 30,
                                            220);
                std::string label = award.awardText;
                if (label.size() > 34)
                    label = label.substr(0, 33) + "...";
                text.drawText(sdl, x, y, 1, static_cast<int>(i) == selected_ ? 250 : 200,
                              static_cast<int>(i) == selected_ ? 230 : 200,
                              static_cast<int>(i) == selected_ ? 170 : 170, label);
                rowY_.push_back(y);
                y += rowHeight_;
            }
            // Detail pane.
            if (selected_ >= 0 && selected_ < static_cast<int>(awardCatalog_->size()))
            {
                int dy = bounds_.y + 72;
                for (const auto& ln : wrapText((*awardCatalog_)[selected_].awardText, detailW / 8))
                {
                    text.drawText(sdl, detailX, dy, 1, 230, 230, 230, ln);
                    dy += rowHeight_;
                }
                if (!(*awardCatalog_)[selected_].notes.empty())
                {
                    dy += 4;
                    for (const auto& ln : wrapText((*awardCatalog_)[selected_].notes, detailW / 8))
                    {
                        text.drawText(sdl, detailX, dy, 1, 200, 200, 170, ln);
                        dy += rowHeight_;
                    }
                }
            }
        }
    }

    // Footer hint.
    const int hintY = bounds_.y + bounds_.height - 18;
    text.drawText(sdl, x, hintY, 1, 160, 160, 170,
                  "Up/Dn: select   Click tab to switch   Esc: close");
}

bool JournalWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;
    if (event.type == UIEventType::MouseDown && bounds_.contains(event.mouseX, event.mouseY))
    {
        // Tab buttons (3 tabs at y ~46..66 from bounds top).
        const int tabY = bounds_.y + 46;
        if (event.mouseY >= tabY && event.mouseY < tabY + 20)
        {
            for (int t = 0; t < 3; t++)
            {
                const int tabX = bounds_.x + 20 + t * 78;
                if (event.mouseX >= tabX && event.mouseX < tabX + 72)
                {
                    activeTab_ = static_cast<JournalTab>(t);
                    selected_ = 0;
                    return true;
                }
            }
        }
        for (size_t i = 0; i < rowY_.size(); i++)
        {
            if (event.mouseY >= rowY_[i] && event.mouseY < rowY_[i] + rowHeight_)
            {
                selected_ = static_cast<int>(i);
                return true;
            }
        }
        return true; // consume clicks inside the panel
    }
    if (event.type == UIEventType::KeyDown)
    {
        if (event.scancode == SDL_SCANCODE_UP)
        {
            if (selected_ > 0)
                selected_--;
            return true;
        }
        if (event.scancode == SDL_SCANCODE_DOWN)
        {
            if (selected_ < static_cast<int>(rows_.size()) - 1)
                selected_++;
            return true;
        }
        if (event.scancode == SDL_SCANCODE_ESCAPE)
        {
            setVisible(false);
            return true;
        }
    }
    return false;
}

} // namespace runeharbor::ui
