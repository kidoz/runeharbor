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
#include "../../graphics/world_renderer.hpp"

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
}

void InGameState::exit()
{
    dialogue_.close();
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

    updateCameraInput();
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

    if (ctx.shared && ctx.shared->combatSystem &&
        ctx.window.wasMousePressed(platform::MouseButton::Left))
    {
        bool handled = false;
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
        spellbook_.setBounds(static_cast<int>(offsetX), static_cast<int>(offsetY),
                             static_cast<int>(kGameWidth * scale),
                             static_cast<int>(kGameHeight * scale));
        spellbook_.render(*ctx.renderer, *ctx.debugText);

        restWidget_.setGameWorld(ctx.shared->gameWorld);
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
    }

    renderOverlay();
    if (ctx.renderer && ctx.debugText && dialogue_.isOpen())
    {
        dialogue_.render(*ctx.renderer, *ctx.debugText, ctx.viewportWidth, ctx.viewportHeight);
    }
}

void InGameState::updateCameraInput()
{
    if (!ctx.camera || !ctx.shared || !ctx.shared->mapScene || !ctx.shared->gameWorld)
    {
        return;
    }

    auto& party = ctx.shared->gameWorld->party();
    const auto& config = ctx.shared->gameWorld->runtimeConfig();

    float orbitSpeed = 16.0f; // MM7 rotation units per frame
    float panSpeed = std::max(1.0f, config.walkSpeed / 2.0f);

    if (ctx.isKeyDown(SDL_SCANCODE_LSHIFT) || ctx.isKeyDown(SDL_SCANCODE_RSHIFT))
    {
        orbitSpeed *= 2.0f;
        panSpeed *= 2.0f;
    }

    // Party Orientation (Yaw/Pitch in MM7 0-2047 units)
    float yaw = party.yaw();
    float pitch = party.pitch();

    if (ctx.isKeyDown(SDL_SCANCODE_LEFT))
    {
        yaw -= orbitSpeed;
        if (yaw < 0) yaw += 2048.0f;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_RIGHT))
    {
        yaw += orbitSpeed;
        if (yaw >= 2048.0f) yaw -= 2048.0f;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_UP))
    {
        pitch += orbitSpeed;
        pitch = std::min(pitch, 512.0f); // Limit looking up
    }
    if (ctx.isKeyDown(SDL_SCANCODE_DOWN))
    {
        pitch -= orbitSpeed;
        pitch = std::max(pitch, -512.0f); // Limit looking down
    }

    party.setOrientation(yaw, pitch);

    // Party Movement
    float px = party.worldX();
    float py = party.worldY();
    float pz = party.worldZ();

    float yawRad = yaw * M_PI / 1024.0f;
    float dx = std::cos(yawRad);
    float dy = std::sin(yawRad);

    // Simple forward/back and strafe
    if (ctx.isKeyDown(SDL_SCANCODE_W))
    {
        px += dx * panSpeed;
        py += dy * panSpeed;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_S))
    {
        px -= dx * panSpeed;
        py -= dy * panSpeed;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_A))
    {
        px -= dy * panSpeed;
        py += dx * panSpeed;
    }
    if (ctx.isKeyDown(SDL_SCANCODE_D))
    {
        px += dy * panSpeed;
        py -= dx * panSpeed;
    }

    party.setWorldPosition(px, py, pz);

    // Sync camera to party
    graphics::Vec3 target = {px, py, pz + config.partyEyeLevel};
    ctx.camera->setTarget(target);
    ctx.camera->setPosition(target);
    
    // Convert MM7 angle (0=East, CCW) to camera yaw
    // For our camera, we probably just pass the radians. Let's reset and orbit to set absolute angles.
    // Wait, camera.orbit applies a delta. Our camera has no absolute `setRotation` yet.
    // Let's check if Camera has a setYaw/Pitch or if we can recreate it.
    // For now, we will add a method or recreate the view matrix. 
    // Actually, earlier we saw Camera had `yaw` and `pitch` private members but `orbit` adds to them.
    // We can just rely on the party position and calculate a "lookAt" target slightly ahead.
    
    graphics::Vec3 lookTarget = {
        px + dx * 100.0f,
        py + dy * 100.0f,
        pz + config.partyEyeLevel + static_cast<float>(std::sin(pitch * M_PI / 1024.0f)) * 100.0f
    };
    ctx.camera->lookAt(lookTarget, 0.0f);
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
        candidates.push_back({i, graphics::Vec3{monster.x, monster.y, monster.z}});
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

    for (int i = 0; i < game::kPartySize; i++)
    {
        if (ctx.shared->gameWorld->party().member(i).isConscious())
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

    lines.push_back("ARROWS ORBIT  Q/E ZOOM  WASD PAN  R RESET");
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
