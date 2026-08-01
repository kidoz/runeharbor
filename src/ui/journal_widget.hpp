// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../formats/autonote_parser.hpp"
#include "../formats/awards_parser.hpp"
#include "../game/game_world.hpp"
#include "../game/quest_log.hpp"
#include "widgets.hpp"

namespace runeharbor::ui
{

// Quest / journal screen (the MM7 "Quest" book, opened with Q). Renders the
// active and completed quest lists, the selected quest's description, and a
// chronological journal log. See docs/quest-journal.md.
class JournalWidget : public Widget
{
  public:
    JournalWidget();

    // Active tab: Quests (default), Autonotes, Awards.
    enum class JournalTab : uint8_t
    {
        Quests = 0,
        Autonotes = 1,
        Awards = 2
    };

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup) { textureLookup_ = lookup; }

    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setQuestLog(game::QuestLog* questLog) { questLog_ = questLog; }
    void setAutonoteCatalog(const std::vector<formats::AutonoteEntry>* catalog)
    {
        autonoteCatalog_ = catalog;
    }
    void setAwardCatalog(const std::vector<formats::AwardEntry>* catalog)
    {
        awardCatalog_ = catalog;
    }

    // Open to a specific tab (e.g. the A key opens straight to Autonotes).
    void setActiveTab(JournalTab tab)
    {
        activeTab_ = tab;
        selected_ = 0;
    }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;
    bool handleEvent(const UIEvent& event) override;

    void setBackground(void* tex, int w, int h)
    {
        bgTexture_ = tex;
        bgW_ = w;
        bgH_ = h;
    }

  private:
    // A flattened, display-ordered quest row (active first, then completed).
    struct QuestRow
    {
        const game::QuestEntry* entry = nullptr;
        std::string label;
        bool active = false;
    };

    void rebuildRows();
    std::string statusLabel(game::QuestState state) const;
    // Row count of the active tab's list (Quests -> rows_, Autonotes ->
    // autonoteCatalog_, Awards -> awardCatalog_). Used so keyboard navigation
    // and render clamping agree on how far the selection can move.
    int currentListSize() const;

    game::GameWorld* gameWorld_ = nullptr;
    game::QuestLog* questLog_ = nullptr;
    const std::vector<formats::AutonoteEntry>* autonoteCatalog_ = nullptr;
    const std::vector<formats::AwardEntry>* awardCatalog_ = nullptr;
    TextureLookup textureLookup_;

    JournalTab activeTab_ = JournalTab::Quests;

    std::vector<QuestRow> rows_;
    std::vector<int> rowY_; // screen Y per row for hit-testing
    int rowHeight_ = 16;
    int selected_ = 0;

    struct CachedTexture
    {
        void* tex;
        int w, h;
    };
    std::unordered_map<std::string, CachedTexture> textureCache_;

    void* bgTexture_ = nullptr;
    int bgW_ = 0, bgH_ = 0;
};

} // namespace runeharbor::ui
