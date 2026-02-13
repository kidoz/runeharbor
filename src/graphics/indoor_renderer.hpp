// SPDX-License-Identifier: MIT
#pragma once

#include "camera.hpp"
#include "sdl_renderer.hpp"
#include "../engine/map_scene.hpp"

namespace runeharbor::graphics
{

class IndoorRenderer
{
  public:
    IndoorRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~IndoorRenderer();

    void render(const engine::MapScene& scene, const Camera& camera);

  private:
    SDLRenderer& renderer;
    util::ILogger& logger;
};

} // namespace runeharbor::graphics
