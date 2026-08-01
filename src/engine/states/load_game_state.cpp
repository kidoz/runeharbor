// SPDX-License-Identifier: MIT
#include "load_game_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>
#include <utility>

#include <cctype>

#include "../../game/game_world.hpp"
#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"

namespace runeharbor::engine
{

LoadGameState::LoadGameState(StateContext& ctx) : ctx(ctx) {}

LoadGameState::~LoadGameState() = default;

void LoadGameState::setBackground(void* tex, int w, int h)
{
    background_ = tex;
    backgroundWidth_ = w;
    backgroundHeight_ = h;
}

void LoadGameState::enter()
{
    refreshSlots();
    statusMessage_.clear();
}

void LoadGameState::exit() {}

std::optional<GameStateId> LoadGameState::update()
{
    if (ctx.isKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        return GameStateId::TitleScreen;
    }

    if (slots_.empty())
    {
        refreshSlots();
    }

    const int entryCount = totalEntries();
    if (ctx.isKeyPressed(SDL_SCANCODE_UP))
    {
        selectedSlot_ = (selectedSlot_ + entryCount - 1) % entryCount;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_DOWN))
    {
        selectedSlot_ = (selectedSlot_ + 1) % entryCount;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_PAGEUP))
    {
        selectedSlot_ = (selectedSlot_ + entryCount - 10) % entryCount;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_PAGEDOWN))
    {
        selectedSlot_ = (selectedSlot_ + 10) % entryCount;
    }

    const int pageStart = (selectedSlot_ / 10) * 10;

    struct NumberKey
    {
        SDL_Scancode key;
        int pageOffset;
    };
    constexpr NumberKey kNumberKeys[] = {
        {SDL_SCANCODE_1, 0}, {SDL_SCANCODE_2, 1}, {SDL_SCANCODE_3, 2}, {SDL_SCANCODE_4, 3},
        {SDL_SCANCODE_5, 4}, {SDL_SCANCODE_6, 5}, {SDL_SCANCODE_7, 6}, {SDL_SCANCODE_8, 7},
        {SDL_SCANCODE_9, 8}, {SDL_SCANCODE_0, 9},
    };
    for (const auto& key : kNumberKeys)
    {
        if (ctx.isKeyPressed(key.key))
        {
            selectedSlot_ = std::min(entryCount - 1, pageStart + key.pageOffset);
            break;
        }
    }

    if (ctx.isKeyPressed(SDL_SCANCODE_RETURN))
    {
        if (loadSelectedSlot())
        {
            return GameStateId::Loading;
        }
    }

    return std::nullopt;
}

void LoadGameState::render()
{
    if (!ctx.renderer)
    {
        return;
    }

    if (background_)
    {
        ctx.renderFullscreenTexture(background_, backgroundWidth_, backgroundHeight_);
    }
    else
    {
        ctx.renderer->clear(20, 20, 40, 255);
    }

    if (!ctx.debugText || !ctx.renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    int scale = 2;

    int titleX = ctx.scaleX(210);
    int titleY = ctx.scaleY(32);
    ctx.debugText->drawText(sdlRenderer, titleX, titleY, scale + 1, 255, 215, 0, "LOAD GAME");

    int listX = ctx.scaleX(72);
    int listY = ctx.scaleY(90);
    int lineHeight = ctx.debugText->lineHeight(scale);
    const int entryCount = totalEntries();
    const int pageStart = (selectedSlot_ / 10) * 10;
    const int pageEnd = std::min(pageStart + 10, entryCount);

    for (int i = pageStart; i < pageEnd; i++)
    {
        const bool selected = (i == selectedSlot_);
        const auto colorR = selected ? 255 : 200;
        const auto colorG = selected ? 235 : 200;
        const auto colorB = selected ? 100 : 200;

        const std::string text = entryLabel(i);
        ctx.debugText->drawText(sdlRenderer, listX, listY + (i - pageStart) * lineHeight, scale,
                                colorR, colorG, colorB, text);
    }

    const int pageLabelX = ctx.scaleX(430);
    const int pageLabelY = ctx.scaleY(90);
    const int pageIndex = (selectedSlot_ / 10) + 1;
    const int pageCount = (entryCount + 9) / 10;
    ctx.debugText->drawText(sdlRenderer, pageLabelX, pageLabelY, std::max(1, scale - 1), 180, 180,
                            180, std::format("PAGE {}/{}", pageIndex, pageCount));

    if (entryCount > 10)
    {
        ctx.debugText->drawText(sdlRenderer, pageLabelX, pageLabelY + lineHeight,
                                std::max(1, scale - 1), 140, 140, 140,
                                std::format("SLOTS {}-{}", pageStart + 1, pageEnd));
    }

    if (!statusMessage_.empty())
    {
        int msgX = ctx.scaleX(72);
        int msgY = ctx.scaleY(410);
        ctx.debugText->drawText(sdlRenderer, msgX, msgY, scale, 255, 180, 120, statusMessage_);
    }

    int hintX = ctx.scaleX(72);
    int hintY = ctx.scaleY(444);
    ctx.debugText->drawText(sdlRenderer, hintX, hintY, std::max(1, scale - 1), 120, 120, 120,
                            "UP/DOWN select, PGUP/PGDN page, 1-0 pick in page, ENTER load, ESC");
}

void LoadGameState::refreshSlots()
{
    slots_.clear();
    autosaveExists_ = false;
    autosaveHeader_.reset();
    if (!ctx.shared || !ctx.shared->saveGame)
    {
        return;
    }
    slots_ = ctx.shared->saveGame->listSlots();
    autosaveExists_ = ctx.shared->saveGame->autosaveExists();
    autosaveHeader_ = ctx.shared->saveGame->autosaveHeader();

    if (autosaveExists_)
    {
        selectedSlot_ = 0;
        return;
    }

    int firstExisting = 1;
    for (size_t i = 0; i < slots_.size(); i++)
    {
        if (slots_[i].exists)
        {
            firstExisting = static_cast<int>(i) + 1;
            break;
        }
    }

    selectedSlot_ = firstExisting;
}

bool LoadGameState::loadSelectedSlot()
{
    if (!ctx.shared || !ctx.shared->saveGame || !ctx.shared->gameWorld)
    {
        statusMessage_ = "Save system unavailable";
        return false;
    }

    std::vector<uint8_t> eventRuntimeState;
    if (selectedSlot_ == 0)
    {
        if (!autosaveExists_)
        {
            statusMessage_ = "Autosave is empty";
            return false;
        }
        if (!ctx.shared->saveGame->loadAutosave(*ctx.shared->gameWorld, &eventRuntimeState,
                                                ctx.shared->inventory, ctx.shared->questLog))
        {
            statusMessage_ = "Failed to load autosave";
            return false;
        }
    }
    else if (!ctx.shared->saveGame->slotExists(selectedSlot_ - 1))
    {
        statusMessage_ = std::format("Slot {} is empty", selectedSlot_);
        return false;
    }
    else if (!ctx.shared->saveGame->load(*ctx.shared->gameWorld, selectedSlot_ - 1,
                                         &eventRuntimeState, ctx.shared->inventory,
                                         ctx.shared->questLog))
    {
        statusMessage_ = std::format("Failed to load slot {}", selectedSlot_);
        return false;
    }

    const std::string mapName = ctx.shared->gameWorld->currentMap();
    if (mapName.empty())
    {
        statusMessage_ = "Loaded save has empty map name";
        return false;
    }

    std::string ext = mapName.size() >= 4 ? mapName.substr(mapName.size() - 4) : "";
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    ctx.shared->startupMapName = mapName;
    ctx.shared->startupPreferOutdoor = (ext == ".odm");
    ctx.shared->autoLoadMap = true;
    ctx.shared->loadFromSave = true;
    ctx.shared->hasPendingEventRuntimeState = !eventRuntimeState.empty();
    ctx.shared->pendingEventRuntimeState = std::move(eventRuntimeState);
    if (selectedSlot_ == 0)
    {
        ctx.shared->statusMessage = std::format("Loaded autosave ({})", mapName);
    }
    else
    {
        ctx.shared->statusMessage = std::format("Loaded slot {} ({})", selectedSlot_, mapName);
    }
    statusMessage_ = std::format("Loading {}...", mapName);
    return true;
}

std::string LoadGameState::slotLabel(const game::SaveSlotInfo& slot)
{
    if (!slot.exists)
    {
        return std::format("{}. <empty>", slot.slotIndex + 1);
    }

    const std::string map =
        (slot.header.mapName[0] != '\0') ? slot.header.mapName : std::string("(unknown map)");
    const std::string leader =
        (slot.header.partyLeader[0] != '\0') ? slot.header.partyLeader : std::string("Party");

    return std::format("{}. {}  {}  L{}", slot.slotIndex + 1, map, leader, slot.header.partyLevel);
}

std::string LoadGameState::entryLabel(int entryIndex) const
{
    if (entryIndex == 0)
    {
        if (!autosaveExists_)
        {
            return "A. AUTOSAVE <empty>";
        }

        if (!autosaveHeader_.has_value())
        {
            return "A. AUTOSAVE <unknown>";
        }

        const std::string map = (autosaveHeader_->mapName[0] != '\0')
                                    ? autosaveHeader_->mapName
                                    : std::string("(unknown map)");
        const std::string leader = (autosaveHeader_->partyLeader[0] != '\0')
                                       ? autosaveHeader_->partyLeader
                                       : std::string("Party");
        return std::format("A. AUTOSAVE {}  {}  L{}", map, leader, autosaveHeader_->partyLevel);
    }

    const int slotIndex = entryIndex - 1;
    if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size()))
    {
        return std::format("{}. <unavailable>", entryIndex);
    }
    return slotLabel(slots_[static_cast<size_t>(slotIndex)]);
}

int LoadGameState::totalEntries() const
{
    return game::SaveGame::kMaxSlots + 1;
}

} // namespace runeharbor::engine
