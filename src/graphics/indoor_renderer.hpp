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

// Callback to resolve texture name -> SDL_Texture*
using TextureLookup = std::function<SDL_Texture*(const std::string& name)>;

class IndoorRenderer
{
  public:
    IndoorRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~IndoorRenderer();

    void setTextureLookup(TextureLookup lookup);
    void render(const engine::MapScene& scene, const Camera& camera);

  private:
    SDLRenderer& renderer;
    [[maybe_unused]] util::ILogger& logger;
    TextureLookup textureLookup;
};

} // namespace runeharbor::graphics
