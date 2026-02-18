// SPDX-License-Identifier: MIT
#pragma once

#include "../engine/map_scene.hpp"
#include "camera.hpp"
#include "indoor_renderer.hpp"
#include "outdoor_renderer.hpp"
#include "sdl_renderer.hpp"

namespace runeharbor::graphics
{

class WorldRenderer
{
  public:
    WorldRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~WorldRenderer();

    void setTextureLookup(TextureLookup lookup);
    void render(const engine::MapScene& scene, const Camera& camera);

  private:
    std::unique_ptr<IndoorRenderer> indoorRenderer;
    std::unique_ptr<OutdoorRenderer> outdoorRenderer;
};

} // namespace runeharbor::graphics
