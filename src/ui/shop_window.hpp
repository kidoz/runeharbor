// SPDX-License-Identifier: MIT
//
// Shop / building service window. Mirrors the architecture of DialogueWindow:
// a self-contained modal driven from the in-game state, rendering with the same
// IRenderer + DebugText primitives. The transaction math is delegated to
// game::ShopSystem (see docs/re/29-shops-and-economy.md); this window is the
// player-facing surface for the economy loop.
//
// RE basis: the original engine opens this UI from EVT_SHOW_BUILDING (opcode
// 0x16) via Window Type 10 (FUN_004B30BA). RuneHarbor wires the same entry
// point: onShowBuilding -> SharedGameData::openShop -> InGameState::shopWindow_.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../formats/items_parser.hpp"
#include "../formats/two_d_events_parser.hpp"
#include "../game/building_type.hpp"
#include "../game/shop_system.hpp"
#include "../game/travel_destinations.hpp"

namespace runeharbor::graphics
{
class IRenderer;
class DebugText;
} // namespace runeharbor::graphics

namespace runeharbor::game
{
class Inventory;
class Party;
} // namespace runeharbor::game

namespace runeharbor::ui
{

// Which list the player is currently browsing inside the shop.
enum class ShopTab
{
    Buy = 0,  // items the shop sells
    Sell = 1, // the active character's backpack
};

// One selectable row in either list.
struct ShopListRow
{
    int itemId = 0;
    std::string name;
    int price = 0; // buy price (Buy tab) or sell credit (Sell tab)
    bool identified = false;
    bool broken = false;
    int backpackSlot = -1; // valid on the Sell tab only
};

class ShopWindow
{
  public:
    ShopWindow() = default;

    // Open the window for a building. Stocks the buy list from `items` filtered
    // by the building type; the sell list is rebuilt from the active character's
    // backpack on each render.
    void show(const formats::TwoDEventEntry& building,
              const std::vector<formats::ItemEntry>& items);

    void close();
    bool isOpen() const { return open_; }

    // Wire the game state in (called each frame while open, like other widgets).
    void setContext(game::Party* party, game::Inventory* inventory)
    {
        party_ = party;
        inventory_ = inventory;
    }

    // Status-line feedback (e.g. "Purchased Short Sword for 68 gold").
    void setStatusCallback(std::function<void(const std::string&)> cb)
    {
        onStatus_ = std::move(cb);
    }

    // Fired when the player picks a travel destination. The host advances the
    // game clock and triggers the map transition (the shop system itself can't
    // reach the transition machinery).
    struct TravelRequest
    {
        std::string mapName;
        float arrivalX = 0.0f;
        float arrivalY = 0.0f;
        float arrivalZ = 0.0f;
        float arrivalFacing = 0.0f;
        int travelDays = 1;
    };
    void setTravelRequestCallback(std::function<void(const TravelRequest&)> cb)
    {
        onTravelRequest_ = std::move(cb);
    }

    // Input. Returns true if the event was consumed (the window is modal).
    bool handleClick(int mouseX, int mouseY);
    bool handleKey(int scancode);

    // Render in 640x480 game coordinates.
    void render(graphics::IRenderer& renderer, const graphics::DebugText& debugText, int viewportW,
                int viewportH);

  private:
    // Layout rectangles for hit-testing, rebuilt each render.
    struct ButtonRect
    {
        int x = 0, y = 0, w = 0, h = 0;
        std::string label;
    };

    bool open_ = false;
    game::BuildingType buildingType_ = game::BuildingType::None;
    game::ShopFamily family_ = game::ShopFamily::None;
    std::string shopName_;
    std::string proprietor_;
    float buyMultiplier_ = 1.0f;

    ShopTab activeTab_ = ShopTab::Buy;
    int listSelection_ = 0; // item list (Buy/Sell) OR temple member index
    int scrollOffset_ = 0;

    std::vector<ShopListRow> buyList_;
    std::vector<ShopListRow> sellList_;

    game::Party* party_ = nullptr;
    game::Inventory* inventory_ = nullptr;
    game::ShopSystem shop_;

    std::vector<ButtonRect> buttonRects_;
    std::function<void(const std::string&)> onStatus_;

    // Builds the per-shop buy list by filtering the item table on equipStat.
    void buildBuyList(const std::vector<formats::ItemEntry>& items);
    void rebuildSellList();
    void doBuy();
    void doSell();
    // Temple service handlers.
    void doHeal();
    void doResurrect();
    void doDonate();
    // Training + travel handlers.
    void doTrain();
    void doTravel();
    void refreshLists();
    void clampSelection();

    // Family-mode render helpers (party roster / list + service buttons).
    void renderTemple(graphics::IRenderer& renderer, const graphics::DebugText& debugText);
    void renderTraining(graphics::IRenderer& renderer, const graphics::DebugText& debugText);
    void renderTravel(graphics::IRenderer& renderer, const graphics::DebugText& debugText);
    void renderSimpleService(graphics::IRenderer& renderer, const graphics::DebugText& debugText);
    void doDeposit();
    void doWithdraw();
    void doRestInn();
    void doLearnSpell();

    // Travel destinations offered by the current building (built on show()).
    std::vector<game::TravelDestination> travelDestinations_;
    std::function<void(const TravelRequest&)> onTravelRequest_;

    // True if an item's equipStat is sold by the given building type.
    static bool itemFitsShop(game::BuildingType type, const std::string& equipStat);
};

} // namespace runeharbor::ui
