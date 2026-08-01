// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../engine/map_scene.hpp"
#include "../game/game_world.hpp"
#include "../graphics/visibility.hpp"
#include "widgets.hpp"

namespace runeharbor::ui
{

class MapWidget : public Widget
{
  public:
    MapWidget();

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup) { textureLookup_ = lookup; }

    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setMapScene(engine::MapScene* mapScene) { mapScene_ = mapScene; }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;
    bool handleEvent(const UIEvent& event) override;

    void setBackground(void* tex, int w, int h)
    {
        bgTexture_ = tex;
        bgW_ = w;
        bgH_ = h;
    }

    // Fog-of-war persistence. syncExploredFromWorld loads the explored-sector
    // set for the current map from GameWorld's SavedMapState (call on map
    // load / before rendering). syncExploredToWorld writes the live set back
    // (call on save / panel close) so it survives across saves and map transitions.
    void syncExploredFromWorld();
    void syncExploredToWorld();

  private:
    // Mark the party's current sector explored (indoor only). Called each render.
    void markCurrentSectorExplored();

    game::GameWorld* gameWorld_ = nullptr;
    engine::MapScene* mapScene_ = nullptr;
    TextureLookup textureLookup_;

    // Live explored-sector set for the current indoor map (fog-of-war).
    std::unordered_set<uint16_t> exploredSectors_;
    graphics::PortalVisibility visibility_;
    std::string exploredMapName_; // which map exploredSectors_ belongs to

    void* bgTexture_ = nullptr;
    int bgW_ = 0, bgH_ = 0;
};

} // namespace runeharbor::ui
