// SPDX-License-Identifier: MIT
#include "outdoor_renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>

#include <cmath>

#include "../game/game_world.hpp"
#include "billboard.hpp"
#include "clip_utils.hpp"
#include "sprite_facing.hpp"
#include "visibility.hpp"
#include "world_coordinates.hpp"

namespace runeharbor::graphics
{
namespace
{
constexpr float kMinSpawnBillboardHalfWidth = 24.0f;
constexpr float kMinSpawnBillboardHeight = 56.0f;

float waterWavePhase(float worldX, float worldZ, float timeSeconds)
{
    return timeSeconds * 2.4f + worldX * 0.0025f + worldZ * 0.0020f;
}

float gammaToScale(int gamma)
{
    // MM7-style gamma keys are additive brightness adjustments; map to a bounded scale.
    return std::clamp(1.0f + static_cast<float>(gamma) / 32.0f, 0.25f, 2.5f);
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

std::array<uint8_t, 3> blendRgb(const std::array<uint8_t, 3>& day,
                                const std::array<uint8_t, 3>& night, float nightBlend)
{
    const float t = clamp01(nightBlend);
    std::array<uint8_t, 3> out = {};
    for (int i = 0; i < 3; i++)
    {
        const float value = (1.0f - t) * static_cast<float>(day[static_cast<size_t>(i)]) +
                            t * static_cast<float>(night[static_cast<size_t>(i)]);
        out[static_cast<size_t>(i)] =
            static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 255L));
    }
    return out;
}

std::string trimmed(std::string_view value)
{
    size_t start = 0;
    size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start])))
    {
        start++;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        end--;
    }
    return std::string(value.substr(start, end - start));
}

} // namespace

namespace detail
{

// Pull the sprite's real texture name, animation attributes and world scale out of
// the sprite frame table, and let the table override the per-octant mirroring.
void applyFrameTableEntry(SpawnBillboard& sprite, const formats::SpriteFrameTable* spriteFrameTable,
                          int octant)
{
    if (!spriteFrameTable)
    {
        return;
    }

    const auto* entry = spriteFrameTable->findEntryByIcon(sprite.textureName);
    if (!entry)
    {
        return;
    }

    if (!entry->textureName.empty())
    {
        sprite.textureName = trimmed(entry->textureName);
    }
    sprite.attributes = entry->attributes;
    sprite.scale = entry->scale;
    // Animation: derive the frame count from the sequence length / duration.
    if (entry->animLength > 0 && entry->animDuration > 0)
    {
        sprite.animFrameCount =
            std::clamp(entry->animLength / std::max(1, (int)entry->animDuration), 1, 8);
    }
    if (formats::spriteFrameMirrorsOctant(entry->attributes, octant))
    {
        sprite.flipU = !sprite.flipU;
    }
}

OutdoorLightingParams makeOutdoorLightingParams(const game::RuntimeConfig* runtimeConfig,
                                                float nightBlend, bool terrainPass)
{
    OutdoorLightingParams params;
    if (!runtimeConfig)
    {
        return params;
    }

    params.shadeStart = static_cast<float>(std::max(0, runtimeConfig->distShade));
    params.shadeMistStart =
        static_cast<float>(std::max(runtimeConfig->distShade, runtimeConfig->distShadeMist));
    params.mistFull = static_cast<float>(std::max(
        std::max(runtimeConfig->distShade, runtimeConfig->distShadeMist), runtimeConfig->distMist));
    params.noMist = runtimeConfig->noMist;
    params.gammaScale =
        gammaToScale(terrainPass ? runtimeConfig->terrainGamma : runtimeConfig->buildingGamma);
    params.ambientScale = std::clamp(1.0f - nightBlend * 0.28f, 0.45f, 1.0f);

    const auto mist =
        blendRgb(runtimeConfig->skyDayBottom, runtimeConfig->skyNightBottom, nightBlend);
    params.mistColor = {static_cast<float>(mist[0]) / 255.0f, static_cast<float>(mist[1]) / 255.0f,
                        static_cast<float>(mist[2]) / 255.0f, 1.0f};
    return params;
}

SDL_FColor applyOutdoorLighting(SDL_FColor color, float distance,
                                const OutdoorLightingParams& params)
{
    const float shadeStart = params.shadeStart;
    const float shadeMistStart = std::max(params.shadeMistStart, shadeStart + 1.0f);
    const float mistFull = std::max(params.mistFull, shadeMistStart + 1.0f);

    float shadeFactor = 1.0f;
    float fogBlend = 0.0f;

    if (distance > shadeStart)
    {
        if (distance <= shadeMistStart)
        {
            const float t = (distance - shadeStart) / (shadeMistStart - shadeStart);
            shadeFactor = 1.0f - 0.35f * std::clamp(t, 0.0f, 1.0f);
        }
        else if (distance <= mistFull)
        {
            const float t = (distance - shadeMistStart) / (mistFull - shadeMistStart);
            shadeFactor = 0.65f - 0.30f * std::clamp(t, 0.0f, 1.0f);
            fogBlend = std::clamp(t, 0.0f, 1.0f);
        }
        else
        {
            shadeFactor = 0.35f;
            fogBlend = 1.0f;
        }
    }

    if (params.noMist)
    {
        fogBlend = 0.0f;
    }

    color.r =
        std::clamp(color.r * shadeFactor * params.gammaScale * params.ambientScale, 0.0f, 1.0f);
    color.g =
        std::clamp(color.g * shadeFactor * params.gammaScale * params.ambientScale, 0.0f, 1.0f);
    color.b =
        std::clamp(color.b * shadeFactor * params.gammaScale * params.ambientScale, 0.0f, 1.0f);

    if (fogBlend > 0.0f)
    {
        color.r = color.r * (1.0f - fogBlend) + params.mistColor.r * fogBlend;
        color.g = color.g * (1.0f - fogBlend) + params.mistColor.g * fogBlend;
        color.b = color.b * (1.0f - fogBlend) + params.mistColor.b * fogBlend;
    }

    return color;
}

float calcHorizonY(float viewportHeight, float fovY, float cameraPitch)
{
    const float viewPlaneDist = (viewportHeight / 2.0f) / std::tan(fovY / 2.0f);
    float horizonY = (viewportHeight / 2.0f) + viewPlaneDist * std::tan(cameraPitch);
    return std::clamp(horizonY, 0.0f, viewportHeight * 2.0f);
}

SpawnBillboard makeOutdoorSpawnBillboard(const formats::ODMSpawnPoint& spawn, const Vec3& cameraPos,
                                         [[maybe_unused]] const game::RuntimeConfig* config,
                                         const OutdoorRenderer::MonsterSpriteLookup& monsterLookup,
                                         const formats::SpriteFrameTable* spriteFrameTable,
                                         uint32_t ticks)
{
    SpawnBillboard sprite;
    sprite.basePos = gameplayToRenderPosition(
        static_cast<float>(spawn.x), static_cast<float>(spawn.y), static_cast<float>(spawn.z));

    const float baseHeight =
        std::max(kMinSpawnBillboardHeight, static_cast<float>(spawn.radius) * 2.0f);
    sprite.height = std::clamp(baseHeight, 56.0f, 260.0f);
    sprite.halfWidth = std::max(kMinSpawnBillboardHalfWidth, sprite.height * 0.38f);

    if (monsterLookup)
    {
        std::string baseName = monsterLookup(spawn.objectType);
        if (!baseName.empty())
        {
            // A spawn point has no AI yet, so its heading is fixed per spawn rather
            // than spun by the frame clock — what changes as the party walks around
            // is which octant of the sprite faces the camera.
            const int spriteHeading = (static_cast<int>(spawn.objectIndex) * 137) % kMM7FullTurn;
            const int octant = cameraRelativeOctant(spriteHeading, cameraPos, sprite.basePos);
            const SpriteFacing facing = resolveSpriteFacing(octant);
            sprite.flipU = facing.flipU;

            sprite.textureName =
                std::format("{}{:02d}", baseName.substr(0, std::min<size_t>(baseName.length(), 6)),
                            facing.frameDirection);
            applyFrameTableEntry(sprite, spriteFrameTable, octant);
        }
    }
    (void)ticks;

    const int group = std::max(0, static_cast<int>(spawn.group));
    const int seed = (static_cast<int>(spawn.objectType) * 163) ^
                     (static_cast<int>(spawn.objectIndex) * 97) ^ (group * 19);
    const float r = 0.42f + static_cast<float>((seed >> 0) & 0x7) * 0.06f;
    const float g = 0.38f + static_cast<float>((seed >> 3) & 0x7) * 0.06f;
    const float b = 0.35f + static_cast<float>((seed >> 6) & 0x7) * 0.06f;
    sprite.color = {std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f),
                    0.74f};

    if (spawn.objectType == 0x16u || spawn.objectType == 0x17u)
    {
        sprite.color = {0.83f, 0.72f, 0.46f, 0.82f};
    }
    else if (spawn.objectType >= 500u)
    {
        sprite.color = {0.60f, 0.82f, 1.0f, 0.76f};
    }

    const Vec3 delta = sprite.basePos - cameraPos;
    sprite.distanceSq = delta.lengthSquared();
    return sprite;
}

SpawnBillboard makeLiveActorBillboard(const LiveActor& actor, const Vec3& cameraPos,
                                      const OutdoorRenderer::MonsterSpriteLookup& monsterLookup,
                                      const formats::SpriteFrameTable* spriteFrameTable)
{
    SpawnBillboard sprite;
    sprite.basePos =
        gameplayToRenderPosition(actor.x, actor.y, actor.z + (actor.flying ? 256.0f : 0.0f));

    // Real monster dimensions aren't carried on MonsterInstance; use a sensible
    // default that the draw pass overrides from the resolved texture pixels.
    sprite.height = 128.0f;
    sprite.halfWidth = 48.0f;

    if (monsterLookup)
    {
        std::string baseName = monsterLookup(actor.monsterId);
        if (!baseName.empty())
        {
            // The actor's real heading drives the facing octant (RE: live actor
            // table iteration uses the actor's +0x72 facing, not a fixed value).
            const int octant = cameraRelativeOctant(static_cast<int>(actor.facingAngle), cameraPos,
                                                    sprite.basePos);
            const SpriteFacing facing = resolveSpriteFacing(octant);
            sprite.flipU = facing.flipU;
            sprite.textureName =
                std::format("{}{:02d}", baseName.substr(0, std::min<size_t>(baseName.length(), 6)),
                            facing.frameDirection);
            applyFrameTableEntry(sprite, spriteFrameTable, octant);
        }
    }

    // White tint so the real palette shows through (MM7 uses per-tint lit
    // palettes, not the random per-spawn tinting the static path used).
    if (actor.dead)
    {
        // Corpse: laid out, dimmed.
        sprite.color = {0.55f, 0.45f, 0.40f, 0.65f};
        sprite.height *= 0.5f;
    }
    else
    {
        sprite.color = {1.0f, 1.0f, 1.0f, 1.0f};
    }

    const Vec3 delta = sprite.basePos - cameraPos;
    sprite.distanceSq = delta.lengthSquared();
    return sprite;
}

SpawnBillboard makeWorldItemBillboard(const WorldItem& item, const Vec3& cameraPos,
                                      const WorldItemSpriteLookup& spriteLookup)
{
    SpawnBillboard sprite;
    sprite.basePos = gameplayToRenderPosition(item.x, item.y, item.z);
    // Items render as small ground billboards (loot piles).
    sprite.height = 32.0f;
    sprite.halfWidth = 16.0f;
    sprite.color = {1.0f, 0.95f, 0.6f, 0.9f}; // subtle gold tint so items catch the eye
    if (spriteLookup)
    {
        sprite.textureName = spriteLookup(item.itemId);
    }
    const Vec3 delta = sprite.basePos - cameraPos;
    sprite.distanceSq = delta.lengthSquared();
    return sprite;
}

SpawnBillboard makeOutdoorDecorationBillboard(const formats::ParsedDecoration& decoration,
                                              const Vec3& cameraPos,
                                              const formats::SpriteFrameTable* spriteFrameTable)
{
    SpawnBillboard sprite;
    sprite.basePos =
        gameplayToRenderPosition(static_cast<float>(decoration.x), static_cast<float>(decoration.y),
                                 static_cast<float>(decoration.z));
    sprite.textureName = decoration.name;
    applyFrameTableEntry(sprite, spriteFrameTable, 0);

    // Tint emissive/liquid decorations to match the indoor treatment.
    std::string lower = sprite.textureName;
    for (char& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    sprite.color = {1.0f, 1.0f, 1.0f, 1.0f};
    if (lower.find("torch") != std::string::npos || lower.find("fire") != std::string::npos)
        sprite.color = {1.0f, 0.82f, 0.50f, 0.92f};
    else if (lower.find("water") != std::string::npos || lower.find("mist") != std::string::npos)
        sprite.color = {0.66f, 0.82f, 1.0f, 0.80f};

    // Fallback size when the texture cannot be measured; the draw pass overrides
    // this from the actual sprite dimensions.
    sprite.halfWidth = 32.0f;
    sprite.height = 96.0f;

    const Vec3 delta = sprite.basePos - cameraPos;
    sprite.distanceSq = delta.lengthSquared();
    return sprite;
}

} // namespace detail

OutdoorRenderer::OutdoorRenderer(SDLRenderer& renderer, util::ILogger& logger)
    : renderer(renderer), logger(logger)
{
    (void)this->logger;
}

OutdoorRenderer::~OutdoorRenderer() {}

void OutdoorRenderer::setTextureLookup(TextureLookup lookup)
{
    textureLookup = std::move(lookup);
}

void OutdoorRenderer::setMonsterSpriteLookup(MonsterSpriteLookup lookup)
{
    monsterSpriteLookup = std::move(lookup);
}

void OutdoorRenderer::setSpriteFrameTable(const formats::SpriteFrameTable* table)
{
    spriteFrameTable = table;
}

void OutdoorRenderer::render(const engine::MapScene& scene, const Camera& camera,
                             const game::RuntimeConfig* runtimeConfig, float nightBlend,
                             const Frustum* frustumOverride)
{
    lastMapName_ = scene.getName();
    const auto& odmData = scene.getODMData();
    if (odmData.heightmap.empty())
    {
        return;
    }

    Frustum localFrustum;
    const Frustum* finalFrustum = frustumOverride;
    if (!finalFrustum)
    {
        localFrustum.extractFromMatrix(camera.getViewProjectionMatrix());
        finalFrustum = &localFrustum;
    }

    const float blend = clamp01(nightBlend);

    // Draw the sky via SDL_Renderer (software/2D path)
    renderSky(runtimeConfig, blend);

    renderTerrain(odmData, camera, runtimeConfig, blend, finalFrustum);
    // Buildings and sprites share one back-to-front pass so that billboards are
    // occluded by the structures in front of them.
    renderObjects(odmData, camera, runtimeConfig, blend, finalFrustum);
}

void OutdoorRenderer::renderSky(const game::RuntimeConfig* runtimeConfig, float nightBlend)
{
    if (runtimeConfig && runtimeConfig->noSky)
    {
        return;
    }

    const auto top = (runtimeConfig != nullptr) ? blendRgb(runtimeConfig->skyDayTop,
                                                           runtimeConfig->skyNightTop, nightBlend)
                                                : std::array<uint8_t, 3>{81, 121, 236};
    const auto bottom =
        (runtimeConfig != nullptr)
            ? blendRgb(runtimeConfig->skyDayBottom, runtimeConfig->skyNightBottom, nightBlend)
            : std::array<uint8_t, 3>{153, 193, 237};

    // Single gradient quad (2 triangles, 4 verts) instead of one SDL_RenderLine
    // per scanline (was hundreds of draw calls/frame). Vertex colors interpolate
    // the top→bottom gradient; no texture needed.
    const float w = static_cast<float>(renderer.getViewportWidth());
    const float h = static_cast<float>(renderer.getViewportHeight());
    const SDL_FColor topCol = {static_cast<float>(top[0]) / 255.0f,
                               static_cast<float>(top[1]) / 255.0f,
                               static_cast<float>(top[2]) / 255.0f, 1.0f};
    const SDL_FColor botCol = {static_cast<float>(bottom[0]) / 255.0f,
                               static_cast<float>(bottom[1]) / 255.0f,
                               static_cast<float>(bottom[2]) / 255.0f, 1.0f};
    const SDL_Vertex skyVerts[4] = {
        {{0.0f, 0.0f}, topCol, {0.0f, 0.0f}},
        {{w, 0.0f}, topCol, {1.0f, 0.0f}},
        {{w, h}, botCol, {1.0f, 1.0f}},
        {{0.0f, h}, botCol, {0.0f, 1.0f}},
    };
    const int skyIndices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer.getSDLRenderer(), nullptr, skyVerts, 4, skyIndices, 6);
}

void OutdoorRenderer::renderTerrain(const formats::ODMMapData& odmData, const Camera& camera,
                                    const game::RuntimeConfig* runtimeConfig, float nightBlend,
                                    const Frustum* frustumOverride)
{
    const auto& viewProjection = camera.getViewProjectionMatrix();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());
    const Vec3 cameraPos = camera.getPosition();
    const detail::OutdoorLightingParams terrainLighting =
        detail::makeOutdoorLightingParams(runtimeConfig, nightBlend, true);

    constexpr int SIZE = formats::ODMMapData::TERRAIN_SIZE;
    constexpr float CELL_SIZE = 512.0f;
    Frustum localFrustum;
    const Frustum* frustum = frustumOverride;
    if (!frustum)
    {
        localFrustum.extractFromMatrix(viewProjection);
        frustum = &localFrustum;
    }

    // Build the camera-independent terrain vertex cache once per map (avoids
    // per-frame heightmap reads + grid→world conversions for all 128×128
    // corners). The cache stores render-space positions; the camera-dependent
    // clip transform and LOD still run per-frame.
    if (terrainCacheMapName_ != lastMapName_)
    {
        terrainCacheMapName_ = lastMapName_;
        terrainWorldVerts_.clear();
        terrainWorldVerts_.resize(static_cast<size_t>(SIZE) * SIZE);
        for (int gy = 0; gy < SIZE; ++gy)
        {
            for (int gx = 0; gx < SIZE; ++gx)
            {
                float height = 0.0f;
                size_t idx = static_cast<size_t>(gy * SIZE + gx);
                if (idx < odmData.heightmap.size())
                {
                    height = static_cast<float>(odmData.heightmap[idx].height);
                }
                terrainWorldVerts_[idx] = gameplayToRenderPosition(
                    formats::outdoorGridToWorldX(static_cast<float>(gx)),
                    formats::outdoorGridToWorldY(static_cast<float>(gy)), height);
            }
        }
    }

    // Helper: look up a cached world-space position for a grid corner.
    auto worldPos = [&](int gx, int gy) -> Vec3
    {
        const size_t idx = static_cast<size_t>(gy * SIZE + gx);
        if (idx < terrainWorldVerts_.size())
        {
            return terrainWorldVerts_[idx];
        }
        return Vec3{};
    };

    // Transform world point to clip space (no perspective divide yet)
    auto toClip = [&](Vec3 wp) -> Vec4 { return viewProjection * Vec4(wp, 1.0f); };

    // Height-based vertex coloring, used only where no tile texture resolved. A
    // textured quad keeps a white base so the tile art shows through unmodulated;
    // distance shading and mist are applied on top of whichever base is used.
    auto heightColor = [](float height) -> SDL_FColor
    {
        float t = std::clamp(height / 8000.0f, 0.0f, 1.0f);
        float r = 0.30f + t * 0.35f;
        float g = 0.45f - t * 0.15f;
        float b = 0.20f + t * 0.05f;
        return {r, g, b, 1.0f};
    };
    auto litTerrainColor = [&](const Vec3& worldPos, bool textured) -> SDL_FColor
    {
        const float dx = worldPos.x - cameraPos.x;
        const float dy = worldPos.y - cameraPos.y;
        const float dz = worldPos.z - cameraPos.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const SDL_FColor base =
            textured ? SDL_FColor{1.0f, 1.0f, 1.0f, 1.0f} : heightColor(worldPos.y);
        return detail::applyOutdoorLighting(base, dist, terrainLighting);
    };

    // Terrain quads are collected with their depth so the whole field can be drawn
    // back-to-front. SDL_Renderer has no depth buffer, so grouping purely by texture
    // (as an unordered batch) lets far hills paint over near ones.
    // Reused member buffers (cleared, not reallocated, each frame).
    auto& quads = terrainQuads_;
    auto& quadVerts = terrainQuadVerts_;
    quads.clear();
    quadVerts.clear();
    // Tessellation scratch — hoisted out of the per-quad loop (was allocated
    // inside it, up to ~16k allocs/frame).
    std::vector<ClipVertex> tessellatedVerts;

    // Resolve tile texture for a grid cell
    auto getTileTexture = [&](int gx, int gy) -> SDL_Texture*
    {
        if (!textureLookup)
            return nullptr;
        size_t idx = static_cast<size_t>(gy * SIZE + gx);
        if (idx >= odmData.heightmap.size())
            return nullptr;
        uint8_t tileIdx = odmData.heightmap[idx].tileIndex;
        if (tileIdx < odmData.tileTextures.size() && !odmData.tileTextures[tileIdx].empty())
        {
            return textureLookup(odmData.tileTextures[tileIdx]);
        }
        return nullptr;
    };

    ClipVertex clipped[MAX_CLIP_VERTS];

    for (int gy = 0; gy < SIZE - 1; gy++)
    {
        int gx = 0;
        while (gx < SIZE - 1)
        {
            Vec3 p00 = worldPos(gx, gy);

            // Center of quad for distance check. Grid Y runs south, so the next
            // row sits at a *lower* render Z.
            float cx = p00.x + CELL_SIZE * 0.5f;
            float cy = p00.y;
            float cz = p00.z - CELL_SIZE * 0.5f;
            float dx = cx - cameraPos.x;
            float dy = cy - cameraPos.y;
            float dz = cz - cameraPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            int step = TerrainLOD::lodStep(distSq);
            if (step <= 0)
            {
                gx++;
                continue;
            }
            if (gx + step >= SIZE)
            {
                step = (SIZE - 1) - gx;
            }
            if (gy + step >= SIZE)
            {
                step = (SIZE - 1) - gy;
            }
            if (step <= 0)
            {
                gx++;
                continue;
            }

            Vec3 p10 = worldPos(gx + step, gy);
            Vec3 p01 = worldPos(gx, gy + step);
            Vec3 p11 = worldPos(gx + step, gy + step);

            float minX = std::min(std::min(p00.x, p10.x), std::min(p01.x, p11.x));
            float minY = std::min(std::min(p00.y, p10.y), std::min(p01.y, p11.y));
            float minZ = std::min(std::min(p00.z, p10.z), std::min(p01.z, p11.z));
            float maxX = std::max(std::max(p00.x, p10.x), std::max(p01.x, p11.x));
            float maxY = std::max(std::max(p00.y, p10.y), std::max(p01.y, p11.y));
            float maxZ = std::max(std::max(p00.z, p10.z), std::max(p01.z, p11.z));
            if (!frustum->testAABB(minX, minY, minZ, maxX, maxY, maxZ))
            {
                gx += step;
                continue;
            }

            // Resolve tile texture for this quad
            SDL_Texture* tileTex = getTileTexture(gx, gy);
            const bool textured = (tileTex != nullptr);
            const uint32_t quadFirstVertex = static_cast<uint32_t>(quadVerts.size());

            // Clip-space vertices for the 4 corners
            Vec4 c00 = toClip(p00);
            Vec4 c10 = toClip(p10);
            Vec4 c01 = toClip(p01);
            Vec4 c11 = toClip(p11);

            SDL_FColor col00 = litTerrainColor(p00, textured);
            SDL_FColor col10 = litTerrainColor(p10, textured);
            SDL_FColor col01 = litTerrainColor(p01, textured);
            SDL_FColor col11 = litTerrainColor(p11, textured);

            // Process two triangles per quad
            ClipVertex tris[2][3] = {
                {
                    {c00, col00, 0.0f, 0.0f},
                    {c10, col10, 1.0f, 0.0f},
                    {c01, col01, 0.0f, 1.0f},
                },
                {
                    {c10, col10, 1.0f, 0.0f},
                    {c11, col11, 1.0f, 1.0f},
                    {c01, col01, 0.0f, 1.0f},
                },
            };

            for (int t = 0; t < 2; t++)
            {
                // Skip if all vertices behind near plane
                if (tris[t][0].clip.w < CLIP_NEAR_EPSILON &&
                    tris[t][1].clip.w < CLIP_NEAR_EPSILON && tris[t][2].clip.w < CLIP_NEAR_EPSILON)
                {
                    continue;
                }

                int count = clipPolygonNearPlane(tris[t], 3, clipped);
                if (count < 3)
                {
                    continue;
                }

                // Tessellate clip-space polygons to minimize affine texture warping.
                // (tessellatedVerts is hoisted outside the loop — clear+reuse.)
                tessellatedVerts.clear();
                for (int i = 1; i + 1 < count; i++)
                {
                    tessellateTriangle(clipped[0], clipped[i], clipped[i + 1], tessellatedVerts);
                }

                const size_t triangleStart = quadVerts.size();
                bool anyFailed = false;

                for (const auto& tv : tessellatedVerts)
                {
                    float sx, sy;
                    if (!projectClipToScreen(tv.clip, vpW, vpH, sx, sy))
                    {
                        anyFailed = true;
                        break;
                    }
                    SDL_Vertex sv;
                    sv.position = {sx, sy};
                    sv.color = tv.color;
                    sv.tex_coord = {tv.u, tv.v};
                    quadVerts.push_back(sv);
                }

                if (anyFailed)
                {
                    quadVerts.resize(triangleStart);
                }
            }

            const uint32_t quadVertexCount =
                static_cast<uint32_t>(quadVerts.size()) - quadFirstVertex;
            if (quadVertexCount >= 3)
            {
                quads.push_back({tileTex, distSq, quadFirstVertex, quadVertexCount});
            }
            else
            {
                quadVerts.resize(quadFirstVertex);
            }

            gx += step;
        }
    }

    // Painter's algorithm: farthest quad first. Consecutive quads that share a
    // texture are merged into a single draw so the common case (large runs of the
    // same tile) still issues few calls.
    std::sort(quads.begin(), quads.end(), [](const TerrainQuad& a, const TerrainQuad& b)
              { return a.distanceSq > b.distanceSq; });

    auto& runVerts = terrainRunVerts_;
    runVerts.clear();
    auto flushRun = [&](SDL_Texture* texture)
    {
        if (runVerts.size() < 3)
        {
            runVerts.clear();
            return;
        }
        SDL_RenderGeometry(renderer.getSDLRenderer(), texture, runVerts.data(),
                           static_cast<int>(runVerts.size()), nullptr, 0);
        runVerts.clear();
    };

    SDL_Texture* runTexture = nullptr;
    bool runOpen = false;
    for (const auto& quad : quads)
    {
        if (runOpen && quad.texture != runTexture)
        {
            flushRun(runTexture);
            runOpen = false;
        }
        runTexture = quad.texture;
        runOpen = true;
        runVerts.insert(runVerts.end(), quadVerts.begin() + quad.firstVertex,
                        quadVerts.begin() + quad.firstVertex + quad.vertexCount);
    }
    if (runOpen)
    {
        flushRun(runTexture);
    }
}

void OutdoorRenderer::renderObjects(const formats::ODMMapData& odmData, const Camera& camera,
                                    const game::RuntimeConfig* runtimeConfig, float nightBlend,
                                    const Frustum* frustumOverride)
{
    const auto& viewProjection = camera.getViewProjectionMatrix();
    const Vec3 cameraPos = camera.getPosition();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());
    const detail::OutdoorLightingParams objectLighting =
        detail::makeOutdoorLightingParams(runtimeConfig, nightBlend, false);
    const bool animateWater = (runtimeConfig == nullptr) || !runtimeConfig->noWavyWater;
    const uint32_t ticks = SDL_GetTicks();
    const float waterTimeSeconds = static_cast<float>(ticks) * 0.001f;

    Frustum localFrustum;
    const Frustum* frustum = frustumOverride;
    if (!frustum)
    {
        localFrustum.extractFromMatrix(viewProjection);
        frustum = &localFrustum;
    }

    // One draw list for building faces and billboards. SDL_Renderer has no depth
    // buffer, so sprites can only be occluded by walls if both go through the same
    // back-to-front ordering.
    struct RenderOp
    {
        const formats::ParsedBuilding* building = nullptr; // null for billboards
        uint32_t faceIndex = 0;
        float distanceSq = 0.0f;
        detail::SpawnBillboard billboard;
    };

    std::vector<RenderOp> ops;

    for (const auto& building : odmData.buildings)
    {
        // BSP model bounding boxes are in absolute world coordinates (not relative to posX/Y/Z).
        // Convert from gameplay coords (Z=up) to render coords (Y=up).
        const float minX = static_cast<float>(building.minX);
        const float maxX = static_cast<float>(building.maxX);
        const float minY = static_cast<float>(building.minZ); // gameplay Z → render Y
        const float maxY = static_cast<float>(building.maxZ);
        const float minZ = static_cast<float>(building.minY); // gameplay Y → render Z
        const float maxZ = static_cast<float>(building.maxY);

        if (!frustum->testAABB(minX, minY, minZ, maxX, maxY, maxZ))
        {
            continue;
        }

        for (uint32_t fi = 0; fi < building.faces.size(); fi++)
        {
            const auto& face = building.faces[fi];
            if (face.numVertices < 3 || face.vertexIndices.empty() || face.isInvisible())
            {
                continue;
            }

            // Face centroid, used for backface culling and depth sorting.
            float cx = 0, cy = 0, cz = 0;
            uint32_t valid = 0;
            for (uint8_t j = 0; j < face.numVertices && j < face.vertexIndices.size(); j++)
            {
                uint16_t vi = face.vertexIndices[j];
                if (vi < building.vertices.size())
                {
                    const auto& v = building.vertices[vi];
                    cx += static_cast<float>(v.x);
                    cy += static_cast<float>(v.z);
                    cz += static_cast<float>(v.y);
                    valid++;
                }
            }
            if (valid == 0)
            {
                continue;
            }
            const float inv = 1.0f / static_cast<float>(valid);
            cx *= inv;
            cy *= inv;
            cz *= inv;

            const float viewX = cx - cameraPos.x;
            const float viewY = cy - cameraPos.y;
            const float viewZ = cz - cameraPos.z;
            const Vec3 faceNormal =
                gameplayToRenderDirection(face.normalFX, face.normalFY, face.normalFZ);
            if (faceNormal.x * viewX + faceNormal.y * viewY + faceNormal.z * viewZ > 0.0f)
            {
                continue;
            }

            RenderOp op;
            op.building = &building;
            op.faceIndex = fi;
            op.distanceSq = viewX * viewX + viewY * viewY + viewZ * viewZ;
            ops.push_back(std::move(op));
        }
    }

    // Monster spawns.
    if (!(runtimeConfig && runtimeConfig->noMonsters))
    {
        for (const auto& spawn : odmData.spawns)
        {
            if (spawn.objectType == 0)
            {
                continue;
            }

            const float cullRadius = std::max(64.0f, static_cast<float>(spawn.radius) * 2.0f);
            const Vec3 renderPos =
                gameplayToRenderPosition(static_cast<float>(spawn.x), static_cast<float>(spawn.y),
                                         static_cast<float>(spawn.z));
            if (!frustum->testSphere(renderPos.x, renderPos.y, renderPos.z, cullRadius))
            {
                continue;
            }

            RenderOp op;
            op.billboard = detail::makeOutdoorSpawnBillboard(
                spawn, cameraPos, runtimeConfig, monsterSpriteLookup, spriteFrameTable, ticks);
            op.distanceSq = op.billboard.distanceSq;
            ops.push_back(std::move(op));
        }
    }

    // Live roaming/combat monsters (RE: the live actor table). These carry real
    // heading/position/state from CombatSystem, unlike the static spawn markers.
    if (liveActorProvider_)
    {
        const auto actors = liveActorProvider_();
        for (const auto& actor : actors)
        {
            RenderOp op;
            op.billboard = detail::makeLiveActorBillboard(actor, cameraPos, monsterSpriteLookup,
                                                          spriteFrameTable);
            op.distanceSq = op.billboard.distanceSq;
            ops.push_back(std::move(op));
        }
    }

    // World-dropped items (loot piles).
    if (worldItemProvider_)
    {
        const auto items = worldItemProvider_();
        for (const auto& item : items)
        {
            RenderOp op;
            op.billboard = detail::makeWorldItemBillboard(item, cameraPos, worldItemSpriteLookup_);
            op.distanceSq = op.billboard.distanceSq;
            ops.push_back(std::move(op));
        }
    }

    // Static decorations (trees, rocks, signs, campfires). Sound and party-start
    // markers carry no sprite and drop out when their texture does not resolve.
    if (!(runtimeConfig && runtimeConfig->noDecorations))
    {
        for (const auto& deco : odmData.decorations)
        {
            if (deco.hidden)
            {
                continue;
            }
            const Vec3 renderPos = gameplayToRenderPosition(
                static_cast<float>(deco.x), static_cast<float>(deco.y), static_cast<float>(deco.z));
            if (!frustum->testSphere(renderPos.x, renderPos.y, renderPos.z, 512.0f))
            {
                continue;
            }

            RenderOp op;
            op.billboard =
                detail::makeOutdoorDecorationBillboard(deco, cameraPos, spriteFrameTable);
            if (op.billboard.textureName.empty() ||
                (textureLookup && !textureLookup(op.billboard.textureName)))
            {
                continue;
            }
            op.distanceSq = op.billboard.distanceSq;
            ops.push_back(std::move(op));
        }
    }

    if (ops.empty())
    {
        return;
    }

    // Two-pass render: opaque building faces first (back-to-front), then
    // transparent billboards (back-to-front). This reduces sprite/geometry
    // z-fighting compared to a single mixed sort (MM7 tests each sprite against
    // overlapping facets; this is a pragmatic approximation of that).
    auto opaqueEnd = std::partition(ops.begin(), ops.end(),
                                    [](const RenderOp& op) { return op.building != nullptr; });
    std::sort(ops.begin(), opaqueEnd,
              [](const RenderOp& a, const RenderOp& b) { return a.distanceSq > b.distanceSq; });
    std::sort(opaqueEnd, ops.end(),
              [](const RenderOp& a, const RenderOp& b) { return a.distanceSq > b.distanceSq; });

    SDL_SetRenderDrawBlendMode(renderer.getSDLRenderer(), SDL_BLENDMODE_BLEND);

    ClipVertex polyIn[MAX_CLIP_VERTS];
    ClipVertex polyOut[MAX_CLIP_VERTS];
    std::vector<ClipVertex> tessellatedVerts;
    std::vector<SDL_Vertex> verts;

    // Pass 1: opaque building faces.
    for (auto it = ops.begin(); it != opaqueEnd; ++it)
    {
        const auto& op = *it;

        const auto& building = *op.building;
        const auto& face = building.faces[op.faceIndex];

        // Resolve the texture first: it decides both the vertex base colour and the
        // UV scale, since MM7 stores face UVs in texels rather than normalised.
        SDL_Texture* texture = nullptr;
        float texWidth = 256.0f;
        float texHeight = 256.0f;
        if (textureLookup && !face.textureName.empty())
        {
            texture = textureLookup(face.textureName);
            if (texture)
            {
                float w = 0.0f;
                float h = 0.0f;
                if (SDL_GetTextureSize(texture, &w, &h) && w > 0.0f && h > 0.0f)
                {
                    texWidth = w;
                    texHeight = h;
                }
            }
        }

        int polyCount = 0;
        bool allBehind = true;

        for (uint8_t i = 0; i < face.numVertices; i++)
        {
            if (i >= face.vertexIndices.size())
            {
                break;
            }
            uint16_t vi = face.vertexIndices[i];
            if (vi >= building.vertices.size())
            {
                break;
            }
            if (polyCount >= MAX_CLIP_VERTS)
            {
                break;
            }

            const auto& v = building.vertices[vi];
            // BSP model vertices are in absolute world coordinates (not relative to worldX/Y/Z).
            Vec3 wp = gameplayToRenderPosition(static_cast<float>(v.x), static_cast<float>(v.y),
                                               static_cast<float>(v.z));

            Vec4 clip = viewProjection * Vec4(wp, 1.0f);
            if (clip.w >= CLIP_NEAR_EPSILON)
            {
                allBehind = false;
            }

            // Textured faces keep a white base so the art comes through at full
            // brightness; the surface-type palette is only a stand-in for faces
            // whose texture could not be resolved.
            SDL_FColor color;
            if (texture)
            {
                color = {1.0f, 1.0f, 1.0f, face.isWater() ? 0.85f : 1.0f};
            }
            else if (face.isFloor())
            {
                color = {0.45f, 0.42f, 0.38f, 1.0f};
            }
            else if (face.isCeiling())
            {
                color = {0.30f, 0.28f, 0.25f, 1.0f};
            }
            else if (face.isWater())
            {
                color = {0.15f, 0.30f, 0.55f, 0.8f};
            }
            else
            {
                color = {0.55f, 0.50f, 0.42f, 1.0f};
            }

            const float dx = wp.x - cameraPos.x;
            const float dy = wp.y - cameraPos.y;
            const float dz = wp.z - cameraPos.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            color = detail::applyOutdoorLighting(color, dist, objectLighting);
            if (face.isWater() && animateWater)
            {
                const float wave = std::sin(waterWavePhase(wp.x, wp.z, waterTimeSeconds));
                color.r = std::clamp(color.r + wave * 0.020f, 0.0f, 1.0f);
                color.g = std::clamp(color.g + wave * 0.035f, 0.0f, 1.0f);
                color.b = std::clamp(color.b + wave * 0.065f, 0.0f, 1.0f);
                color.a = std::clamp(color.a + 0.06f + wave * 0.03f, 0.0f, 1.0f);
            }

            // Face UVs are texel offsets into the face's own texture.
            float u = 0.0f, uv = 0.0f;
            if (i < face.uCoords.size() && i < face.vCoords.size())
            {
                u = static_cast<float>(face.uCoords[i]) / texWidth;
                uv = static_cast<float>(face.vCoords[i]) / texHeight;
                if (face.isWater() && animateWater)
                {
                    const float flow = waterTimeSeconds * 0.08f;
                    u += flow;
                    uv += flow * 0.55f;
                }
            }

            polyIn[polyCount++] = {clip, color, u, uv};
        }

        if (allBehind || polyCount < 3)
        {
            continue;
        }

        const int clippedCount = clipPolygonNearPlane(polyIn, polyCount, polyOut);
        if (clippedCount < 3)
        {
            continue;
        }

        // Tessellate clip-space polygons to minimize affine texture warping
        tessellatedVerts.clear();
        for (int i = 1; i + 1 < clippedCount; i++)
        {
            tessellateTriangle(polyOut[0], polyOut[i], polyOut[i + 1], tessellatedVerts);
        }

        verts.clear();
        verts.reserve(tessellatedVerts.size());
        bool anyFailed = false;

        for (const auto& tv : tessellatedVerts)
        {
            float sx, sy;
            if (!projectClipToScreen(tv.clip, vpW, vpH, sx, sy))
            {
                anyFailed = true;
                break;
            }
            SDL_Vertex sv;
            sv.position = {sx, sy};
            sv.color = tv.color;
            sv.tex_coord = {tv.u, tv.v};
            verts.push_back(sv);
        }

        if (anyFailed || verts.size() < 3)
        {
            continue;
        }

        SDL_RenderGeometry(renderer.getSDLRenderer(), texture, verts.data(),
                           static_cast<int>(verts.size()), nullptr, 0);
    }

    // Pass 2: transparent billboards (sprites), back-to-front, drawn on top of
    // all opaque geometry.
    for (auto it = opaqueEnd; it != ops.end(); ++it)
    {
        drawBillboard(it->billboard, camera, objectLighting, ticks);
    }
}

void OutdoorRenderer::drawBillboard(const detail::SpawnBillboard& sprite, const Camera& camera,
                                    const detail::OutdoorLightingParams& lighting, uint32_t ticks)
{
    const auto& viewProjection = camera.getViewProjectionMatrix();
    const Vec3 cameraPos = camera.getPosition();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());

    SDL_Texture* texture = nullptr;
    float halfWidth = sprite.halfWidth;
    float height = sprite.height;

    if (textureLookup && !sprite.textureName.empty())
    {
        // Animate: when the frame table marked this sprite as multi-frame, cycle
        // the texture suffix using (tick>>3) % frameCount (RE: FUN_0044e1c6).
        std::string texName = sprite.textureName;
        if (sprite.animFrameCount > 1)
        {
            texName =
                formats::animatedTextureName(sprite.textureName, ticks, sprite.animFrameCount);
        }
        texture = textureLookup(texName);
        if (!texture && sprite.animFrameCount > 1)
        {
            texture = textureLookup(sprite.textureName); // fall back to base frame
        }
        if (texture)
        {
            float texW = 0.0f;
            float texH = 0.0f;
            if (SDL_GetTextureSize(texture, &texW, &texH) && texW > 0.0f && texH > 0.0f)
            {
                // MM7 sprites are authored at one texel per world unit; the frame
                // table's scale is the only per-sprite adjustment.
                halfWidth = texW * sprite.scale * 0.5f;
                height = texH * sprite.scale;
            }
        }
    }

    const float distance = std::sqrt(sprite.distanceSq);
    SDL_FColor drawColor = (sprite.attributes & formats::kSpriteFrameLit)
                               ? sprite.color
                               : detail::applyOutdoorLighting(sprite.color, distance, lighting);

    Vec3 drawPos = sprite.basePos;
    if (sprite.attributes & formats::kSpriteFrameCenter)
    {
        // The anchor is the sprite's centre rather than its base.
        drawPos.y -= height * 0.5f;
    }
    if (sprite.attributes & formats::kSpriteFrameTransparent)
    {
        drawColor.a *= 0.5f;
    }
    if (sprite.attributes & formats::kSpriteFrameGlowing)
    {
        const float glow = (std::sin(static_cast<float>(ticks) * 0.005f) + 1.0f) * 0.5f;
        drawColor.r = std::clamp(drawColor.r + glow * 0.2f, 0.0f, 1.0f);
        drawColor.g = std::clamp(drawColor.g + glow * 0.2f, 0.0f, 1.0f);
        drawColor.b = std::clamp(drawColor.b + glow * 0.2f, 0.0f, 1.0f);
    }

    const BillboardQuad quad =
        computeBillboardQuad(drawPos, cameraPos, halfWidth, height, sprite.flipU);

    const Vec4 clipBL = viewProjection * Vec4(quad.bottomLeft, 1.0f);
    const Vec4 clipBR = viewProjection * Vec4(quad.bottomRight, 1.0f);
    const Vec4 clipTL = viewProjection * Vec4(quad.topLeft, 1.0f);
    const Vec4 clipTR = viewProjection * Vec4(quad.topRight, 1.0f);
    if (clipBL.w < CLIP_NEAR_EPSILON || clipBR.w < CLIP_NEAR_EPSILON ||
        clipTL.w < CLIP_NEAR_EPSILON || clipTR.w < CLIP_NEAR_EPSILON)
    {
        return;
    }

    float sxBL, syBL, sxBR, syBR, sxTL, syTL, sxTR, syTR;
    if (!projectClipToScreen(clipBL, vpW, vpH, sxBL, syBL) ||
        !projectClipToScreen(clipBR, vpW, vpH, sxBR, syBR) ||
        !projectClipToScreen(clipTL, vpW, vpH, sxTL, syTL) ||
        !projectClipToScreen(clipTR, vpW, vpH, sxTR, syTR))
    {
        return;
    }

    SDL_Vertex vertices[4];
    vertices[0].position = {sxBL, syBL};
    vertices[1].position = {sxBR, syBR};
    vertices[2].position = {sxTL, syTL};
    vertices[3].position = {sxTR, syTR};
    for (auto& vertex : vertices)
    {
        vertex.color = drawColor;
    }
    vertices[0].tex_coord = {quad.uvBottomLeft.u, quad.uvBottomLeft.v};
    vertices[1].tex_coord = {quad.uvBottomRight.u, quad.uvBottomRight.v};
    vertices[2].tex_coord = {quad.uvTopLeft.u, quad.uvTopLeft.v};
    vertices[3].tex_coord = {quad.uvTopRight.u, quad.uvTopRight.v};

    constexpr int indices[6] = {0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(renderer.getSDLRenderer(), texture, vertices, 4, indices, 6);
}

void OutdoorRenderer::invalidateGPUCache() {}

} // namespace runeharbor::graphics
