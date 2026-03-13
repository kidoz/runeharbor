// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <unordered_set>

#include "../engine/map_scene.hpp"
#include "../formats/frame_tables.hpp"
#include "camera.hpp"
#include "sdl_renderer.hpp"
#include "light_stack.hpp"

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

    using MonsterSpriteLookup = std::function<std::string(uint16_t objectType)>;
    void setTextureLookup(TextureLookup lookup);
    void setMonsterSpriteLookup(MonsterSpriteLookup lookup);
    void setSpriteFrameTable(const formats::SpriteFrameTable* table);
    void render(const engine::MapScene& scene, const Camera& camera,
                const runeharbor::game::RuntimeConfig* runtimeConfig = nullptr,
                const std::unordered_set<uint16_t>* visibleSectors = nullptr);

    LightStack& getStationaryLightStack() { return stationaryLights_; }
    LightStack& getMobileLightStack() { return mobileLights_; }

  private:
    SDLRenderer& renderer;
    [[maybe_unused]] util::ILogger& logger;
    TextureLookup textureLookup;
    MonsterSpriteLookup monsterSpriteLookup;
    const formats::SpriteFrameTable* spriteFrameTable = nullptr;

    LightStack stationaryLights_;
    LightStack mobileLights_;
};

} // namespace runeharbor::graphics
