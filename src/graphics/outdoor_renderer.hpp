// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>

#include "../engine/map_scene.hpp"
#include "camera.hpp"
#include "sdl_renderer.hpp"
#include "visibility.hpp"

struct SDL_Texture;

namespace runeharbor::game
{
struct RuntimeConfig;
}

namespace runeharbor::graphics
{

class OutdoorRenderer
{
  public:
    using TextureLookup = std::function<SDL_Texture*(const std::string& name)>;

    OutdoorRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~OutdoorRenderer();

    void setTextureLookup(TextureLookup lookup);
    void render(const engine::MapScene& scene, const Camera& camera,
                const game::RuntimeConfig* runtimeConfig = nullptr, float nightBlend = 0.0f,
                const Frustum* frustumOverride = nullptr);

  private:
    void renderSky(const game::RuntimeConfig* runtimeConfig, float nightBlend);
    void renderTerrain(const formats::ODMMapData& odmData, const Camera& camera,
                       const game::RuntimeConfig* runtimeConfig, float nightBlend,
                       const Frustum* frustumOverride);
    void renderBuildings(const formats::ODMMapData& odmData, const Camera& camera,
                         const game::RuntimeConfig* runtimeConfig, float nightBlend,
                         const Frustum* frustumOverride);
    void renderSpawnBillboards(const formats::ODMMapData& odmData, const Camera& camera,
                               const game::RuntimeConfig* runtimeConfig, float nightBlend,
                               const Frustum* frustumOverride);

    SDLRenderer& renderer;
    util::ILogger& logger;
    TextureLookup textureLookup;
};

} // namespace runeharbor::graphics
