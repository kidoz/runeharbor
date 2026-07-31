// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL_gpu.h>

#include <functional>
#include <string>

#include "../engine/map_scene.hpp"
#include "../formats/frame_tables.hpp"
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

namespace detail
{
struct OutdoorLightingParams;
struct SpawnBillboard;
} // namespace detail

class OutdoorRenderer
{
  public:
    using TextureLookup = std::function<SDL_Texture*(const std::string& name)>;

    OutdoorRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~OutdoorRenderer();

    using MonsterSpriteLookup = std::function<std::string(uint16_t objectType)>;
    void setTextureLookup(TextureLookup lookup);
    void setMonsterSpriteLookup(MonsterSpriteLookup lookup);
    void setSpriteFrameTable(const formats::SpriteFrameTable* table);
    void render(const engine::MapScene& scene, const Camera& camera,
                const game::RuntimeConfig* runtimeConfig = nullptr, float nightBlend = 0.0f,
                const Frustum* frustumOverride = nullptr);
    void invalidateGPUCache();

  private:
    void renderSky(const game::RuntimeConfig* runtimeConfig, float nightBlend);
    void renderTerrain(const formats::ODMMapData& odmData, const Camera& camera,
                       const game::RuntimeConfig* runtimeConfig, float nightBlend,
                       const Frustum* frustumOverride);
    /// Draw building faces and sprite billboards in a single back-to-front pass.
    void renderObjects(const formats::ODMMapData& odmData, const Camera& camera,
                       const game::RuntimeConfig* runtimeConfig, float nightBlend,
                       const Frustum* frustumOverride);
    void drawBillboard(const detail::SpawnBillboard& sprite, const Camera& camera,
                       const detail::OutdoorLightingParams& lighting, uint32_t ticks);

    SDLRenderer& renderer;
    util::ILogger& logger;
    TextureLookup textureLookup;
    MonsterSpriteLookup monsterSpriteLookup;
    const formats::SpriteFrameTable* spriteFrameTable = nullptr;
};

namespace detail
{
struct OutdoorLightingParams
{
    float shadeStart = 2048.0f;
    float shadeMistStart = 4096.0f;
    float mistFull = 8192.0f;
    bool noMist = false;
    float gammaScale = 1.0f;
    float ambientScale = 1.0f;
    SDL_FColor mistColor = {153.0f / 255.0f, 193.0f / 255.0f, 237.0f / 255.0f, 1.0f};
};

SDL_FColor applyOutdoorLighting(SDL_FColor color, float distance,
                                const OutdoorLightingParams& params);

struct SpawnBillboard
{
    Vec3 basePos;
    float halfWidth = 24.0f;
    float height = 56.0f;
    float distanceSq = 0.0f;
    SDL_FColor color = {1.0f, 1.0f, 1.0f, 0.75f};
    std::string textureName;
    uint32_t attributes = 0;
    float scale = 1.0f;
    bool flipU = false;
};

void applyFrameTableEntry(SpawnBillboard& sprite, const formats::SpriteFrameTable* spriteFrameTable,
                          int octant);

SpawnBillboard makeOutdoorSpawnBillboard(const formats::ODMSpawnPoint& spawn, const Vec3& cameraPos,
                                         const game::RuntimeConfig* config,
                                         const OutdoorRenderer::MonsterSpriteLookup& monsterLookup,
                                         const formats::SpriteFrameTable* spriteFrameTable,
                                         uint32_t ticks = 0);

SpawnBillboard makeOutdoorDecorationBillboard(const formats::ParsedDecoration& decoration,
                                              const Vec3& cameraPos,
                                              const formats::SpriteFrameTable* spriteFrameTable);

float calcHorizonY(float viewportHeight, float fovY, float cameraPitch);
} // namespace detail

} // namespace runeharbor::graphics
