// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL_gpu.h>

#include <functional>
#include <string>
#include <vector>

#include "../engine/map_scene.hpp"
#include "../formats/frame_tables.hpp"
#include "camera.hpp"
#include "live_actors.hpp"
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
    // Live roaming/combat monsters (RE: the actor table at 0x5FF06A). When set,
    // the renderer draws these with their real heading/position/state in
    // addition to (or instead of) the static map spawn markers.
    void setLiveActorProvider(LiveActorProvider provider)
    {
        liveActorProvider_ = std::move(provider);
    }
    // World-dropped items (loot piles). The sprite lookup maps itemId->texture.
    void setWorldItemProvider(WorldItemProvider provider)
    {
        worldItemProvider_ = std::move(provider);
    }
    void setWorldItemSpriteLookup(WorldItemSpriteLookup lookup)
    {
        worldItemSpriteLookup_ = std::move(lookup);
    }
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
    LiveActorProvider liveActorProvider_;
    WorldItemProvider worldItemProvider_;
    WorldItemSpriteLookup worldItemSpriteLookup_;
    const formats::SpriteFrameTable* spriteFrameTable = nullptr;

    // Reused per-frame buffers (avoid heap churn — previously allocated fresh
    // every frame / per-quad, causing up to ~16k allocations/frame for terrain).
    struct TerrainQuad
    {
        SDL_Texture* texture = nullptr;
        float distanceSq = 0.0f;
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
    };
    std::vector<TerrainQuad> terrainQuads_;
    std::vector<SDL_Vertex> terrainQuadVerts_;
    std::vector<SDL_Vertex> terrainRunVerts_;
    // Camera-independent world-space positions for every heightmap grid corner,
    // precomputed once per map to avoid per-frame heightmap reads + grid→world
    // conversions in the terrain hot loop. Indexed [gy * TERRAIN_SIZE + gx].
    std::vector<Vec3> terrainWorldVerts_;
    std::string terrainCacheMapName_; // which map the cache belongs to
    std::string lastMapName_;         // current map being rendered
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
    int animFrameCount = 1; // >1 => cycle frame suffix via (tick>>3) % count
};

void applyFrameTableEntry(SpawnBillboard& sprite, const formats::SpriteFrameTable* spriteFrameTable,
                          int octant);

SpawnBillboard makeOutdoorSpawnBillboard(const formats::ODMSpawnPoint& spawn, const Vec3& cameraPos,
                                         const game::RuntimeConfig* config,
                                         const OutdoorRenderer::MonsterSpriteLookup& monsterLookup,
                                         const formats::SpriteFrameTable* spriteFrameTable,
                                         uint32_t ticks = 0);

// Build a billboard from a live (roaming/combat) monster with its real
// heading + position + dead/flying state. Replaces the static, fixed-heading
// spawn-marker rendering for monsters that have been spawned by CombatSystem.
SpawnBillboard makeLiveActorBillboard(const LiveActor& actor, const Vec3& cameraPos,
                                      const OutdoorRenderer::MonsterSpriteLookup& monsterLookup,
                                      const formats::SpriteFrameTable* spriteFrameTable);

// Build a small ground billboard for a world-dropped item (loot pile).
SpawnBillboard makeWorldItemBillboard(const WorldItem& item, const Vec3& cameraPos,
                                      const WorldItemSpriteLookup& spriteLookup);

SpawnBillboard makeOutdoorDecorationBillboard(const formats::ParsedDecoration& decoration,
                                              const Vec3& cameraPos,
                                              const formats::SpriteFrameTable* spriteFrameTable);

float calcHorizonY(float viewportHeight, float fovY, float cameraPitch);
} // namespace detail

} // namespace runeharbor::graphics
