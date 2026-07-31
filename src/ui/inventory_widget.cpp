// SPDX-License-Identifier: MIT
#include "inventory_widget.hpp"

#include <format>

#include "../game/party.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

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

    // Draw character body
    int drawX = bounds_.x + (bounds_.width * 16 / 460); // approximate MM7 layout
    int drawY = bounds_.y + (bounds_.height * 16 / 344);
    int scaleW = bounds_.width * bodyW_ / 460;
    int scaleH = bounds_.height * bodyH_ / 344;

    // Default portrait body fallback
    if (bodyTexture_)
    {
        graphics::Rect src = {0, 0, bodyW_, bodyH_};
        graphics::Rect dst = {drawX, drawY, scaleW, scaleH};
        SDL_FRect sdlSrc = {static_cast<float>(src.x), static_cast<float>(src.y),
                            static_cast<float>(src.width), static_cast<float>(src.height)};
        SDL_FRect sdlDst = {static_cast<float>(dst.x), static_cast<float>(dst.y),
                            static_cast<float>(dst.width), static_cast<float>(dst.height)};
        SDL_RenderTexture(sdl, static_cast<SDL_Texture*>(bodyTexture_), &sdlSrc, &sdlDst);
    }

    // Try to load base body (if we have a mapping, but for now we rely on the equipped items
    // layering) MM7 items are rendered in a specific order (background to foreground: cloaks,
    // armor, weapons)

    if (inventory_)
    {
        const auto& inventory = inventory_->getInventory(activeCharacterIndex_);

        // Render equipped items
        // Simplified rendering order: Cloak, Armor, Boots, Belt, Helm, Gauntlets, Amulet, Rings,
        // Weapons
        std::vector<game::EquipSlot> renderOrder = {
            game::EquipSlot::Cloak,  game::EquipSlot::Armor,    game::EquipSlot::Boots,
            game::EquipSlot::Belt,   game::EquipSlot::Helmet,   game::EquipSlot::Gauntlets,
            game::EquipSlot::Amulet, game::EquipSlot::Ring1,    game::EquipSlot::Ring2,
            game::EquipSlot::Ring3,  game::EquipSlot::Ring4,    game::EquipSlot::Ring5,
            game::EquipSlot::Ring6,  game::EquipSlot::MainHand, game::EquipSlot::OffHand,
            game::EquipSlot::Bow};

        for (auto slot : renderOrder)
        {
            size_t slotIdx = static_cast<size_t>(slot);
            if (inventory.equipped[slotIdx].valid())
            {
                auto* itemDef = inventory_->getItemDef(inventory.equipped[slotIdx].itemId);
                if (itemDef)
                {
                    int iw = 0, ih = 0;
                    void* itemTex = getCachedTexture(itemDef->picFile, iw, ih);
                    if (itemTex)
                    {
                        // In a real paper doll, items have precise X/Y offsets defined per
                        // item/class. Here we use simplified placeholder offsets centering the
                        // items.
                        int itemDrawX = drawX + (scaleW / 2) - (iw / 2);
                        int itemDrawY = drawY + (scaleH / 2) - (ih / 2);

                        SDL_FRect sdlSrc = {0.0f, 0.0f, static_cast<float>(iw),
                                            static_cast<float>(ih)};
                        SDL_FRect sdlDst = {static_cast<float>(itemDrawX),
                                            static_cast<float>(itemDrawY), static_cast<float>(iw),
                                            static_cast<float>(ih)};
                        SDL_RenderTexture(sdl, static_cast<SDL_Texture*>(itemTex), &sdlSrc,
                                          &sdlDst);
                    }
                }
            }
        }

        // Render backpack grid (14 slots)
        // Backpack usually starts around x=180, y=20 in the inventory frame (in 460x344 scale)
        int gridStartX = bounds_.x + (bounds_.width * 180 / 460);
        int gridStartY = bounds_.y + (bounds_.height * 20 / 344);
        int slotSize = bounds_.width * 30 / 460;
        int cols = 7;

        for (int i = 0; i < game::kBackpackSlots; i++)
        {
            int row = i / cols;
            int col = i % cols;
            int slotX = gridStartX + col * (slotSize + 2);
            int slotY = gridStartY + row * (slotSize + 2);

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

namespace
{

// Paper-doll slot rectangles in the 460x344 design space (the coordinate
// system the render code divides bounds_ by). These define the click regions
// for unequipping; visually the items still render at the body center, so the
// regions are a pragmatic fixed layout covering the body silhouette.
struct SlotRect
{
    game::EquipSlot slot;
    int x, y, w, h; // design-space pixels (460x344)
};

// A standard MM7 paper-doll layout: weapons flanking the body, armor over the
// torso, helm at the head, boots at the feet, rings/amulet/belt down the sides.
constexpr SlotRect kPaperDollSlots[] = {
    {game::EquipSlot::MainHand, 90, 140, 50, 80},  // left hand (weapon)
    {game::EquipSlot::OffHand, 320, 140, 50, 80},  // right hand (shield)
    {game::EquipSlot::Bow, 30, 60, 50, 80},        // far left
    {game::EquipSlot::Armor, 180, 110, 100, 110},  // torso
    {game::EquipSlot::Helmet, 200, 40, 60, 60},    // head
    {game::EquipSlot::Boots, 200, 250, 100, 50},   // feet
    {game::EquipSlot::Belt, 200, 225, 100, 20},    // waist
    {game::EquipSlot::Cloak, 130, 110, 50, 110},   // back/shoulders
    {game::EquipSlot::Gauntlets, 90, 230, 50, 40}, // left forearm
    {game::EquipSlot::Amulet, 210, 100, 40, 30},   // neck
    {game::EquipSlot::Ring1, 30, 200, 25, 25},     {game::EquipSlot::Ring2, 30, 230, 25, 25},
    {game::EquipSlot::Ring3, 30, 260, 25, 25},     {game::EquipSlot::Ring4, 405, 200, 25, 25},
    {game::EquipSlot::Ring5, 405, 230, 25, 25},    {game::EquipSlot::Ring6, 405, 260, 25, 25},
};

} // namespace

int InventoryWidget::backpackSlotAt(int mouseX, int mouseY) const
{
    // Mirror the render math exactly (inventory_widget.cpp render()).
    const int gridStartX = bounds_.x + (bounds_.width * 180 / 460);
    const int gridStartY = bounds_.y + (bounds_.height * 20 / 344);
    const int slotSize = bounds_.width * 30 / 460;
    const int stride = slotSize + 2;
    const int cols = 7;

    const int localX = mouseX - gridStartX;
    const int localY = mouseY - gridStartY;
    if (localX < 0 || localY < 0 || slotSize <= 0)
        return -1;
    const int col = localX / stride;
    const int row = localY / stride;
    if (col < 0 || col >= cols || row < 0 || row >= (game::kBackpackSlots / cols + 1))
        return -1;
    // Reject clicks in the 2px gap between cells.
    if (localX - col * stride >= slotSize || localY - row * stride >= slotSize)
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
    // Convert screen pixel to design-space (460x344) coordinates.
    const float dx =
        static_cast<float>(mouseX - bounds_.x) * 460.0f / static_cast<float>(bounds_.width);
    const float dy =
        static_cast<float>(mouseY - bounds_.y) * 344.0f / static_cast<float>(bounds_.height);
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

    // Backpack click: equip the item in the clicked cell.
    const int backpackSlot = backpackSlotAt(mouseX, mouseY);
    if (backpackSlot < 0)
        return;
    const auto& inv = inventory_->getInventory(activeCharacterIndex_);
    if (!inv.backpack[static_cast<size_t>(backpackSlot)].valid())
        return;

    const int itemId = inv.backpack[static_cast<size_t>(backpackSlot)].itemId;
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