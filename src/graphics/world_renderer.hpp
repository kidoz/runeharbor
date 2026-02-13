// SPDX-License-Identifier: MIT
#pragma once

#include "camera.hpp"
#include "sdl_renderer.hpp"
#include "../engine/map_scene.hpp"

namespace runeharbor::graphics
{
class IndoorRenderer;

class WorldRenderer
{
  public:
    WorldRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~WorldRenderer();

    void render(const engine::MapScene& scene, const Camera& camera);

  private:
    util::ILogger& logger;
    std::unique_ptr<IndoorRenderer> indoorRenderer;
};

} // namespace runeharbor::graphics
