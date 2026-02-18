// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>

#include "../engine/map_scene.hpp"
#include "camera.hpp"
#include "sdl_renderer.hpp"

struct SDL_Texture;

namespace runeharbor::graphics
{

class OutdoorRenderer
{
  public:
    using TextureLookup = std::function<SDL_Texture*(const std::string& name)>;

    OutdoorRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~OutdoorRenderer();

    void setTextureLookup(TextureLookup lookup);
    void render(const engine::MapScene& scene, const Camera& camera);

  private:
    void renderTerrain(const formats::ODMMapData& odmData, const Camera& camera);
    void renderBuildings(const formats::ODMMapData& odmData, const Camera& camera);

    SDLRenderer& renderer;
    util::ILogger& logger;
    TextureLookup textureLookup;
};

} // namespace runeharbor::graphics
