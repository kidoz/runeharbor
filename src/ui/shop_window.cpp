// SPDX-License-Identifier: MIT
#include "shop_window.hpp"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <format>

#include "../game/inventory.hpp"
#include "../game/party.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../util/string_utils.hpp"

namespace runeharbor::ui
{

namespace
{

// Shop window layout (in 640x480 game coordinates).
constexpr int kWindowX = 40;
constexpr int kWindowY = 30;
constexpr int kWindowW = 560;
constexpr int kWindowH = 420;
constexpr int kPadding = 10;
constexpr int kTitleScale = 2;
constexpr int kTextScale = 1;
constexpr int kRowHeight = 14;
constexpr int kMaxVisibleRows = 14;
constexpr int kButtonHeight = 22;
constexpr int kButtonWidth = 110;

bool iequals(std::string_view a, std::string_view b)
{
    return util::toLower(a) == util::toLower(b);
}

// Short display label for an active condition (used by the temple roster).
std::string conditionLabel(game::ConditionIndex c)
{
    switch (c)
    {
    case game::ConditionIndex::Cursed:
        return "Cursed";
    case game::ConditionIndex::Weak:
        return "Weak";
    case game::ConditionIndex::Asleep:
        return "Asleep";
    case game::ConditionIndex::Afraid:
        return "Afraid";
    case game::ConditionIndex::Drunk:
        return "Drunk";
    case game::ConditionIndex::Insane:
        return "Insane";
    case game::ConditionIndex::Poison1:
    case game::ConditionIndex::Poison2:
    case game::ConditionIndex::Poison3:
        return "Poisoned";
    case game::ConditionIndex::Disease1:
    case game::ConditionIndex::Disease2:
    case game::ConditionIndex::Disease3:
        return "Diseased";
    case game::ConditionIndex::Paralyzed:
        return "Paralyzed";
    case game::ConditionIndex::Unconscious:
        return "Unconscious";
    case game::ConditionIndex::Dead:
        return "Dead";
    case game::ConditionIndex::Stoned:
        return "Stoned";
    case game::ConditionIndex::Eradicated:
        return "Eradicated";
    case game::ConditionIndex::Zombie:
        return "Zombie";
    case game::ConditionIndex::Count:
        return "Healthy";
    }
    return "Unknown";
}

} // namespace

void ShopWindow::show(const formats::TwoDEventEntry& building,
                      const std::vector<formats::ItemEntry>& items)
{
    open_ = true;
    buildingType_ = building.buildingType;
    family_ = game::shopFamily(buildingType_);
    shopName_ = building.name.empty() ? building.displayName : building.name;
    proprietor_ = building.proprietorName;
    if (!building.title.empty())
    {
        if (!proprietor_.empty())
            proprietor_ += ", ";
        proprietor_ += building.title;
    }
    buyMultiplier_ = building.buyMultiplier > 0.0f ? building.buyMultiplier : 1.0f;
    activeTab_ = ShopTab::Buy;
    listSelection_ = 0;
    scrollOffset_ = 0;
    buyList_.clear();
    sellList_.clear();
    // Only item shops stock a buy list; temples/travel render their own UI.
    if (family_ == game::ShopFamily::ItemShop)
    {
        buildBuyList(items);
    }
    // Stables/boats: build the destination list for this building's map.
    if (family_ == game::ShopFamily::Travel)
    {
        travelDestinations_ = game::destinationsForBuilding(building.mapId, buildingType_);
    }
    else
    {
        travelDestinations_.clear();
    }
}

void ShopWindow::close()
{
    open_ = false;
}

bool ShopWindow::itemFitsShop(game::BuildingType type, const std::string& equipStat)
{
    // Stocking heuristic from the ITEMS.TXT equipStat categories, mapped to the
    // MM7 shop types. (Full shop-inventory *generation* RE is a follow-up; this
    // deterministic filter gives every shop a populated, type-appropriate list.)
    switch (type)
    {
    case game::BuildingType::WeaponShop:
        return iequals(equipStat, "weapon") || iequals(equipStat, "weapon1") ||
               iequals(equipStat, "weaponw") || iequals(equipStat, "weapon2") ||
               iequals(equipStat, "weapon1or2") || iequals(equipStat, "missile") ||
               iequals(equipStat, "bow");
    case game::BuildingType::ArmorShop:
        return iequals(equipStat, "armor") || iequals(equipStat, "shield") ||
               iequals(equipStat, "helm") || iequals(equipStat, "boots") ||
               iequals(equipStat, "gauntlets") || iequals(equipStat, "belt") ||
               iequals(equipStat, "cloak") || iequals(equipStat, "amulet") ||
               iequals(equipStat, "ring");
    case game::BuildingType::MagicShop:
        return iequals(equipStat, "sscroll") || iequals(equipStat, "book") ||
               iequals(equipStat, "mscroll") || iequals(equipStat, "gem");
    case game::BuildingType::Alchemist:
        return iequals(equipStat, "bottle") || iequals(equipStat, "reagent");
    default:
        return false;
    }
}

void ShopWindow::buildBuyList(const std::vector<formats::ItemEntry>& items)
{
    buyList_.clear();
    if (party_ == nullptr)
    {
        return;
    }

    const game::Character& buyer = party_->member(std::clamp(party_->activeMemberIndex(), 0, 3));
    const int discount = game::ShopSystem::merchantDiscountPct(buyer, party_->reputation());

    for (const auto& def : items)
    {
        if (def.id <= 0 || def.value <= 0)
            continue;
        if (!itemFitsShop(buildingType_, def.equipStat))
            continue;

        const game::Item item{def.id};
        const int full = game::ShopSystem::itemFullPrice(def, item);
        const int price = game::ShopSystem::buyPrice(full, buyMultiplier_, discount);

        ShopListRow row;
        row.itemId = def.id;
        row.name = def.name.empty() ? def.notIdentifiedName : def.name;
        row.price = price;
        row.identified = true;
        buyList_.push_back(std::move(row));
    }
}

void ShopWindow::rebuildSellList()
{
    sellList_.clear();
    if (inventory_ == nullptr || party_ == nullptr)
    {
        return;
    }

    const int charIdx = std::clamp(party_->activeMemberIndex(), 0, 3);
    const auto& inv = inventory_->getInventory(charIdx);
    const game::Character& seller = party_->member(charIdx);
    const int discount = game::ShopSystem::merchantDiscountPct(seller, party_->reputation());

    for (size_t slot = 0; slot < inv.backpack.size(); slot++)
    {
        const auto& item = inv.backpack[slot];
        if (!item.valid())
            continue;

        const formats::ItemEntry* def = inventory_->getItemDef(item.itemId);
        if (def == nullptr)
            continue;

        const int full = game::ShopSystem::itemFullPrice(*def, item);
        const int credit = game::ShopSystem::sellPrice(full, buyMultiplier_, discount);

        ShopListRow row;
        row.itemId = item.itemId;
        row.name = (item.identified || def->name.empty()) ? def->name : def->notIdentifiedName;
        if (row.name.empty())
            row.name = def->name;
        row.price = credit;
        row.identified = item.identified;
        row.broken = item.broken;
        row.backpackSlot = static_cast<int>(slot);
        sellList_.push_back(std::move(row));
    }
}

void ShopWindow::refreshLists()
{
    // The sell list tracks the live backpack; the buy list is fixed for the
    // shop visit (stocked once on show()).
    if (activeTab_ == ShopTab::Sell)
        rebuildSellList();
}

bool ShopWindow::handleClick(int mouseX, int mouseY)
{
    if (!open_)
        return false;

    // Button hit-test (rects cached from the last render).
    for (const auto& btn : buttonRects_)
    {
        if (mouseX >= btn.x && mouseX < btn.x + btn.w && mouseY >= btn.y && mouseY < btn.y + btn.h)
        {
            if (family_ == game::ShopFamily::Temple)
            {
                if (btn.label == "Heal All")
                    doHeal();
                else if (btn.label == "Resurrect")
                    doResurrect();
                else if (btn.label == "Donate")
                    doDonate();
                else if (btn.label == "Close" || btn.label == "Exit")
                    close();
            }
            else if (family_ == game::ShopFamily::Training)
            {
                if (btn.label == "Train")
                    doTrain();
                else if (btn.label == "Close" || btn.label == "Exit")
                    close();
            }
            else if (family_ == game::ShopFamily::Travel)
            {
                if (btn.label == "Travel")
                    doTravel();
                else if (btn.label == "Close" || btn.label == "Exit")
                    close();
            }
            else
            {
                if (btn.label == "Buy/Do")
                {
                    if (activeTab_ == ShopTab::Buy)
                        doBuy();
                    else
                        doSell();
                }
                else if (btn.label == "Buy Tab")
                {
                    activeTab_ = ShopTab::Buy;
                    listSelection_ = 0;
                    scrollOffset_ = 0;
                }
                else if (btn.label == "Sell Tab")
                {
                    activeTab_ = ShopTab::Sell;
                    listSelection_ = 0;
                    scrollOffset_ = 0;
                    rebuildSellList();
                }
                else if (btn.label == "Close" || btn.label == "Exit")
                {
                    close();
                }
            }
            return true;
        }
    }
    return true; // modal: consume all clicks while open
}

bool ShopWindow::handleKey(int scancode)
{
    if (!open_)
        return false;

    if (scancode == SDL_SCANCODE_ESCAPE)
    {
        close();
        return true;
    }

    // Temple mode: Up/Down selects a party member (for Resurrect), Enter/H
    // heals, R resurrects the selected member, D donates.
    if (family_ == game::ShopFamily::Temple)
    {
        if (scancode == SDL_SCANCODE_DOWN)
        {
            if (listSelection_ < game::kPartySize - 1)
                listSelection_++;
            return true;
        }
        if (scancode == SDL_SCANCODE_UP)
        {
            if (listSelection_ > 0)
                listSelection_--;
            return true;
        }
        if (scancode == SDL_SCANCODE_H)
        {
            doHeal();
            return true;
        }
        if (scancode == SDL_SCANCODE_R)
        {
            doResurrect();
            return true;
        }
        if (scancode == SDL_SCANCODE_D)
        {
            doDonate();
            return true;
        }
        return true; // modal: consume all other keys
    }

    // Training mode: Up/Down selects a member, Enter/T trains.
    if (family_ == game::ShopFamily::Training)
    {
        if (scancode == SDL_SCANCODE_DOWN)
        {
            if (listSelection_ < game::kPartySize - 1)
                listSelection_++;
            return true;
        }
        if (scancode == SDL_SCANCODE_UP)
        {
            if (listSelection_ > 0)
                listSelection_--;
            return true;
        }
        if (scancode == SDL_SCANCODE_T || scancode == SDL_SCANCODE_RETURN ||
            scancode == SDL_SCANCODE_SPACE)
        {
            doTrain();
            return true;
        }
        return true;
    }

    // Travel mode: Up/Down selects a destination, Enter travels.
    if (family_ == game::ShopFamily::Travel)
    {
        const int destCount = static_cast<int>(travelDestinations_.size());
        if (scancode == SDL_SCANCODE_DOWN)
        {
            if (listSelection_ < destCount - 1)
                listSelection_++;
            return true;
        }
        if (scancode == SDL_SCANCODE_UP)
        {
            if (listSelection_ > 0)
                listSelection_--;
            return true;
        }
        if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_SPACE)
        {
            doTravel();
            return true;
        }
        return true;
    }

    if (scancode == SDL_SCANCODE_TAB)
    {
        activeTab_ = (activeTab_ == ShopTab::Buy) ? ShopTab::Sell : ShopTab::Buy;
        listSelection_ = 0;
        scrollOffset_ = 0;
        if (activeTab_ == ShopTab::Sell)
            rebuildSellList();
        return true;
    }

    const auto& list = (activeTab_ == ShopTab::Buy) ? buyList_ : sellList_;
    if (scancode == SDL_SCANCODE_DOWN)
    {
        if (listSelection_ < static_cast<int>(list.size()) - 1)
            listSelection_++;
        clampSelection();
        return true;
    }
    if (scancode == SDL_SCANCODE_UP)
    {
        if (listSelection_ > 0)
            listSelection_--;
        clampSelection();
        return true;
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_SPACE)
    {
        if (activeTab_ == ShopTab::Buy)
            doBuy();
        else
            doSell();
        return true;
    }
    return true; // modal: consume all keys while open
}

void ShopWindow::clampSelection()
{
    if (listSelection_ < scrollOffset_)
        scrollOffset_ = listSelection_;
    if (listSelection_ >= scrollOffset_ + kMaxVisibleRows)
        scrollOffset_ = listSelection_ - kMaxVisibleRows + 1;
}

void ShopWindow::doBuy()
{
    if (listSelection_ < 0 || listSelection_ >= static_cast<int>(buyList_.size()))
        return;
    if (party_ == nullptr || inventory_ == nullptr)
        return;

    if (buildingType_ == game::BuildingType::None)
        return;

    formats::TwoDEventEntry building;
    building.id = 0;
    building.buildingType = buildingType_;
    building.buyMultiplier = buyMultiplier_;
    game::ShopContext ctx{&building, party_, inventory_};

    const int charIdx = std::clamp(party_->activeMemberIndex(), 0, 3);
    const int itemId = buyList_[listSelection_].itemId;
    auto result = shop_.buyItem(ctx, charIdx, itemId);
    if (result.has_value())
    {
        if (onStatus_)
        {
            onStatus_(std::format("Purchased {} for {} gold.", buyList_[listSelection_].name,
                                  result->goldSpent));
        }
    }
    else if (onStatus_)
    {
        onStatus_(std::format("Cannot buy: {}.", game::shopErrorText(result.error())));
    }
}

void ShopWindow::doSell()
{
    if (listSelection_ < 0 || listSelection_ >= static_cast<int>(sellList_.size()))
        return;
    if (party_ == nullptr || inventory_ == nullptr)
        return;

    formats::TwoDEventEntry building;
    building.id = 0;
    building.buildingType = buildingType_;
    building.buyMultiplier = buyMultiplier_;
    game::ShopContext ctx{&building, party_, inventory_};

    const int charIdx = std::clamp(party_->activeMemberIndex(), 0, 3);
    const int slot = sellList_[listSelection_].backpackSlot;
    const std::string name = sellList_[listSelection_].name;
    auto result = shop_.sellItem(ctx, charIdx, slot);
    if (result.has_value())
    {
        if (onStatus_)
            onStatus_(std::format("Sold {} for {} gold.", name, result->goldGained));
        rebuildSellList();
        clampSelection();
    }
    else if (onStatus_)
    {
        onStatus_(std::format("Cannot sell: {}.", game::shopErrorText(result.error())));
    }
}

void ShopWindow::doHeal()
{
    if (party_ == nullptr)
        return;

    formats::TwoDEventEntry building;
    building.id = 0;
    building.buildingType = buildingType_;
    building.buyMultiplier = buyMultiplier_;
    game::ShopContext ctx{&building, party_, inventory_};

    auto result = shop_.healParty(ctx);
    if (onStatus_)
    {
        onStatus_(result.has_value()
                      ? std::format("Healed the party for {} gold.", result->goldSpent)
                      : std::format("Cannot heal: {}.", game::shopErrorText(result.error())));
    }
}

void ShopWindow::doResurrect()
{
    if (party_ == nullptr)
        return;

    formats::TwoDEventEntry building;
    building.id = 0;
    building.buildingType = buildingType_;
    building.buyMultiplier = buyMultiplier_;
    game::ShopContext ctx{&building, party_, inventory_};

    const int member = std::clamp(listSelection_, 0, game::kPartySize - 1);
    auto result = shop_.resurrectMember(ctx, member);
    if (onStatus_)
    {
        onStatus_(
            result.has_value()
                ? std::format("Resurrected member {} for {} gold.", member + 1, result->goldSpent)
                : std::format("Cannot resurrect: {}.", game::shopErrorText(result.error())));
    }
}

void ShopWindow::doDonate()
{
    if (party_ == nullptr)
        return;

    formats::TwoDEventEntry building;
    building.id = 0;
    building.buildingType = buildingType_;
    building.buyMultiplier = buyMultiplier_;
    game::ShopContext ctx{&building, party_, inventory_};

    auto result = shop_.donate(ctx);
    if (onStatus_)
    {
        onStatus_(result.has_value()
                      ? std::format("Donated {} gold to the temple.", result->goldSpent)
                      : std::format("Cannot donate: {}.", game::shopErrorText(result.error())));
    }
}

void ShopWindow::doTrain()
{
    if (party_ == nullptr)
        return;

    formats::TwoDEventEntry building;
    building.id = 0;
    building.buildingType = buildingType_;
    building.buyMultiplier = buyMultiplier_;
    game::ShopContext ctx{&building, party_, inventory_};

    const int member = std::clamp(listSelection_, 0, game::kPartySize - 1);
    auto result = shop_.trainMember(ctx, member);
    if (onStatus_)
    {
        onStatus_(result.has_value()
                      ? std::format("Trained member {} for {} gold (now level {}).", member + 1,
                                    result->goldSpent, party_->member(member).level)
                      : std::format("Cannot train: {}.", game::shopErrorText(result.error())));
    }
}

void ShopWindow::doTravel()
{
    if (party_ == nullptr)
        return;
    if (listSelection_ < 0 || listSelection_ >= static_cast<int>(travelDestinations_.size()))
    {
        if (onStatus_)
            onStatus_("No destination selected.");
        return;
    }

    const int discount = party_ ? game::ShopSystem::merchantDiscountPct(
                                      party_->member(std::clamp(party_->activeMemberIndex(), 0, 3)),
                                      party_->reputation())
                                : 0;
    const int cost = game::ShopSystem::travelCost(buildingType_, buyMultiplier_, discount);
    if (party_->gold() < cost)
    {
        if (onStatus_)
            onStatus_(std::format("Cannot travel: {}.",
                                  game::shopErrorText(game::ShopError::InsufficientGold)));
        return;
    }
    (void)party_->spendGold(cost);

    const auto& dest = travelDestinations_[listSelection_];
    if (onTravelRequest_)
    {
        TravelRequest req;
        req.mapName = dest.mapName;
        req.arrivalX = dest.arrivalX;
        req.arrivalY = dest.arrivalY;
        req.arrivalZ = dest.arrivalZ;
        req.arrivalFacing = dest.arrivalFacing;
        req.travelDays = dest.travelDays;
        onTravelRequest_(req);
    }
    if (onStatus_)
    {
        onStatus_(std::format("Traveling to {} ({} gold, {} days).", dest.displayName, cost,
                              dest.travelDays));
    }
    close(); // leave the shop on departure
}

void ShopWindow::render(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                        int viewportW, int viewportH)
{
    if (!open_)
        return;

    // Temples/training/travel render their own roster + service-button layout.
    if (family_ == game::ShopFamily::Temple)
    {
        renderTemple(renderer, debugText);
        return;
    }
    if (family_ == game::ShopFamily::Training)
    {
        renderTraining(renderer, debugText);
        return;
    }
    if (family_ == game::ShopFamily::Travel)
    {
        renderTravel(renderer, debugText);
        return;
    }

    // Keep the sell list fresh with the live backpack.
    if (activeTab_ == ShopTab::Sell)
        rebuildSellList();

    // Dim background (modal).
    renderer.drawFilledRect(0, 0, viewportW, viewportH, 0, 0, 0, 140);

    // Window chrome.
    renderer.drawFilledRect(kWindowX, kWindowY, kWindowW, kWindowH, 22, 22, 38, 235);
    renderer.drawRect(kWindowX, kWindowY, kWindowW, kWindowH, 130, 108, 60, 255);
    renderer.drawRect(kWindowX + 1, kWindowY + 1, kWindowW - 2, kWindowH - 2, 84, 74, 42, 210);

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (sdl == nullptr)
        return;

    int y = kWindowY + kPadding;

    // Title (shop name + proprietor).
    if (!shopName_.empty())
    {
        debugText.drawText(sdl, kWindowX + kPadding, y, kTitleScale, 255, 220, 120, shopName_);
        y += debugText.lineHeight(kTitleScale) + 2;
    }
    if (!proprietor_.empty())
    {
        debugText.drawText(sdl, kWindowX + kPadding, y, kTextScale, 180, 180, 200, proprietor_);
        y += debugText.lineHeight(kTextScale) + 2;
    }

    // Gold display (top-right).
    if (party_ != nullptr)
    {
        const std::string gold = std::format("Gold: {}", party_->gold());
        debugText.drawText(sdl, kWindowX + kWindowW - kPadding - 120, kWindowY + kPadding,
                           kTextScale, 255, 215, 0, gold);
    }

    // Separator.
    y += 2;
    renderer.drawFilledRect(kWindowX + kPadding, y, kWindowW - kPadding * 2, 1, 90, 78, 44, 220);
    y += kPadding / 2;

    // Tab buttons + action button + close (cached for hit-testing).
    buttonRects_.clear();
    int bx = kWindowX + kPadding;
    const int by = y;

    auto addBtn = [&](int x, int w, std::string label, uint8_t r, uint8_t g, uint8_t b)
    {
        renderer.drawFilledRect(x, by, w, kButtonHeight, r, g, b, 220);
        renderer.drawRect(x, by, w, kButtonHeight, 200, 190, 160, 255);
        debugText.drawText(sdl, x + 6, by + 4, kTextScale, 240, 240, 240, label);
        buttonRects_.push_back({x, by, w, kButtonHeight, std::move(label)});
    };

    const bool onBuy = activeTab_ == ShopTab::Buy;
    addBtn(bx, kButtonWidth, "Buy Tab", onBuy ? 70 : 40, onBuy ? 110 : 40, onBuy ? 70 : 55);
    bx += kButtonWidth + kPadding;
    addBtn(bx, kButtonWidth, "Sell Tab", !onBuy ? 70 : 40, !onBuy ? 110 : 40, !onBuy ? 70 : 55);
    bx += kButtonWidth + kPadding;
    addBtn(bx, kButtonWidth, onBuy ? "Buy/Do" : "Sell/Do", 50, 90, 50);
    bx += kButtonWidth + kPadding;
    addBtn(kWindowX + kWindowW - kPadding - kButtonWidth, kButtonWidth, "Close", 90, 40, 40);

    y = by + kButtonHeight + kPadding;

    // List area.
    const int listX = kWindowX + kPadding;
    const int listW = kWindowW - kPadding * 2;
    const int listTop = y;
    renderer.drawRect(listX, listTop, listW, kRowHeight * kMaxVisibleRows + 4, 60, 60, 80, 200);

    const std::string_view header = onBuy ? "  Item                                          Price"
                                          : "  Item                                          Value";
    debugText.drawText(sdl, listX + 4, listTop + 2, kTextScale, 200, 200, 160, std::string(header));
    y = listTop + kRowHeight + 4;

    const auto& list = onBuy ? buyList_ : sellList_;
    if (list.empty())
    {
        debugText.drawText(sdl, listX + 4, y, kTextScale, 160, 160, 170,
                           onBuy ? "(no stock)" : "(backpack empty)");
    }

    const int rowCount =
        std::min<int>(kMaxVisibleRows, static_cast<int>(list.size()) - scrollOffset_);
    for (int i = 0; i < rowCount; i++)
    {
        const int index = scrollOffset_ + i;
        if (index >= static_cast<int>(list.size()))
            break;
        const auto& row = list[index];
        const bool selected = index == listSelection_;

        if (selected)
        {
            renderer.drawFilledRect(listX + 2, y - 1, listW - 4, kRowHeight, 80, 70, 30, 220);
        }

        const std::string name = std::format("{}{}{}", row.broken ? "[broken] " : "",
                                             row.identified ? row.name : ("?" + row.name),
                                             row.identified ? "" : " (unidentified)");
        const std::string price = std::format("{}", row.price);
        debugText.drawText(sdl, listX + 6, y, kTextScale, 230, 230, 230, name);
        debugText.drawText(sdl, listX + listW - 6 - static_cast<int>(price.size()) * 8, y,
                           kTextScale, 255, 215, 0, price);
        y += kRowHeight;
    }

    // Footer hint.
    const int hintY = kWindowY + kWindowH - kPadding - debugText.lineHeight(kTextScale);
    debugText.drawText(sdl, kWindowX + kPadding, hintY, kTextScale, 160, 160, 170,
                       "Up/Down: select   Tab: switch list   Enter: transact   Esc: close");
}

void ShopWindow::renderTemple(graphics::IRenderer& renderer, const graphics::DebugText& debugText)
{
    const int viewportW = 640;
    const int viewportH = 480;

    // Dim background (modal).
    renderer.drawFilledRect(0, 0, viewportW, viewportH, 0, 0, 0, 140);

    // Window chrome.
    renderer.drawFilledRect(kWindowX, kWindowY, kWindowW, kWindowH, 30, 24, 22, 235);
    renderer.drawRect(kWindowX, kWindowY, kWindowW, kWindowH, 150, 120, 60, 255);
    renderer.drawRect(kWindowX + 1, kWindowY + 1, kWindowW - 2, kWindowH - 2, 96, 80, 44, 210);

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (sdl == nullptr)
        return;

    int y = kWindowY + kPadding;

    if (!shopName_.empty())
    {
        debugText.drawText(sdl, kWindowX + kPadding, y, kTitleScale, 255, 220, 120, shopName_);
        y += debugText.lineHeight(kTitleScale) + 2;
    }
    if (!proprietor_.empty())
    {
        debugText.drawText(sdl, kWindowX + kPadding, y, kTextScale, 180, 180, 200, proprietor_);
        y += debugText.lineHeight(kTextScale) + 2;
    }

    if (party_ != nullptr)
    {
        const std::string gold = std::format("Gold: {}", party_->gold());
        debugText.drawText(sdl, kWindowX + kWindowW - kPadding - 120, kWindowY + kPadding,
                           kTextScale, 255, 215, 0, gold);
    }

    y += 2;
    renderer.drawFilledRect(kWindowX + kPadding, y, kWindowW - kPadding * 2, 1, 110, 90, 50, 220);
    y += kPadding;

    // Service buttons (cached for hit-testing).
    buttonRects_.clear();
    const int by = y;
    auto addBtn = [&](int x, int w, std::string label, uint8_t r, uint8_t g, uint8_t b)
    {
        renderer.drawFilledRect(x, by, w, kButtonHeight, r, g, b, 220);
        renderer.drawRect(x, by, w, kButtonHeight, 210, 200, 160, 255);
        debugText.drawText(sdl, x + 6, by + 4, kTextScale, 240, 240, 240, label);
        buttonRects_.push_back({x, by, w, kButtonHeight, std::move(label)});
    };

    int bx = kWindowX + kPadding;
    addBtn(bx, kButtonWidth, "Heal All", 50, 110, 60);
    bx += kButtonWidth + kPadding;
    addBtn(bx, kButtonWidth, "Resurrect", 110, 90, 50);
    bx += kButtonWidth + kPadding;
    addBtn(bx, kButtonWidth, "Donate", 90, 90, 130);
    bx += kButtonWidth + kPadding;
    addBtn(kWindowX + kWindowW - kPadding - kButtonWidth, kButtonWidth, "Close", 90, 40, 40);

    y = by + kButtonHeight + kPadding;

    // Party roster: each member's HP/SP/condition + resurrect cost.
    const int rosterX = kWindowX + kPadding;
    const int rosterW = kWindowW - kPadding * 2;
    renderer.drawRect(rosterX, y, rosterW, kRowHeight * (game::kPartySize + 1) + 4, 70, 60, 40,
                      200);
    y += 2;

    debugText.drawText(sdl, rosterX + 4, y, kTextScale, 200, 200, 160,
                       "  Member                HP/SP      Condition     Raise");
    y += kRowHeight + 2;

    const int negotiatorIdx =
        party_ ? std::clamp(party_->activeMemberIndex(), 0, game::kPartySize - 1) : 0;
    const int discount = party_ ? game::ShopSystem::merchantDiscountPct(
                                      party_->member(negotiatorIdx), party_->reputation())
                                : 0;

    for (int i = 0; i < game::kPartySize; i++)
    {
        const bool selected = (i == listSelection_);
        if (selected)
        {
            renderer.drawFilledRect(rosterX + 2, y - 1, rosterW - 4, kRowHeight, 70, 60, 30, 220);
        }

        std::string condLabel = "Healthy";
        std::string raiseLabel = "-";
        if (party_ != nullptr)
        {
            const auto& m = party_->member(i);
            const game::ConditionIndex worst = m.worstActiveCondition();
            if (worst != game::ConditionIndex::Count)
                condLabel = conditionLabel(worst);

            const bool dead =
                (worst == game::ConditionIndex::Dead || worst == game::ConditionIndex::Stoned ||
                 worst == game::ConditionIndex::Eradicated);
            if (dead)
            {
                const int cost = game::ShopSystem::templeCost(m, buyMultiplier_, discount);
                raiseLabel = std::format("{}g", cost);
            }
        }

        const std::string hpSp =
            party_ ? std::format("{}/{}  {}/{}", party_->member(i).hitPoints,
                                 party_->member(i).maxHitPoints, party_->member(i).spellPoints,
                                 party_->member(i).maxSpellPoints)
                   : "-";
        const std::string line =
            std::format("  #{}  {:<14}{:<12}{:<14}{}", i + 1, "", hpSp, condLabel, raiseLabel);
        const uint8_t col = selected ? 255 : 230;
        debugText.drawText(sdl, rosterX + 4, y, kTextScale, col, col, col, line);
        y += kRowHeight;
    }

    // Footer hint.
    const int hintY = kWindowY + kWindowH - kPadding - debugText.lineHeight(kTextScale);
    debugText.drawText(
        sdl, kWindowX + kPadding, hintY, kTextScale, 160, 160, 170,
        "Up/Dn: select member   H: heal all   R: resurrect   D: donate   Esc: close");
}

void ShopWindow::renderTraining(graphics::IRenderer& renderer, const graphics::DebugText& debugText)
{
    const int viewportW = 640;
    const int viewportH = 480;

    renderer.drawFilledRect(0, 0, viewportW, viewportH, 0, 0, 0, 140);
    renderer.drawFilledRect(kWindowX, kWindowY, kWindowW, kWindowH, 24, 26, 34, 235);
    renderer.drawRect(kWindowX, kWindowY, kWindowW, kWindowH, 130, 108, 60, 255);
    renderer.drawRect(kWindowX + 1, kWindowY + 1, kWindowW - 2, kWindowH - 2, 90, 78, 44, 210);

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (sdl == nullptr)
        return;

    int y = kWindowY + kPadding;
    if (!shopName_.empty())
    {
        debugText.drawText(sdl, kWindowX + kPadding, y, kTitleScale, 255, 220, 120, shopName_);
        y += debugText.lineHeight(kTitleScale) + 2;
    }
    if (party_ != nullptr)
    {
        const std::string gold = std::format("Gold: {}", party_->gold());
        debugText.drawText(sdl, kWindowX + kWindowW - kPadding - 120, kWindowY + kPadding,
                           kTextScale, 255, 215, 0, gold);
    }
    y += 2;
    renderer.drawFilledRect(kWindowX + kPadding, y, kWindowW - kPadding * 2, 1, 110, 90, 50, 220);
    y += kPadding;

    // Train + Close buttons.
    buttonRects_.clear();
    const int by = y;
    auto addBtn = [&](int x, int w, std::string label, uint8_t r, uint8_t g, uint8_t b)
    {
        renderer.drawFilledRect(x, by, w, kButtonHeight, r, g, b, 220);
        renderer.drawRect(x, by, w, kButtonHeight, 210, 200, 160, 255);
        debugText.drawText(sdl, x + 6, by + 4, kTextScale, 240, 240, 240, label);
        buttonRects_.push_back({x, by, w, kButtonHeight, std::move(label)});
    };
    addBtn(kWindowX + kPadding, kButtonWidth, "Train", 60, 90, 130);
    addBtn(kWindowX + kWindowW - kPadding - kButtonWidth, kButtonWidth, "Close", 90, 40, 40);
    y = by + kButtonHeight + kPadding;

    // Party roster with level / XP-to-go / cost / can-level indicator.
    const int rosterX = kWindowX + kPadding;
    const int rosterW = kWindowW - kPadding * 2;
    renderer.drawRect(rosterX, y, rosterW, kRowHeight * (game::kPartySize + 1) + 4, 70, 60, 40,
                      200);
    y += 2;
    debugText.drawText(sdl, rosterX + 4, y, kTextScale, 200, 200, 160,
                       "  Member             Lvl   XP / Need     Cost     Ready");
    y += kRowHeight + 2;

    const int negotiatorIdx =
        party_ ? std::clamp(party_->activeMemberIndex(), 0, game::kPartySize - 1) : 0;
    const int discount = party_ ? game::ShopSystem::merchantDiscountPct(
                                      party_->member(negotiatorIdx), party_->reputation())
                                : 0;

    for (int i = 0; i < game::kPartySize; i++)
    {
        const bool selected = (i == listSelection_);
        if (selected)
            renderer.drawFilledRect(rosterX + 2, y - 1, rosterW - 4, kRowHeight, 70, 60, 30, 220);

        const auto& m = party_->member(i);
        const int need = m.xpRequiredForNextLevel();
        const bool ready = m.canLevelUp();
        const int cost = game::ShopSystem::trainingCost(m, buyMultiplier_, discount);
        const std::string line =
            std::format("  #{} {:<14}{:>3}   {}/{}   {:>5}   {}", i + 1, "", m.level, m.experience,
                        need, cost, ready ? "YES" : "no");
        debugText.drawText(sdl, rosterX + 4, y, kTextScale, selected ? 255 : 220,
                           selected ? 230 : 220, selected ? 170 : 200, line);
        y += kRowHeight;
    }

    const int hintY = kWindowY + kWindowH - kPadding - debugText.lineHeight(kTextScale);
    debugText.drawText(sdl, kWindowX + kPadding, hintY, kTextScale, 160, 160, 170,
                       "Up/Dn: select member   T/Enter: train   Esc: close");
}

void ShopWindow::renderTravel(graphics::IRenderer& renderer, const graphics::DebugText& debugText)
{
    const int viewportW = 640;
    const int viewportH = 480;

    renderer.drawFilledRect(0, 0, viewportW, viewportH, 0, 0, 0, 140);
    renderer.drawFilledRect(kWindowX, kWindowY, kWindowW, kWindowH, 20, 28, 34, 235);
    renderer.drawRect(kWindowX, kWindowY, kWindowW, kWindowH, 130, 108, 60, 255);
    renderer.drawRect(kWindowX + 1, kWindowY + 1, kWindowW - 2, kWindowH - 2, 90, 78, 44, 210);

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (sdl == nullptr)
        return;

    int y = kWindowY + kPadding;
    if (!shopName_.empty())
    {
        debugText.drawText(sdl, kWindowX + kPadding, y, kTitleScale, 255, 220, 120, shopName_);
        y += debugText.lineHeight(kTitleScale) + 2;
    }
    if (party_ != nullptr)
    {
        const std::string gold = std::format("Gold: {}", party_->gold());
        debugText.drawText(sdl, kWindowX + kWindowW - kPadding - 120, kWindowY + kPadding,
                           kTextScale, 255, 215, 0, gold);
    }
    y += 2;
    renderer.drawFilledRect(kWindowX + kPadding, y, kWindowW - kPadding * 2, 1, 110, 90, 50, 220);
    y += kPadding;

    // Travel + Close buttons.
    buttonRects_.clear();
    const int by = y;
    auto addBtn = [&](int x, int w, std::string label, uint8_t r, uint8_t g, uint8_t b)
    {
        renderer.drawFilledRect(x, by, w, kButtonHeight, r, g, b, 220);
        renderer.drawRect(x, by, w, kButtonHeight, 210, 200, 160, 255);
        debugText.drawText(sdl, x + 6, by + 4, kTextScale, 240, 240, 240, label);
        buttonRects_.push_back({x, by, w, kButtonHeight, std::move(label)});
    };
    addBtn(kWindowX + kPadding, kButtonWidth, "Travel", 60, 110, 130);
    addBtn(kWindowX + kWindowW - kPadding - kButtonWidth, kButtonWidth, "Close", 90, 40, 40);
    y = by + kButtonHeight + kPadding;

    const int listX = kWindowX + kPadding;
    const int listW = kWindowW - kPadding * 2;

    if (travelDestinations_.empty())
    {
        debugText.drawText(sdl, listX + 4, y, kTextScale, 200, 200, 200,
                           "No destinations available from here.");
    }
    else
    {
        const int negotiatorIdx =
            party_ ? std::clamp(party_->activeMemberIndex(), 0, game::kPartySize - 1) : 0;
        const int discount = party_ ? game::ShopSystem::merchantDiscountPct(
                                          party_->member(negotiatorIdx), party_->reputation())
                                    : 0;
        const int cost = game::ShopSystem::travelCost(buildingType_, buyMultiplier_, discount);

        renderer.drawRect(listX, y, listW, kRowHeight * 2 + 4, 70, 60, 40, 200);
        y += 2;
        debugText.drawText(sdl, listX + 4, y, kTextScale, 200, 200, 160,
                           std::format("  Destination              Days   Cost: {}g each", cost));
        y += kRowHeight + 2;

        for (int i = 0; i < static_cast<int>(travelDestinations_.size()); i++)
        {
            const bool selected = (i == listSelection_);
            if (selected)
                renderer.drawFilledRect(listX + 2, y - 1, listW - 4, kRowHeight, 70, 60, 30, 220);
            const auto& d = travelDestinations_[i];
            const std::string line = std::format("  {:<22}{:>3} days", d.displayName, d.travelDays);
            debugText.drawText(sdl, listX + 4, y, kTextScale, selected ? 255 : 220,
                               selected ? 230 : 220, selected ? 170 : 200, line);
            y += kRowHeight;
        }
    }

    const int hintY = kWindowY + kWindowH - kPadding - debugText.lineHeight(kTextScale);
    debugText.drawText(sdl, kWindowX + kPadding, hintY, kTextScale, 160, 160, 170,
                       "Up/Dn: select destination   Enter: travel   Esc: close");
}

} // namespace runeharbor::ui
