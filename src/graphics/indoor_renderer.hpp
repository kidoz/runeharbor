// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <unordered_set>

#include "../engine/map_scene.hpp"
#include "camera.hpp"
#include "sdl_renderer.hpp"

struct SDL_Texture;
namespace runeharbor::game
{
struct RuntimeConfig;
}

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
    void render(const engine::MapScene& scene, const Camera& camera,
                const runeharbor::game::RuntimeConfig* runtimeConfig = nullptr,
                const std::unordered_set<uint16_t>* visibleSectors = nullptr);

  private:
    SDLRenderer& renderer;
    [[maybe_unused]] util::ILogger& logger;
    TextureLookup textureLookup;
};

} // namespace runeharbor::graphics
