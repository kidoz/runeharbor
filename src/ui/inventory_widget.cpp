// SPDX-License-Identifier: MIT
#include "inventory_widget.hpp"

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
    // Just swallow clicks for now
    if (event.type == UIEventType::MouseDown && bounds_.contains(event.mouseX, event.mouseY))
    {
        return true;
    }
    return false;
}

} // namespace runeharbor::ui