// SPDX-License-Identifier: MIT
#include "ingame_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

#include <cctype>
#include <cmath>

#include "../../formats/odm_map.hpp"
#include "../../game/combat.hpp"
#include "../../game/event_engine.hpp"
#include "../../game/game_world.hpp"
#include "../../game/inventory.hpp"
#include "../../game/save_game.hpp"
#include "../../game/spells.hpp"
#include "../../graphics/camera.hpp"
#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"
#include "../../graphics/math3d.hpp"
#include "../../graphics/visibility.hpp"
#include "../../graphics/world_coordinates.hpp"
#include "../../graphics/world_renderer.hpp"
#include "../outdoor_terrain.hpp"
#include "../physics.hpp"

namespace runeharbor::engine
{

namespace
{
const char* onOff(bool value)
{
    return value ? "ON" : "OFF";
}

std::string pickTypeLabel(graphics::PickObjectType type)
{
    switch (type)
    {
    case graphics::PickObjectType::IndoorFace:
        return "FACE";
    case graphics::PickObjectType::IndoorDecoration:
        return "DECOR";
    case graphics::PickObjectType::OutdoorBuildingFace:
        return "BLD_FACE";
    case graphics::PickObjectType::Monster:
        return "MONSTER";
    case graphics::PickObjectType::MapItem:
        return "ITEM";
    case graphics::PickObjectType::Unknown:
    default:
        return "OBJECT";
    }
}

SDL_Color pickTypeColor(graphics::PickObjectType type)
{
    switch (type)
    {
    case graphics::PickObjectType::IndoorFace:
        return SDL_Color{255, 210, 90, 255};
    case graphics::PickObjectType::IndoorDecoration:
        return SDL_Color{120, 240, 190, 255};
    case graphics::PickObjectType::OutdoorBuildingFace:
        return SDL_Color{255, 140, 90, 255};
    case graphics::PickObjectType::Monster:
        return SDL_Color{255, 80, 80, 255};
    case graphics::PickObjectType::MapItem:
        return SDL_Color{120, 180, 255, 255};
    case graphics::PickObjectType::Unknown:
    default:
        return SDL_Color{255, 255, 255, 255};
    }
}

game::EventInteractionType toInteractionType(graphics::PickObjectType type)
{
    switch (type)
    {
    case graphics::PickObjectType::IndoorFace:
        return game::EventInteractionType::IndoorFace;
    case graphics::PickObjectType::IndoorDecoration:
        return game::EventInteractionType::IndoorDecoration;
    case graphics::PickObjectType::OutdoorBuildingFace:
        return game::EventInteractionType::OutdoorBuildingFace;
    case graphics::PickObjectType::Monster:
    case graphics::PickObjectType::MapItem:
    case graphics::PickObjectType::Unknown:
    default:
        return game::EventInteractionType::Unknown;
    }
}

std::string toUpper(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}
} // namespace

InGameState::InGameState(StateContext& ctx) : ctx(ctx) {}

InGameState::~InGameState() = default;

void InGameState::enter()
{
    lastUpdateTicks_ = 0;
    fpsAccumulatorMs_ = 0.0f;
    fpsFrameCounter_ = 0;
    displayedFps_ = 0.0f;

    inventory_.setVisible(false);
    characterStats_.setVisible(false);
    spellbook_.setVisible(false);
    restWidget_.setVisible(false);
    mapWidget_.setVisible(false);
    journalWidget_.setVisible(false);

    if (ctx.shared && ctx.shared->gameWorld)
    {
        hud_.setGameWorld(ctx.shared->gameWorld);
    }
    if (ctx.shared)
    {
        hud_.setMapScene(ctx.shared->mapScene);
    }

    dialogue_.setOnDismiss([this]() { statusLine_ = "Dialogue closed"; });
    dialogue_.setOnChoice(
        [this](int choiceId)
        {
            if (choiceId <= 0)
            {
                statusLine_ = "Dialogue option had no event id";
                return;
            }
            if (!ctx.shared || !ctx.shared->eventEngine)
            {
                statusLine_ = std::format("Dialogue option #{} unavailable", choiceId);
                return;
            }

            const bool triggered = ctx.shared->eventEngine->triggerEvent(choiceId);
            statusLine_ = triggered ? std::format("Triggered dialogue event #{}", choiceId)
                                    : std::format("Dialogue event #{} not found", choiceId);
        });

    // Shop window status feedback -> on-screen status line.
    shopWindow_.setStatusCallback(
        [this](const std::string& message)
        {
            statusLine_ = message;
            if (ctx.shared)
            {
                ctx.shared->statusMessage = message;
            }
        });
    // Travel: advance the game clock by the trip duration, then request a map
    // transition to the destination (FUN_004B68A6 advances time then drives the
    // map-load pipeline directly).
    shopWindow_.setTravelRequestCallback(
        [this](const ui::ShopWindow::TravelRequest& req)
        {
            if (!ctx.shared || !ctx.shared->gameWorld)
                return;
            // Advance the calendar by travelDays * 24 hours.
            ctx.shared->gameWorld->advanceTime(static_cast<int64_t>(req.travelDays) * 24 * 60);
            // Hand off the destination to the loading state.
            ctx.shared->startupMapName = req.mapName;
            ctx.shared->startupPreferOutdoor =
                (req.mapName.size() >= 4 &&
                 req.mapName.compare(req.mapName.size() - 4, 4, ".odm") == 0);
            ctx.shared->autoLoadMap = true;
            ctx.shared->arrivalOverrideActive = true;
            ctx.shared->arrivalX = req.arrivalX;
            ctx.shared->arrivalY = req.arrivalY;
            ctx.shared->arrivalZ = req.arrivalZ;
            ctx.shared->arrivalYaw = req.arrivalFacing;
            pendingTravel_ = true;
        });
    inventory_.setStatusCallback(
        [this](const std::string& message)
        {
            statusLine_ = message;
            if (ctx.shared)
            {
                ctx.shared->statusMessage = message;
            }
        });
    spellbook_.setStatusCallback(
        [this](const std::string& message)
        {
            statusLine_ = message;
            if (ctx.shared)
            {
                ctx.shared->statusMessage = message;
            }
        });
    spellbook_.setSpellCastRequestCallback(
        [this](const ui::SpellCastRequest& req)
        {
            // Enter targeting mode for spells that need a clicked target;
            // fire immediately for self/party buffs.
            selectedSpellId_ = req.spellId;
            pendingTargetType_ = req.targetType;
            if (req.targetType == game::SpellTarget::Self ||
                req.targetType == game::SpellTarget::AllAllies)
            {
                // Buff/self: cast immediately on the active caster.
                const int caster = findActivePartyMember();
                if (caster >= 0 && ctx.shared && ctx.shared->spellSystem)
                {
                    auto r = ctx.shared->spellSystem->castBuffSpell(caster, req.spellId);
                    statusLine_ = r.description;
                }
                targetingActive_ = false;
            }
            else
            {
                targetingActive_ = true;
                spellbook_.setVisible(false); // close the spellbook to pick a target
                statusLine_ =
                    (req.targetType == game::SpellTarget::SingleAlly)
                        ? std::format("Click a party member to cast spell #{}.", req.spellId)
                        : std::format("Click a monster to cast spell #{}.", req.spellId);
            }
        });
}

void InGameState::exit()
{
    dialogue_.close();
    shopWindow_.close();
}

graphics::Rect InGameState::worldViewportRect() const
{
    const int fullW = std::max(1, ctx.viewportWidth);
    const int fullH = std::max(1, ctx.viewportHeight);

    int gameX = 8;
    int gameY = 8;
    int gameW = 468;
    int gameH = 351;
    if (ctx.shared)
    {
        gameX = std::max(0, ctx.shared->worldViewportX);
        gameY = std::max(0, ctx.shared->worldViewportY);
        gameW = std::max(1, ctx.shared->worldViewportWidth);
        gameH = std::max(1, ctx.shared->worldViewportHeight);
    }

    const float scale = std::min(static_cast<float>(fullW) / static_cast<float>(kGameWidth),
                                 static_cast<float>(fullH) / static_cast<float>(kGameHeight));
    const float offsetX =
        (static_cast<float>(fullW) - static_cast<float>(kGameWidth) * scale) * 0.5f;
    const float offsetY =
        (static_cast<float>(fullH) - static_cast<float>(kGameHeight) * scale) * 0.5f;

    int x = static_cast<int>(offsetX + static_cast<float>(gameX) * scale);
    int y = static_cast<int>(offsetY + static_cast<float>(gameY) * scale);
    int w = std::max(1, static_cast<int>(static_cast<float>(gameW) * scale));
    int h = std::max(1, static_cast<int>(static_cast<float>(gameH) * scale));

    x = std::clamp(x, 0, fullW - 1);
    y = std::clamp(y, 0, fullH - 1);
    w = std::clamp(w, 1, fullW - x);
    h = std::clamp(h, 1, fullH - y);
    return graphics::Rect{x, y, w, h};
}

bool InGameState::mapMouseToWorldViewport(int screenX, int screenY, int& localX, int& localY) const
{
    const graphics::Rect worldVp = worldViewportRect();
    if (!worldVp.contains(screenX, screenY))
    {
        return false;
    }
    localX = screenX - worldVp.x;
    localY = screenY - worldVp.y;
    return true;
}

std::optional<GameStateId> InGameState::update()
{
    const uint64_t now = SDL_GetTicks();
    const float deltaMs = (lastUpdateTicks_ == 0)
                              ? 16.0f
                              : static_cast<float>(std::max<uint64_t>(1, now - lastUpdateTicks_));
    lastUpdateTicks_ = now;
    fpsAccumulatorMs_ += deltaMs;
    fpsFrameCounter_++;
    if (fpsAccumulatorMs_ >= 250.0f)
    {
        displayedFps_ = static_cast<float>(fpsFrameCounter_) * 1000.0f / fpsAccumulatorMs_;
        fpsAccumulatorMs_ = 0.0f;
        fpsFrameCounter_ = 0;
    }

    if (ctx.shared && ctx.shared->gameWorld)
    {
        constexpr double kTicksPerMs = 128.0 / 1000.0;
        const int64_t deltaTicks =
            std::max<int64_t>(1, static_cast<int64_t>(deltaMs * kTicksPerMs));
        ctx.shared->gameWorld->calendar().advanceTicks(deltaTicks);
    }

    if (ctx.shared && ctx.shared->eventEngine)
    {
        (void)ctx.shared->eventEngine->updateRuntimeTriggers();
    }

    if (ctx.shared && ctx.shared->gameWorld)
    {
        const int deferredBuildingId = ctx.shared->gameWorld->consumeDeferredBuildingEvent();
        if (deferredBuildingId > 0 && ctx.shared->statusMessage.empty())
        {
            ctx.shared->statusMessage =
                std::format("Building interaction opened (id #{})", deferredBuildingId);
        }
    }

    if (ctx.shared && !ctx.shared->statusMessage.empty())
    {
        statusLine_ = ctx.shared->statusMessage;
        ctx.shared->statusMessage.clear();
    }

    if (ctx.shared && ctx.shared->openNpcDialogue)
    {
        std::string speaker = ctx.shared->npcDialogueSpeaker;
        if (speaker.empty())
        {
            if (ctx.shared->pendingNpcDialogId > 0)
            {
                speaker = std::format("NPC #{}", ctx.shared->pendingNpcDialogId);
            }
            else
            {
                speaker = "NPC";
            }
        }
        const std::string text =
            ctx.shared->npcDialogueText.empty() ? "..." : ctx.shared->npcDialogueText;
        std::vector<ui::DialogueChoice> choices;
        const size_t count = std::min(ctx.shared->npcDialogueChoiceIds.size(),
                                      ctx.shared->npcDialogueChoiceTexts.size());
        choices.reserve(count);
        for (size_t i = 0; i < count; i++)
        {
            const int choiceId = ctx.shared->npcDialogueChoiceIds[i];
            const std::string& choiceText = ctx.shared->npcDialogueChoiceTexts[i];
            if (choiceId <= 0 || choiceText.empty())
            {
                continue;
            }
            choices.push_back({choiceId, choiceText});
        }

        dialogue_.show(speaker, text, choices);
        ctx.shared->openNpcDialogue = false;
        ctx.shared->awaitingNpcDialogText = false;
        ctx.shared->pendingNpcDialogId = -1;
        ctx.shared->npcDialogueSpeaker.clear();
        ctx.shared->npcDialogueText.clear();
        ctx.shared->npcDialogueChoiceIds.clear();
        ctx.shared->npcDialogueChoiceTexts.clear();
    }

    if (ctx.shared && ctx.shared->openShop)
    {
        // Resolve the building entry handed off by EVT_SHOW_BUILDING and open
        // the shop window. Stock the buy list from the loaded item table.
        if (ctx.shared->inventory && ctx.shared->gameWorld)
        {
            shopWindow_.setContext(&ctx.shared->gameWorld->party(), ctx.shared->inventory);
            shopWindow_.show(ctx.shared->pendingShopBuilding, ctx.shared->inventory->itemTable());
        }
        ctx.shared->openShop = false;
    }

    if (shopWindow_.isOpen())
    {
        if (ctx.window.wasMousePressed(platform::MouseButton::Left))
        {
            const auto ms = ctx.window.getMouseState();
            (void)shopWindow_.handleClick(ms.x, ms.y);
        }

        for (const SDL_Scancode key : {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                       SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE, SDL_SCANCODE_TAB})
        {
            if (ctx.isKeyPressed(key))
            {
                (void)shopWindow_.handleKey(static_cast<int>(key));
            }
        }

        return std::nullopt;
    }

    if (dialogue_.isOpen())
    {
        if (ctx.window.wasMousePressed(platform::MouseButton::Left))
        {
            const auto ms = ctx.window.getMouseState();
            (void)dialogue_.handleClick(ms.x, ms.y);
        }

        for (const SDL_Scancode key : {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                       SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE})
        {
            if (ctx.isKeyPressed(key))
            {
                (void)dialogue_.handleKey(static_cast<int>(key));
            }
        }

        return std::nullopt;
    }

    updateCameraInput(deltaMs);
    if (ctx.worldRenderer && ctx.shared && ctx.shared->mapScene &&
        ctx.shared->mapScene->isLoaded() && ctx.camera)
    {
        const graphics::Rect worldVp = worldViewportRect();
        ctx.camera->setAspectRatio(static_cast<float>(worldVp.width) /
                                   static_cast<float>(worldVp.height));

        if (ctx.shared->gameWorld)
        {
            std::vector<graphics::PickCandidate> extraCandidates;
            const std::string mapName = ctx.shared->mapScene->getName();
            if (const auto* spawned = ctx.shared->gameWorld->getSpawnedMapItems(mapName);
                spawned != nullptr)
            {
                extraCandidates.reserve(spawned->size());
                for (size_t i = 0; i < spawned->size(); i++)
                {
                    const auto& item = (*spawned)[i];
                    extraCandidates.push_back({
                        .id = static_cast<int>(i) + 1,
                        .worldPos = {item.x, item.y, item.z},
                        .type = graphics::PickObjectType::MapItem,
                        .objectIndex = static_cast<int>(i),
                        .eventId = 0,
                    });
                }
            }
            ctx.worldRenderer->setExtraPickCandidates(std::move(extraCandidates));
        }
        else
        {
            ctx.worldRenderer->clearExtraPickCandidates();
        }
        ctx.worldRenderer->refreshPickCache(*ctx.shared->mapScene, *ctx.camera);
    }

    hoveredPick_ = pickMapObjectUnderCursor(false);
    hoveredEventId_ = hoveredPick_.has_value() ? hoveredPick_->eventId : 0;
    hoverLine_.clear();
    if (hoveredPick_.has_value())
    {
        std::string hoverLabel;
        if (hoveredEventId_ > 0)
        {
            hoverLabel = std::format("HOVER {} EVENT #{}", pickTypeLabel(hoveredPick_->type),
                                     hoveredEventId_);
            if (ctx.shared && ctx.shared->gameWorld)
            {
                if (const auto* transition = ctx.shared->gameWorld->getTransition(hoveredEventId_);
                    transition && !transition->targetMap.empty())
                {
                    std::string target = transition->targetDisplayName.empty()
                                             ? transition->targetMap
                                             : transition->targetDisplayName;
                    hoverLabel = std::format("HOVER EVENT #{} -> {}", hoveredEventId_, target);
                }
            }
        }
        else
        {
            hoverLabel = std::format("HOVER {} #{}", pickTypeLabel(hoveredPick_->type),
                                     hoveredPick_->objectIndex);
        }
        hoverLine_ = hoverLabel;
    }

    if (ctx.shared && ctx.shared->combatSystem)
    {
        ctx.shared->combatSystem->update(deltaMs);
    }

    // Toggle inventory
    if (ctx.isKeyPressed(SDL_SCANCODE_I))
    {
        inventory_.setVisible(!inventory_.visible());
        if (inventory_.visible())
        {
            inventory_.setActiveCharacter(findActivePartyMember());
            characterStats_.setVisible(false);
            spellbook_.setVisible(false);
            restWidget_.setVisible(false);
            mapWidget_.setVisible(false);
        }
    }

    // Toggle character stats
    if (ctx.isKeyPressed(SDL_SCANCODE_C))
    {
        characterStats_.setVisible(!characterStats_.visible());
        if (characterStats_.visible())
        {
            characterStats_.setActiveCharacter(findActivePartyMember());
            inventory_.setVisible(false);
            spellbook_.setVisible(false);
            restWidget_.setVisible(false);
            mapWidget_.setVisible(false);
        }
    }

    // Toggle spellbook
    if (ctx.isKeyPressed(SDL_SCANCODE_S))
    {
        spellbook_.setVisible(!spellbook_.visible());
        if (spellbook_.visible())
        {
            spellbook_.setActiveCharacter(findActivePartyMember());
            inventory_.setVisible(false);
            characterStats_.setVisible(false);
            restWidget_.setVisible(false);
            mapWidget_.setVisible(false);
        }
    }

    // Spellbook keyboard navigation (Up/Down/Enter/Esc) when open.
    if (spellbook_.visible())
    {
        for (const SDL_Scancode key :
             {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE,
              SDL_SCANCODE_ESCAPE, SDL_SCANCODE_1, SDL_SCANCODE_2})
        {
            if (ctx.isKeyPressed(key))
            {
                ui::UIEvent ev{ui::UIEventType::KeyDown, 0, 0, platform::MouseButton::Left, key};
                (void)spellbook_.handleEvent(ev);
            }
        }
    }

    // Toggle rest screen
    if (ctx.isKeyPressed(SDL_SCANCODE_R))
    {
        restWidget_.setVisible(!restWidget_.visible());
        if (restWidget_.visible())
        {
            inventory_.setVisible(false);
            characterStats_.setVisible(false);
            spellbook_.setVisible(false);
            mapWidget_.setVisible(false);
        }
    }

    // Toggle map screen
    if (ctx.isKeyPressed(SDL_SCANCODE_M))
    {
        mapWidget_.setVisible(!mapWidget_.visible());
        if (mapWidget_.visible())
        {
            inventory_.setVisible(false);
            characterStats_.setVisible(false);
            spellbook_.setVisible(false);
            restWidget_.setVisible(false);
            journalWidget_.setVisible(false);
        }
    }

    // Toggle quest journal
    if (ctx.isKeyPressed(SDL_SCANCODE_Q))
    {
        journalWidget_.setVisible(!journalWidget_.visible());
        if (journalWidget_.visible())
        {
            inventory_.setVisible(false);
            characterStats_.setVisible(false);
            spellbook_.setVisible(false);
            restWidget_.setVisible(false);
            mapWidget_.setVisible(false);
        }
    }

    // Journal keyboard navigation (Up/Down/Esc) when open.
    if (journalWidget_.visible())
    {
        for (const SDL_Scancode key : {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_ESCAPE})
        {
            if (ctx.isKeyPressed(key))
            {
                ui::UIEvent ev{ui::UIEventType::KeyDown, 0, 0, platform::MouseButton::Left, key};
                (void)journalWidget_.handleEvent(ev);
            }
        }
    }

    // Toggle render options
    if (ctx.isKeyPressed(SDL_SCANCODE_F))
    {
        renderOptions.showFloors = !renderOptions.showFloors;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_V))
    {
        renderOptions.showWalls = !renderOptions.showWalls;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_P))
    {
        renderOptions.showPortals = !renderOptions.showPortals;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_L))
    {
        renderOptions.showLights = !renderOptions.showLights;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_G))
    {
        showGrid = !showGrid;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_X))
    {
        showAxes = !showAxes;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_H))
    {
        showHelpOverlay = !showHelpOverlay;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_R) && ctx.camera)
    {
        if (ctx.shared && ctx.shared->mapScene && ctx.shared->mapScene->isLoaded())
        {
            const auto& bounds = ctx.shared->mapScene->getBounds();
            if (bounds.valid)
            {
                float distance = std::max(bounds.radius() * 2.5f, 1000.0f);
                ctx.camera->lookAt(bounds.center(), distance);
            }
        }
    }

    if (ctx.isKeyPressed(SDL_SCANCODE_F9) && ctx.shared && ctx.shared->eventEngine)
    {
        if (ctx.shared->eventEngine->triggerEvent(1))
        {
            statusLine_ = "Triggered event #1";
        }
        else
        {
            statusLine_ = "Event #1 not found";
        }
    }

    if (ctx.isKeyPressed(SDL_SCANCODE_F5))
    {
        if (quickSaveToSlot(0))
        {
            statusLine_ = "Saved to slot 1";
        }
        else
        {
            statusLine_ = "Save failed (slot 1)";
        }
    }

    if (ctx.isKeyPressed(SDL_SCANCODE_F8))
    {
        if (quickLoadFromSlot(0))
        {
            statusLine_ = "Loaded slot 1";
            return GameStateId::Loading;
        }
        statusLine_ = "Load failed (slot 1)";
    }

    // A travel service requested a map transition; hand off to the Loading state.
    if (pendingTravel_)
    {
        pendingTravel_ = false;
        return GameStateId::Loading;
    }

    // HUD portrait click: select the active party member. Only when no panel is
    // open and we are not awaiting a spell target.
    const bool noPanelOpen = !inventory_.visible() && !spellbook_.visible() &&
                             !characterStats_.visible() && !restWidget_.visible() &&
                             !mapWidget_.visible() && !journalWidget_.visible() &&
                             !dialogue_.isOpen() && !shopWindow_.isOpen();
    if (noPanelOpen && !targetingActive_ &&
        ctx.window.wasMousePressed(platform::MouseButton::Left) && ctx.shared &&
        ctx.shared->gameWorld)
    {
        const auto ms = ctx.window.getMouseState();
        const float hudScale =
            std::min(static_cast<float>(ctx.viewportWidth) / static_cast<float>(kGameWidth),
                     static_cast<float>(ctx.viewportHeight) / static_cast<float>(kGameHeight));
        const float hudOffsetX =
            (ctx.viewportWidth - static_cast<float>(kGameWidth) * hudScale) * 0.5f;
        const float hudOffsetY =
            (ctx.viewportHeight - static_cast<float>(kGameHeight) * hudScale) * 0.5f;
        const int portrait = hud_.portraitAt(hudScale, hudOffsetX, hudOffsetY, ms.x, ms.y);
        if (portrait >= 0)
        {
            auto& party = ctx.shared->gameWorld->party();
            if (party.member(portrait).isConscious())
            {
                party.setActiveMemberIndex(portrait);
                statusLine_ = std::format("Selected {}.", party.member(portrait).name);
            }
            else
            {
                statusLine_ = std::format("{} cannot act.", party.member(portrait).name);
            }
        }
    }

    // Spell targeting: resolve the queued spell on the next click. SingleAlly
    // targets a party portrait; SingleEnemy targets the monster under the
    // cursor. Esc cancels.
    if (targetingActive_ && ctx.shared && ctx.shared->gameWorld)
    {
        if (ctx.isKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            targetingActive_ = false;
            statusLine_ = "Cancelled spell.";
        }
        else if (ctx.window.wasMousePressed(platform::MouseButton::Left))
        {
            const int caster = findActivePartyMember();
            const auto ms = ctx.window.getMouseState();
            const float tScale =
                std::min(static_cast<float>(ctx.viewportWidth) / static_cast<float>(kGameWidth),
                         static_cast<float>(ctx.viewportHeight) / static_cast<float>(kGameHeight));
            const float tOffsetX =
                (ctx.viewportWidth - static_cast<float>(kGameWidth) * tScale) * 0.5f;
            const float tOffsetY =
                (ctx.viewportHeight - static_cast<float>(kGameHeight) * tScale) * 0.5f;

            bool resolved = false;
            if (pendingTargetType_ == game::SpellTarget::SingleAlly)
            {
                const int portrait = hud_.portraitAt(tScale, tOffsetX, tOffsetY, ms.x, ms.y);
                if (portrait >= 0 && caster >= 0 && ctx.shared->spellSystem)
                {
                    auto r =
                        ctx.shared->spellSystem->castHealSpell(caster, selectedSpellId_, portrait);
                    statusLine_ = r.description;
                    resolved = true;
                }
            }
            else if (pendingTargetType_ == game::SpellTarget::SingleEnemy && caster >= 0 &&
                     ctx.shared->combatSystem && ctx.shared->spellSystem)
            {
                const int monsterIdx = pickMonsterUnderCursor();
                if (monsterIdx >= 0)
                {
                    auto* target = ctx.shared->combatSystem->getMonster(monsterIdx);
                    if (target && target->isAlive())
                    {
                        ctx.shared->combatSystem->setInCombat(true);
                        auto r = ctx.shared->spellSystem->castDamageSpell(caster, selectedSpellId_,
                                                                          target);
                        statusLine_ = r.description;
                        resolved = true;
                    }
                }
            }

            if (!resolved)
            {
                statusLine_ = "No valid target.";
            }
            targetingActive_ = false;
        }
        return std::nullopt; // swallow input while targeting
    }

    // Inventory panel input: dispatch mouse clicks to the widget so the player
    // can equip/unequip items. When the panel is open it consumes clicks (the
    // click-pickup/place model from FUN_00468F8E), so they must not fall through
    // to world-picking/combat below.
    if (inventory_.visible() && ctx.window.wasMousePressed(platform::MouseButton::Left))
    {
        // Keep the inventory's party pointer current so the skill gate applies.
        if (ctx.shared && ctx.shared->gameWorld && ctx.shared->inventory)
        {
            ctx.shared->inventory->setParty(&ctx.shared->gameWorld->party());
        }
        const auto ms = ctx.window.getMouseState();
        ui::UIEvent ev{ui::UIEventType::MouseDown, ms.x, ms.y, platform::MouseButton::Left,
                       SDL_SCANCODE_UNKNOWN};
        (void)inventory_.handleEvent(ev);
        // Skip the world interaction while the inventory is open.
        return std::nullopt;
    }

    const bool primaryActionPressed = ctx.window.wasMousePressed(platform::MouseButton::Left) ||
                                      ctx.isKeyPressed(SDL_SCANCODE_SPACE) ||
                                      ctx.isKeyPressed(SDL_SCANCODE_A);
    if (ctx.shared && primaryActionPressed)
    {
        bool handled = false;
        if (ctx.shared->combatSystem)
        {
            selectedMonsterIndex_ = pickMonsterUnderCursor();
            if (selectedMonsterIndex_ >= 0)
            {
                const int attacker = findActivePartyMember();
                if (attacker >= 0)
                {
                    ctx.shared->combatSystem->setInCombat(true);
                    auto result =
                        ctx.shared->combatSystem->playerAttack(attacker, selectedMonsterIndex_);
                    statusLine_ = result.description;
                    handled = true;
                }
            }
        }

        if (!handled && ctx.shared->eventEngine)
        {
            const int eventId = hoveredPick_.has_value() ? hoveredPick_->eventId : 0;
            if (eventId > 0)
            {
                if (ctx.shared->gameWorld && hoveredPick_.has_value())
                {
                    ctx.shared->gameWorld->setLastEventInteraction({
                        .eventId = eventId,
                        .type = toInteractionType(hoveredPick_->type),
                        .objectIndex = hoveredPick_->objectIndex,
                    });
                }

                const bool triggered = ctx.shared->eventEngine->triggerEvent(eventId);
                if (ctx.shared->gameWorld)
                {
                    ctx.shared->gameWorld->clearLastEventInteraction();
                }

                if (triggered)
                {
                    statusLine_ = std::format("Triggered map event #{}", eventId);
                }
                else
                {
                    statusLine_ = std::format("Map event #{} not found", eventId);
                }
                handled = true;
            }
        }

        if (!handled && hoveredPick_.has_value() && ctx.shared->inventory &&
            ctx.shared->gameWorld && ctx.shared->mapScene &&
            hoveredPick_->type == graphics::PickObjectType::MapItem)
        {
            const std::string mapName = ctx.shared->mapScene->getName();
            const auto* spawned = ctx.shared->gameWorld->getSpawnedMapItems(mapName);
            const int objectIndex = hoveredPick_->objectIndex;
            if (spawned && objectIndex >= 0 && static_cast<size_t>(objectIndex) < spawned->size())
            {
                const auto& worldItem = (*spawned)[static_cast<size_t>(objectIndex)];
                const int requestCount = std::max(1, worldItem.count);
                int pickedCount = 0;

                if (worldItem.itemType > 0)
                {
                    game::Item item;
                    item.itemId = worldItem.itemType;
                    for (int i = 0; i < requestCount; i++)
                    {
                        if (!ctx.shared->inventory->giveItem(item))
                        {
                            break;
                        }
                        pickedCount++;
                    }
                }
                else
                {
                    ctx.shared->gameWorld->party().addGold(requestCount);
                    pickedCount = requestCount;
                }

                if (pickedCount > 0)
                {
                    auto updatedItems = *spawned;
                    if (pickedCount >= requestCount)
                    {
                        updatedItems.erase(updatedItems.begin() + objectIndex);
                    }
                    else
                    {
                        updatedItems[static_cast<size_t>(objectIndex)].count =
                            requestCount - pickedCount;
                    }
                    ctx.shared->gameWorld->setSpawnedMapItems(mapName, std::move(updatedItems));
                    if (worldItem.itemType > 0)
                    {
                        statusLine_ =
                            std::format("Picked up item #{} x{}", worldItem.itemType, pickedCount);
                    }
                    else
                    {
                        statusLine_ = std::format("Picked up {} gold", pickedCount);
                    }
                    hoveredPick_.reset();
                    hoveredEventId_ = 0;
                    hoverLine_.clear();
                    handled = true;
                }
                else
                {
                    statusLine_ = "Inventory is full";
                    handled = true;
                }
            }
        }
    }

    if (ctx.shared && ctx.shared->combatSystem && ctx.shared->spellSystem &&
        ctx.window.wasMousePressed(platform::MouseButton::Right))
    {
        if (selectedMonsterIndex_ < 0)
        {
            selectedMonsterIndex_ = pickMonsterUnderCursor();
        }

        if (selectedMonsterIndex_ >= 0)
        {
            auto* target = ctx.shared->combatSystem->getMonster(selectedMonsterIndex_);
            const int caster = findActivePartyMember();
            if (caster >= 0 && target && target->isAlive())
            {
                const int spellId = findFirstDamageSpell(caster);
                if (spellId > 0)
                {
                    ctx.shared->combatSystem->setInCombat(true);
                    auto result = ctx.shared->spellSystem->castDamageSpell(caster, spellId, target);
                    statusLine_ = result.description;
                }
            }
        }
    }

    return std::nullopt;
}

void InGameState::render()
{
    if (!ctx.shared)
    {
        return;
    }

    bool mapLoaded = ctx.shared->mapScene && ctx.shared->mapScene->isLoaded();
    if (mapLoaded && ctx.worldRenderer && ctx.camera)
    {
        const graphics::Rect worldVp = worldViewportRect();
        ctx.camera->setAspectRatio(static_cast<float>(worldVp.width) /
                                   static_cast<float>(worldVp.height));

        const auto* runtimeConfig =
            ctx.shared->gameWorld ? &ctx.shared->gameWorld->runtimeConfig() : nullptr;
        const float nightBlend =
            ctx.shared->gameWorld ? ctx.shared->gameWorld->calendar().nightBlend() : 0.0f;

        SDL_Renderer* sdlRenderer = ctx.renderer ? ctx.renderer->getSDLRenderer() : nullptr;
        if (sdlRenderer)
        {
            const SDL_Rect viewport{worldVp.x, worldVp.y, worldVp.width, worldVp.height};
            SDL_SetRenderViewport(sdlRenderer, &viewport);
            ctx.worldRenderer->render(*ctx.shared->mapScene, *ctx.camera, runtimeConfig,
                                      nightBlend);
            SDL_SetRenderViewport(sdlRenderer, nullptr);
        }
        else
        {
            ctx.worldRenderer->render(*ctx.shared->mapScene, *ctx.camera, runtimeConfig,
                                      nightBlend);
        }
    }

    if (ctx.renderer && ctx.debugText && ctx.shared->gameWorld)
    {
        float scale =
            std::min(static_cast<float>(ctx.viewportWidth) / static_cast<float>(kGameWidth),
                     static_cast<float>(ctx.viewportHeight) / static_cast<float>(kGameHeight));
        float offsetX = (ctx.viewportWidth - static_cast<float>(kGameWidth) * scale) * 0.5f;
        float offsetY = (ctx.viewportHeight - static_cast<float>(kGameHeight) * scale) * 0.5f;
        hud_.setGameWorld(ctx.shared->gameWorld);
        hud_.setMapScene(ctx.shared->mapScene);
        hud_.render(*ctx.renderer, *ctx.debugText, scale, offsetX, offsetY);

        inventory_.setGameWorld(ctx.shared->gameWorld);
        inventory_.setInventory(ctx.shared->inventory);
        inventory_.setSpellSystem(ctx.shared->spellSystem);
        inventory_.setBounds(static_cast<int>(offsetX), static_cast<int>(offsetY),
                             static_cast<int>(kGameWidth * scale),
                             static_cast<int>(kGameHeight * scale));
        inventory_.render(*ctx.renderer, *ctx.debugText);

        characterStats_.setGameWorld(ctx.shared->gameWorld);
        characterStats_.setBounds(static_cast<int>(offsetX), static_cast<int>(offsetY),
                                  static_cast<int>(kGameWidth * scale),
                                  static_cast<int>(kGameHeight * scale));
        characterStats_.render(*ctx.renderer, *ctx.debugText);

        spellbook_.setGameWorld(ctx.shared->gameWorld);
        spellbook_.setSpellSystem(ctx.shared->spellSystem);
        spellbook_.setBounds(static_cast<int>(offsetX), static_cast<int>(offsetY),
                             static_cast<int>(kGameWidth * scale),
                             static_cast<int>(kGameHeight * scale));
        spellbook_.render(*ctx.renderer, *ctx.debugText);

        restWidget_.setGameWorld(ctx.shared->gameWorld);
        restWidget_.setCombatSystem(ctx.shared->combatSystem);
        restWidget_.setStatusCallback(
            [this](const std::string& msg)
            {
                statusLine_ = msg;
                if (ctx.shared)
                    ctx.shared->statusMessage = msg;
            });
        restWidget_.setBounds(static_cast<int>(offsetX), static_cast<int>(offsetY),
                              static_cast<int>(kGameWidth * scale),
                              static_cast<int>(kGameHeight * scale));
        restWidget_.render(*ctx.renderer, *ctx.debugText);

        mapWidget_.setGameWorld(ctx.shared->gameWorld);
        mapWidget_.setMapScene(ctx.shared->mapScene);
        mapWidget_.setBounds(static_cast<int>(offsetX), static_cast<int>(offsetY),
                             static_cast<int>(kGameWidth * scale),
                             static_cast<int>(kGameHeight * scale));
        mapWidget_.render(*ctx.renderer, *ctx.debugText);

        journalWidget_.setGameWorld(ctx.shared->gameWorld);
        journalWidget_.setQuestLog(ctx.shared->questLog);
        journalWidget_.setBounds(static_cast<int>(offsetX), static_cast<int>(offsetY),
                                 static_cast<int>(kGameWidth * scale),
                                 static_cast<int>(kGameHeight * scale));
        journalWidget_.render(*ctx.renderer, *ctx.debugText);
    }

    renderOverlay();
    if (ctx.renderer && ctx.debugText && dialogue_.isOpen())
    {
        dialogue_.render(*ctx.renderer, *ctx.debugText, ctx.viewportWidth, ctx.viewportHeight);
    }
    if (ctx.renderer && ctx.debugText && shopWindow_.isOpen())
    {
        shopWindow_.setContext(
            (ctx.shared && ctx.shared->gameWorld) ? &ctx.shared->gameWorld->party() : nullptr,
            ctx.shared ? ctx.shared->inventory : nullptr);
        shopWindow_.render(*ctx.renderer, *ctx.debugText, ctx.viewportWidth, ctx.viewportHeight);
    }
}

void InGameState::updateCameraInput(float deltaMs)
{
    if (!ctx.camera || !ctx.shared || !ctx.shared->mapScene || !ctx.shared->gameWorld)
    {
        return;
    }

    auto& party = ctx.shared->gameWorld->party();
    const auto& config = ctx.shared->gameWorld->runtimeConfig();

    const float frameScale = std::max(0.0f, deltaMs) / 16.0f;
    float turnStep = 16.0f * frameScale; // MM7 angle units per 16 ms tick.
    float moveSpeed = std::max(1.0f, static_cast<float>(config.walkSpeed));

    if (ctx.isKeyDown(SDL_SCANCODE_LSHIFT) || ctx.isKeyDown(SDL_SCANCODE_RSHIFT))
    {
        turnStep *= 2.0f;
        moveSpeed *= 2.0f;
    }

    // Party Orientation (Yaw/Pitch in MM7 0-2047 units)
    float yaw = party.yaw();
    float pitch = party.pitch();

    const auto keyDown = [this](SDL_Scancode primary, SDL_Scancode alternate)
    { return ctx.isKeyDown(primary) || ctx.isKeyDown(alternate); };

    if (keyDown(SDL_SCANCODE_LEFT, SDL_SCANCODE_KP_4))
    {
        yaw += turnStep;
    }
    if (keyDown(SDL_SCANCODE_RIGHT, SDL_SCANCODE_KP_6))
    {
        yaw -= turnStep;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_PAGEUP))
    {
        pitch += turnStep;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_PAGEDOWN))
    {
        pitch -= turnStep;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_HOME))
    {
        pitch = 0.0f;
    }

    while (yaw >= 2048.0f)
    {
        yaw -= 2048.0f;
    }
    while (yaw < 0.0f)
    {
        yaw += 2048.0f;
    }
    pitch = std::clamp(pitch, -512.0f, 512.0f);

    party.setOrientation(yaw, pitch);

    // Party Movement
    const float yawRad = yaw * M_PI / 1024.0f;
    const float dx = std::cos(yawRad);
    const float dy = std::sin(yawRad);

    float forward = 0.0f;
    float strafe = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;

    if (keyDown(SDL_SCANCODE_UP, SDL_SCANCODE_KP_8))
    {
        forward += 1.0f;
    }
    if (keyDown(SDL_SCANCODE_DOWN, SDL_SCANCODE_KP_2))
    {
        forward -= 0.5f;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_INSERT))
    {
        strafe -= 0.75f;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_DELETE))
    {
        strafe += 0.75f;
    }

    const float inputLength = std::hypot(forward, strafe);
    if (inputLength > 1.0f)
    {
        forward /= inputLength;
        strafe /= inputLength;
    }

    vx = (dx * forward + dy * strafe) * moveSpeed;
    vy = (dy * forward - dx * strafe) * moveSpeed;

    party.setVelocityX(vx);
    party.setVelocityY(vy);

    float px = party.worldX();
    float py = party.worldY();
    float pz = party.worldZ();

    if (ctx.shared->mapScene)
    {
        PhysicsConfig physConfig;
        physConfig.playerHeight = static_cast<float>(config.partyHeight);

        const formats::BLVMapData* blv = nullptr;
        const formats::ODMMapData* odm = nullptr;

        if (ctx.shared->gameWorld->isIndoorMap())
            blv = &ctx.shared->mapScene->getBLVData();
        else
            odm = &ctx.shared->mapScene->getODMData();

        updatePartyPhysics(party, blv, odm, deltaMs, physConfig);

        px = party.worldX();
        py = party.worldY();
        pz = party.worldZ();
    }

    const float pitchRad = pitch * M_PI / 1024.0f;
    const float dz = std::sin(pitchRad);

    const graphics::Vec3 eye =
        graphics::gameplayToRenderPosition(px, py, pz + config.partyEyeLevel);
    const graphics::Vec3 lookTarget = graphics::gameplayToRenderPosition(
        px + dx * 100.0f, py + dy * 100.0f, pz + config.partyEyeLevel + dz * 100.0f);
    ctx.camera->setTarget(lookTarget);
    ctx.camera->setPosition(eye);
}

int InGameState::pickMonsterUnderCursor() const
{
    if (!ctx.shared || !ctx.shared->combatSystem || !ctx.camera)
    {
        return -1;
    }

    const auto ms = ctx.window.getMouseState();
    int mouseX = 0;
    int mouseY = 0;
    if (!mapMouseToWorldViewport(ms.x, ms.y, mouseX, mouseY))
    {
        return -1;
    }

    const graphics::Rect worldVp = worldViewportRect();
    const auto& viewProjection = ctx.camera->getViewProjectionMatrix();
    std::vector<graphics::PickCandidate> candidates;

    const auto& monsters = ctx.shared->combatSystem->getMonsters();
    candidates.reserve(monsters.size());
    for (int i = 0; i < static_cast<int>(monsters.size()); i++)
    {
        const auto& monster = monsters[static_cast<size_t>(i)];
        if (!monster.isAlive())
        {
            continue;
        }
        candidates.push_back(
            {i, graphics::gameplayToRenderPosition(monster.x, monster.y, monster.z)});
    }

    const auto hit = graphics::pickClosestProjectedPoint(
        viewProjection, worldVp.width, worldVp.height, mouseX, mouseY, candidates, 28.0f);
    return hit.has_value() ? hit->id : -1;
}

int InGameState::pickMapEventUnderCursor() const
{
    const auto hit = pickMapObjectUnderCursor(true);
    return hit.has_value() ? hit->eventId : 0;
}

std::optional<graphics::PickHit> InGameState::pickMapObjectUnderCursor(bool requireEventId) const
{
    if (!ctx.shared || !ctx.shared->mapScene || !ctx.camera || !ctx.worldRenderer ||
        !ctx.shared->mapScene->isLoaded())
    {
        return std::nullopt;
    }

    const auto ms = ctx.window.getMouseState();
    int mouseX = 0;
    int mouseY = 0;
    if (!mapMouseToWorldViewport(ms.x, ms.y, mouseX, mouseY))
    {
        return std::nullopt;
    }

    const graphics::Rect worldVp = worldViewportRect();
    graphics::PickSelectionFilter filter;
    filter.requireEventId = requireEventId;
    return ctx.worldRenderer->pickMapObject(worldVp.width, worldVp.height, mouseX, mouseY, 36.0f,
                                            filter);
}

int InGameState::findActivePartyMember() const
{
    if (!ctx.shared || !ctx.shared->gameWorld)
    {
        return -1;
    }

    // Prefer the player-selected active member if they can act (RE: the active
    // member at 0x507A6C drives casting/inventory/shops). Falls back to the
    // first conscious member otherwise.
    const auto& party = ctx.shared->gameWorld->party();
    const int selected = party.activeMemberIndex();
    if (selected >= 0 && selected < game::kPartySize && party.member(selected).isConscious())
    {
        return selected;
    }

    for (int i = 0; i < game::kPartySize; i++)
    {
        if (party.member(i).isConscious())
        {
            return i;
        }
    }
    return -1;
}

int InGameState::findFirstDamageSpell(int characterIndex) const
{
    if (!ctx.shared || !ctx.shared->spellSystem)
    {
        return 0;
    }

    const auto spells = ctx.shared->spellSystem->getAvailableSpells(characterIndex);
    for (int spellId : spells)
    {
        const auto* info = ctx.shared->spellSystem->getSpell(spellId);
        if (!info)
        {
            continue;
        }
        if (info->target == game::SpellTarget::SingleEnemy ||
            info->target == game::SpellTarget::AllEnemies ||
            info->target == game::SpellTarget::AreaOfEffect)
        {
            if (ctx.shared->spellSystem->canCast(characterIndex, spellId))
            {
                return spellId;
            }
        }
    }

    return 0;
}

void InGameState::preserveCurrentMapStateForSave()
{
    if (!ctx.shared || !ctx.shared->gameWorld || !ctx.shared->mapScene ||
        !ctx.shared->mapScene->isLoaded())
    {
        return;
    }

    const std::string mapName = ctx.shared->mapScene->getName();
    if (mapName.empty())
    {
        return;
    }

    game::SavedMapState state;

    const auto& blv = ctx.shared->mapScene->getBLVData();
    if (!blv.faces.empty())
    {
        state.indoorFaceAttributes.reserve(blv.faces.size());
        for (const auto& face : blv.faces)
        {
            state.indoorFaceAttributes.push_back(face.attributes);
        }
    }
    if (!blv.decorations.empty())
    {
        state.indoorDecorationHidden.reserve(blv.decorations.size());
        for (const auto& decoration : blv.decorations)
        {
            state.indoorDecorationHidden.push_back(decoration.hidden ? 1u : 0u);
        }
    }

    const auto& odm = ctx.shared->mapScene->getODMData();
    if (!odm.buildings.empty())
    {
        state.outdoorBuildingFaceAttributes.resize(odm.buildings.size());
        for (size_t bi = 0; bi < odm.buildings.size(); bi++)
        {
            const auto& building = odm.buildings[bi];
            auto& attrs = state.outdoorBuildingFaceAttributes[bi];
            attrs.reserve(building.faces.size());
            for (const auto& face : building.faces)
            {
                attrs.push_back(face.attributes);
            }
        }
    }

    ctx.shared->gameWorld->setSavedMapState(mapName, std::move(state));
}

bool InGameState::quickSaveToSlot(int slotIndex)
{
    if (!ctx.shared || !ctx.shared->saveGame || !ctx.shared->gameWorld)
    {
        return false;
    }

    preserveCurrentMapStateForSave();
    std::vector<uint8_t> eventRuntimeState;
    if (ctx.shared->eventEngine)
    {
        eventRuntimeState = ctx.shared->eventEngine->serializeRuntimeState();
    }
    return ctx.shared->saveGame->save(*ctx.shared->gameWorld, slotIndex,
                                      ctx.shared->eventEngine ? &eventRuntimeState : nullptr,
                                      ctx.shared->inventory);
}

bool InGameState::quickLoadFromSlot(int slotIndex)
{
    if (!ctx.shared || !ctx.shared->saveGame || !ctx.shared->gameWorld)
    {
        return false;
    }

    std::vector<uint8_t> eventRuntimeState;
    if (!ctx.shared->saveGame->load(*ctx.shared->gameWorld, slotIndex, &eventRuntimeState,
                                    ctx.shared->inventory))
    {
        return false;
    }

    const std::string mapName = ctx.shared->gameWorld->currentMap();
    if (mapName.empty())
    {
        return false;
    }

    ctx.shared->startupMapName = mapName;
    std::string ext = mapName.size() >= 4 ? mapName.substr(mapName.size() - 4) : "";
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    ctx.shared->startupPreferOutdoor = (ext == ".odm");
    ctx.shared->autoLoadMap = true;
    ctx.shared->loadFromSave = true;
    ctx.shared->hasPendingEventRuntimeState = !eventRuntimeState.empty();
    ctx.shared->pendingEventRuntimeState = std::move(eventRuntimeState);
    ctx.shared->statusMessage = std::format("Loaded slot {} ({})", slotIndex + 1, mapName);
    return true;
}

void InGameState::renderOverlay()
{
    if (!ctx.renderer || !ctx.renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
    if (hoveredPick_.has_value() && ctx.camera)
    {
        const graphics::Rect worldVp = worldViewportRect();
        const auto& vp = ctx.camera->getViewProjectionMatrix();
        const auto& wp = hoveredPick_->worldPos;
        const graphics::Vec4 clip = vp * graphics::Vec4(wp, 1.0f);
        if (clip.w > 0.001f)
        {
            const float ndcX = clip.x / clip.w;
            const float ndcY = clip.y / clip.w;
            if (ndcX >= -1.0f && ndcX <= 1.0f && ndcY >= -1.0f && ndcY <= 1.0f)
            {
                const float sx = static_cast<float>(worldVp.x) +
                                 (ndcX * 0.5f + 0.5f) * static_cast<float>(worldVp.width);
                const float sy = static_cast<float>(worldVp.y) +
                                 (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(worldVp.height);
                const SDL_Color markerColor = pickTypeColor(hoveredPick_->type);
                const float r = 10.0f;

                SDL_SetRenderDrawColor(sdlRenderer, markerColor.r, markerColor.g, markerColor.b,
                                       220);
                SDL_FRect top = {sx - r, sy - r, 2.0f * r, 2.0f};
                SDL_FRect bottom = {sx - r, sy + r - 2.0f, 2.0f * r, 2.0f};
                SDL_FRect left = {sx - r, sy - r, 2.0f, 2.0f * r};
                SDL_FRect right = {sx + r - 2.0f, sy - r, 2.0f, 2.0f * r};
                SDL_RenderFillRect(sdlRenderer, &top);
                SDL_RenderFillRect(sdlRenderer, &bottom);
                SDL_RenderFillRect(sdlRenderer, &left);
                SDL_RenderFillRect(sdlRenderer, &right);
            }
        }
    }

    if (!ctx.debugText)
    {
        return;
    }

    const bool showFrameRate = (ctx.shared && ctx.shared->showFrameRate);
    if (!showHelpOverlay)
    {
        if (showFrameRate)
        {
            const int scale = 2;
            const std::string fpsLine = std::format("FPS: {:.1f}", displayedFps_);
            const int padding = 6;
            const int boxWidth =
                ctx.debugText->charWidth(scale) * static_cast<int>(fpsLine.size()) + padding * 2;
            const int boxHeight = ctx.debugText->lineHeight(scale) + padding * 2;
            SDL_FRect panel = {10.0f, 10.0f, static_cast<float>(boxWidth),
                               static_cast<float>(boxHeight)};
            SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 180);
            SDL_RenderFillRect(sdlRenderer, &panel);
            ctx.debugText->drawText(sdlRenderer, static_cast<int>(panel.x) + padding,
                                    static_cast<int>(panel.y) + padding, scale, 230, 230, 230,
                                    fpsLine);
        }
        return;
    }

    std::vector<std::string> lines;

    if (ctx.shared && ctx.shared->mapScene && ctx.shared->mapScene->isLoaded())
    {
        auto& mapScene = *ctx.shared->mapScene;
        if (mapScene.getODMData().heightmap.empty())
        {
            const auto& data = mapScene.getBLVData();
            lines.push_back("MAP: " + toUpper(mapScene.getName()));
            lines.push_back(std::format("VERTS: {}  FACES: {}  LIGHTS: {}", data.vertices.size(),
                                        data.faces.size(), data.lights.size()));
        }
        else
        {
            const auto& data = mapScene.getODMData();
            lines.push_back("MAP: " + toUpper(mapScene.getName()));
            lines.push_back(
                std::format("TERRAIN: {}x{}  BUILDINGS: {}",
                            data.heightmap.size() > 0 ? formats::ODMMapData::TERRAIN_SIZE : 0,
                            data.heightmap.size() > 0 ? formats::ODMMapData::TERRAIN_SIZE : 0,
                            data.buildings.size()));
        }
    }
    else
    {
        lines.push_back("MAP: (NONE)");
    }

    lines.push_back("ARROWS MOVE/TURN  PGUP/PGDN LOOK  INS/DEL STRAFE  SHIFT RUN");
    lines.push_back("SPACE/A ACTION  I INV  C CHARS  S SPELLS  R REST  M MAP  Q QUESTS");
    lines.push_back("CLICK PORTRAIT: SELECT MEMBER  S: SPELLBOOK -> CLICK SPELL -> CLICK TARGET");
    lines.push_back("RMB: QUICK DAMAGE SPELL  (TARGETING: ESC CANCELS)");
    lines.push_back(std::format("F FLOORS:{}  V WALLS:{}  C CEIL:{}  P PORTAL:{}",
                                onOff(renderOptions.showFloors), onOff(renderOptions.showWalls),
                                onOff(renderOptions.showCeilings),
                                onOff(renderOptions.showPortals)));
    lines.push_back(std::format("L LIGHTS:{}  G GRID:{}  X AXES:{}  H HELP:{}",
                                onOff(renderOptions.showLights), onOff(showGrid), onOff(showAxes),
                                onOff(showHelpOverlay)));
    if (ctx.shared && ctx.shared->combatSystem)
    {
        lines.push_back(std::format("MONSTERS:{}  PICK:{}  F9 EVT#1  F5 SAVE  F8 LOAD",
                                    ctx.shared->combatSystem->aliveMonsterCount(),
                                    selectedMonsterIndex_));
    }
    if (ctx.shared && ctx.shared->gameWorld)
    {
        const auto& runtime = ctx.shared->gameWorld->runtimeConfig();
        const float nightBlend = ctx.shared->gameWorld->calendar().nightBlend();
        lines.push_back(std::format("WALK:{}  PARTY_H:{}  EYE:{}  MIST:{}", runtime.walkSpeed,
                                    runtime.partyHeight, runtime.partyEyeLevel,
                                    onOff(!runtime.noMist)));
        lines.push_back(std::format("LOD:{}|{}|{}  SHADE:{}|{}|{}", runtime.gridBand1,
                                    runtime.gridBand2, runtime.gridBand3, runtime.distShade,
                                    runtime.distShadeMist, runtime.distMist));
        lines.push_back(std::format("NIGHT_BLEND:{:.2f}", nightBlend));
    }
    if (showFrameRate)
    {
        lines.push_back(std::format("FPS: {:.1f}", displayedFps_));
    }
    if (!statusLine_.empty())
    {
        lines.push_back(statusLine_);
    }
    if (!hoverLine_.empty())
    {
        lines.push_back(hoverLine_);
    }

    const int scale = 2;
    int maxLen = 0;
    for (const auto& line : lines)
    {
        maxLen = std::max(maxLen, static_cast<int>(line.size()));
    }

    const int padding = 8;
    int boxWidth = ctx.debugText->charWidth(scale) * maxLen + padding * 2;
    int boxHeight = ctx.debugText->lineHeight(scale) * static_cast<int>(lines.size()) + padding * 2;

    SDL_FRect panel = {10.0f, 10.0f, static_cast<float>(boxWidth), static_cast<float>(boxHeight)};
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 180);
    SDL_RenderFillRect(sdlRenderer, &panel);

    int cursorY = static_cast<int>(panel.y) + padding;
    for (const auto& line : lines)
    {
        ctx.debugText->drawText(sdlRenderer, static_cast<int>(panel.x) + padding, cursorY, scale,
                                230, 230, 230, line);
        cursorY += ctx.debugText->lineHeight(scale);
    }
}

} // namespace runeharbor::engine
