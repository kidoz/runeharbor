// SPDX-License-Identifier: MIT
#include "inventory_widget.hpp"

#include <format>

#include "../game/party.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

namespace
{

// Inventory layout uses a 460x344 "design space" that the render code scales to
// bounds_. Both the paper doll and the backpack grid are expressed in these
// coordinates so render and hit-testing share one source of truth.
constexpr int kDesignW = 460;
constexpr int kDesignH = 344;

// Paper-doll slot rectangles in design space. This single table drives BOTH
// rendering (each equipped item is drawn inside its slot) and hit-testing
// (equippedSlotAt maps a click back to a slot), so the two cannot drift.
struct SlotRect
{
    game::EquipSlot slot;
    int x, y, w, h; // design-space pixels
};

// A standard MM7 paper-doll layout: weapons flanking the body, armor over the
// torso, helm at the head, boots at the feet, rings/amulet/belt down the sides.
// The order is also the render order, back-to-front (so cloaks/armor sit under
// weapons, etc.).
constexpr SlotRect kPaperDollSlots[] = {
    {game::EquipSlot::Cloak, 130, 110, 50, 110},   // back/shoulders
    {game::EquipSlot::Armor, 180, 110, 100, 110},  // torso
    {game::EquipSlot::Boots, 200, 250, 100, 50},   // feet
    {game::EquipSlot::Belt, 200, 225, 100, 20},    // waist
    {game::EquipSlot::Helmet, 200, 40, 60, 60},    // head
    {game::EquipSlot::Gauntlets, 90, 230, 50, 40}, // left forearm
    {game::EquipSlot::Amulet, 210, 100, 40, 30},   // neck
    {game::EquipSlot::Ring1, 30, 200, 25, 25},     {game::EquipSlot::Ring2, 30, 230, 25, 25},
    {game::EquipSlot::Ring3, 30, 260, 25, 25},     {game::EquipSlot::Ring4, 405, 200, 25, 25},
    {game::EquipSlot::Ring5, 405, 230, 25, 25},    {game::EquipSlot::Ring6, 405, 260, 25, 25},
    {game::EquipSlot::MainHand, 90, 140, 50, 80}, // left hand (weapon)
    {game::EquipSlot::OffHand, 320, 140, 50, 80}, // right hand (shield)
    {game::EquipSlot::Bow, 30, 60, 50, 80},       // far left
};

// Backpack grid. MM7's pool is large, but the on-screen grid RuneHarbor renders
// is a compact placeholder; both render and hit-test derive rows/cols from here
// so they agree on the cell layout. cellSize/origin are in design space.
constexpr int kBackpackGridCols = 7;
constexpr int kBackpackGridOriginX = 180;
constexpr int kBackpackGridOriginY = 20;
constexpr int kBackpackCellSize = 30;
constexpr int kBackpackCellStride = kBackpackCellSize + 2; // 2px gap between cells

} // namespace

InventoryWidget::InventoryWidget() {}

void* InventoryWidget::getCachedTexture(const std::string& name, int& w, int& h)
{
    if (name.empty())
        return nullptr;

    auto it = textureCache_.find(name);
    if (it != textureCache_.end())
    {
        w = it->second.w;
        h = it->second.h;
        return it->second.tex;
    }

    if (textureLookup_)
    {
        void* tex = textureLookup_(name, w, h);
        textureCache_[name] = {tex, w, h};
        return tex;
    }

    return nullptr;
}

void InventoryWidget::render(graphics::IRenderer& renderer,
                             [[maybe_unused]] const graphics::DebugText& text)
{
    if (!visible_)
        return;

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    // Draw inventory frame
    if (bgTexture_)
    {
        graphics::Rect src = {0, 0, bgW_, bgH_};
        graphics::Rect dst = {bounds_.x, bounds_.y, bounds_.width, bounds_.height};
        SDL_FRect sdlSrc = {static_cast<float>(src.x), static_cast<float>(src.y),
                            static_cast<float>(src.width), static_cast<float>(src.height)};
        SDL_FRect sdlDst = {static_cast<float>(dst.x), static_cast<float>(dst.y),
                            static_cast<float>(dst.width), static_cast<float>(dst.height)};
        SDL_RenderTexture(sdl, static_cast<SDL_Texture*>(bgTexture_), &sdlSrc, &sdlDst);
    }
    else
    {
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 40, 30, 20,
                                240);
    }

    if (!gameWorld_ || activeCharacterIndex_ < 0)
        return;
    if (activeCharacterIndex_ >= game::kPartySize)
        return;

    // Design-space (460x344) -> screen-space scaling, shared by the paper doll
    // and the backpack grid so render and hit-testing agree.
    const float scaleX = static_cast<float>(bounds_.width) / static_cast<float>(kDesignW);
    const float scaleY = static_cast<float>(bounds_.height) / static_cast<float>(kDesignH);
    auto toScreenX = [&](int designX) { return bounds_.x + static_cast<int>(designX * scaleX); };
    auto toScreenY = [&](int designY) { return bounds_.y + static_cast<int>(designY * scaleY); };

    if (inventory_)
    {
        const auto& inventory = inventory_->getInventory(activeCharacterIndex_);

        // Render equipped items. kPaperDollSlots is the single source of truth
        // for both positioning and click hit-testing; its order is back-to-front
        // (cloak/armor under weapons). Each item is drawn inside its own slot
        // rect, scaled to fit, rather than all stacked at the body center.
        for (const auto& slotRect : kPaperDollSlots)
        {
            const size_t slotIdx = static_cast<size_t>(slotRect.slot);
            if (!inventory.equipped[slotIdx].valid())
                continue;
            const auto* itemDef = inventory_->getItemDef(inventory.equipped[slotIdx].itemId);
            if (!itemDef)
                continue;
            int iw = 0, ih = 0;
            void* itemTex = getCachedTexture(itemDef->picFile, iw, ih);
            if (!itemTex || iw <= 0 || ih <= 0)
                continue;

            const int cellW = static_cast<int>(slotRect.w * scaleX);
            const int cellH = static_cast<int>(slotRect.h * scaleY);
            // Scale the item to fit inside its slot, preserving aspect ratio.
            const float itemScale =
                std::min(static_cast<float>(cellW) / iw, static_cast<float>(cellH) / ih);
            const int drawW = static_cast<int>(iw * itemScale);
            const int drawH = static_cast<int>(ih * itemScale);
            const int drawX = toScreenX(slotRect.x) + (cellW - drawW) / 2;
            const int drawY = toScreenY(slotRect.y) + (cellH - drawH) / 2;

            SDL_FRect sdlSrc = {0.0f, 0.0f, static_cast<float>(iw), static_cast<float>(ih)};
            SDL_FRect sdlDst = {static_cast<float>(drawX), static_cast<float>(drawY),
                                static_cast<float>(drawW), static_cast<float>(drawH)};
            SDL_RenderTexture(sdl, static_cast<SDL_Texture*>(itemTex), &sdlSrc, &sdlDst);
        }

        // Render backpack grid. rows/cols come from the shared design-space
        // constants so the layout matches backpackSlotAt() exactly.
        const int gridStartX = toScreenX(kBackpackGridOriginX);
        const int gridStartY = toScreenY(kBackpackGridOriginY);
        const int slotSize = static_cast<int>(kBackpackCellSize * scaleX);
        const int strideX = static_cast<int>(kBackpackCellStride * scaleX);
        const int strideY = static_cast<int>(kBackpackCellStride * scaleY);
        const int cols = kBackpackGridCols;

        for (int i = 0; i < game::kBackpackSlots; i++)
        {
            int row = i / cols;
            int col = i % cols;
            int slotX = gridStartX + col * strideX;
            int slotY = gridStartY + row * strideY;

            // Draw slot background
            renderer.drawFilledRect(slotX, slotY, slotSize, slotSize, 30, 20, 10, 180);
            renderer.drawRect(slotX, slotY, slotSize, slotSize, 60, 50, 40, 255);

            if (inventory.backpack[i].valid())
            {
                auto* itemDef = inventory_->getItemDef(inventory.backpack[i].itemId);
                if (itemDef)
                {
                    int iw = 0, ih = 0;
                    void* itemTex = getCachedTexture(itemDef->picFile, iw, ih);
                    if (itemTex)
                    {
                        // Scale item to fit slot (or span multiple if it's large)
                        float scale = std::min(static_cast<float>(slotSize) / iw,
                                               static_cast<float>(slotSize) / ih);
                        int scaledW = static_cast<int>(iw * scale);
                        int scaledH = static_cast<int>(ih * scale);

                        SDL_FRect sdlSrc = {0.0f, 0.0f, static_cast<float>(iw),
                                            static_cast<float>(ih)};
                        SDL_FRect sdlDst = {static_cast<float>(slotX + (slotSize - scaledW) / 2),
                                            static_cast<float>(slotY + (slotSize - scaledH) / 2),
                                            static_cast<float>(scaledW),
                                            static_cast<float>(scaledH)};
                        SDL_RenderTexture(sdl, static_cast<SDL_Texture*>(itemTex), &sdlSrc,
                                          &sdlDst);
                    }
                }
            }
        }
    }
}

bool InventoryWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;
    if (event.type != UIEventType::MouseDown)
        return false;
    if (!bounds_.contains(event.mouseX, event.mouseY))
        return false;

    handleClick(event.mouseX, event.mouseY);
    return true; // modal: consume the click while the panel is open
}

int InventoryWidget::backpackSlotAt(int mouseX, int mouseY) const
{
    // Mirror the render math exactly, using the shared design-space constants.
    const float scaleX = static_cast<float>(bounds_.width) / static_cast<float>(kDesignW);
    const float scaleY = static_cast<float>(bounds_.height) / static_cast<float>(kDesignH);
    const int gridStartX = bounds_.x + static_cast<int>(kBackpackGridOriginX * scaleX);
    const int gridStartY = bounds_.y + static_cast<int>(kBackpackGridOriginY * scaleY);
    const int slotSize = static_cast<int>(kBackpackCellSize * scaleX);
    const int strideX = static_cast<int>(kBackpackCellStride * scaleX);
    const int strideY = static_cast<int>(kBackpackCellStride * scaleY);
    const int cols = kBackpackGridCols;

    const int localX = mouseX - gridStartX;
    const int localY = mouseY - gridStartY;
    if (localX < 0 || localY < 0 || slotSize <= 0)
        return -1;
    const int col = localX / strideX;
    const int row = localY / strideY;
    // The grid holds exactly kBackpackSlots cells; reject anything beyond.
    if (col < 0 || col >= cols || row < 0 || row * cols + col >= game::kBackpackSlots)
        return -1;
    // Reject clicks in the gap between cells.
    if (localX - col * strideX >= slotSize || localY - row * strideY >= slotSize)
        return -1;
    const int slot = row * cols + col;
    if (slot < 0 || slot >= game::kBackpackSlots)
        return -1;
    return slot;
}

game::EquipSlot InventoryWidget::equippedSlotAt(int mouseX, int mouseY) const
{
    if (bounds_.width <= 0 || bounds_.height <= 0)
        return game::EquipSlot::Count;
    // Convert screen pixel to design-space (kDesignW x kDesignH) coordinates.
    const float dx = static_cast<float>(mouseX - bounds_.x) * static_cast<float>(kDesignW) /
                     static_cast<float>(bounds_.width);
    const float dy = static_cast<float>(mouseY - bounds_.y) * static_cast<float>(kDesignH) /
                     static_cast<float>(bounds_.height);
    for (const auto& r : kPaperDollSlots)
    {
        if (dx >= r.x && dx < r.x + r.w && dy >= r.y && dy < r.y + r.h)
            return r.slot;
    }
    return game::EquipSlot::Count;
}

void InventoryWidget::handleClick(int mouseX, int mouseY)
{
    if (inventory_ == nullptr)
        return;

    // Paper-doll click: unequip the item in the clicked slot.
    const game::EquipSlot slot = equippedSlotAt(mouseX, mouseY);
    if (slot != game::EquipSlot::Count)
    {
        const auto& inv = inventory_->getInventory(activeCharacterIndex_);
        const size_t slotIdx = static_cast<size_t>(slot);
        if (slotIdx < inv.equipped.size() && inv.equipped[slotIdx].valid())
        {
            const int itemId = inv.equipped[slotIdx].itemId;
            if (inventory_->unequip(activeCharacterIndex_, slot))
            {
                if (onStatus_)
                    onStatus_(std::format("Unequipped item #{}.", itemId));
            }
            else if (onStatus_)
            {
                onStatus_("Backpack full — cannot unequip.");
            }
        }
        return;
    }

    // Backpack click: branch on item kind — usable items are used, equippable
    // items are equipped.
    const int backpackSlot = backpackSlotAt(mouseX, mouseY);
    if (backpackSlot < 0)
        return;
    const auto& inv = inventory_->getInventory(activeCharacterIndex_);
    if (!inv.backpack[static_cast<size_t>(backpackSlot)].valid())
        return;

    const int itemId = inv.backpack[static_cast<size_t>(backpackSlot)].itemId;
    const game::EquipType kind = inventory_->getEquipType(itemId);

    // Usable items (FUN_004680F1 consume dispatch): potion/scroll/book/message.
    if (kind == game::EquipType::Potion || kind == game::EquipType::SpellScroll ||
        kind == game::EquipType::Book || kind == game::EquipType::MessageScroll)
    {
        inventory_->setSpellSystem(spellSystem_);
        auto result =
            inventory_->useItem(activeCharacterIndex_, backpackSlot, activeCharacterIndex_);
        if (onStatus_)
        {
            onStatus_(result.message.empty()
                          ? (result.used ? "Used item." : "Cannot use that item.")
                          : result.message);
        }
        return;
    }

    // Equippable gear.
    if (inventory_->equip(activeCharacterIndex_, backpackSlot))
    {
        if (onStatus_)
            onStatus_(std::format("Equipped item #{}.", itemId));
    }
    else if (onStatus_)
    {
        // The most common refusal is the skill gate (FUN_004926F8).
        onStatus_("Cannot equip — skill not learned or slot unavailable.");
    }
}

} // namespace runeharbor::ui