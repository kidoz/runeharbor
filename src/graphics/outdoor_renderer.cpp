// SPDX-License-Identifier: MIT
#include "outdoor_renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>

#include <SDL3_shadercross/SDL_shadercross.h>
#include <cmath>

#include "../game/game_world.hpp"
#include "clip_utils.hpp"
#include "shaders_compiled.hpp"
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

struct SpawnBillboard
{
    Vec3 basePos;
    float halfWidth = kMinSpawnBillboardHalfWidth;
    float height = kMinSpawnBillboardHeight;
    float distanceSq = 0.0f;
    SDL_FColor color = {1.0f, 1.0f, 1.0f, 0.75f};
    std::string textureName;
    uint32_t attributes = 0;
};

SpawnBillboard makeOutdoorSpawnBillboard(const formats::ODMSpawnPoint& spawn, const Vec3& cameraPos,
                                         [[maybe_unused]] const game::RuntimeConfig* config,
                                         const OutdoorRenderer::MonsterSpriteLookup& monsterLookup,
                                         const formats::SpriteFrameTable* spriteFrameTable)
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
            uint32_t ticks = SDL_GetTicks();
            // Implement the 8-directional facing system.
            // In MM7, circle is 2048 units. Slowly spin in place to simulate idle rotation if
            // stationary.
            int facing = (ticks / 10) % 2048;

            // Calculate angle from sprite to camera
            float dx = cameraPos.x - sprite.basePos.x;
            float dz = cameraPos.z - sprite.basePos.z;
            float camAngle = std::atan2(dx, dz);

            // Convert to MM7 angle (0 to 2047)
            int camFacing = static_cast<int>((camAngle + M_PI) * 1024.0f / M_PI) % 2048;
            if (camFacing < 0)
                camFacing += 2048;

            // Difference between monster facing and camera angle, mapped to 8 directions
            int directionIndex = ((facing - camFacing + 2048 + 128) >> 8) & 7;

            // e.g. "Goblina" -> "Goblin01"
            std::string prefix = baseName.substr(0, std::min<size_t>(baseName.length(), 6));

            // To be precise we need to know the animation set.
            // MM7 uses "w" for walk, "s" for stand, "a" for attack, etc.
            // Let's assume stand (s) or walk (w).
            std::string frameName =
                std::format("{}w{:02d}", prefix, directionIndex + 1); // Walk animation, frame 1-8

            sprite.textureName = frameName;
            if (spriteFrameTable)
            {
                auto entry = spriteFrameTable->findEntryByIcon(frameName);
                if (entry)
                {
                    if (!entry->textureName.empty())
                        sprite.textureName = entry->textureName;
                    sprite.attributes = entry->attributes;
                }
            }
        }
    }

    if (monsterLookup)
    {
        std::string baseName = monsterLookup(spawn.objectType);
        if (!baseName.empty())
        {
            uint32_t ticks = SDL_GetTicks();
            // MM7 monsters slowly spin in place when idle. 2048 is a full circle.
            // Let's simulate a slow spin by using ticks.
            int facing = (ticks / 10) % 2048; 
            
            // Camera relative angle
            float dx = cameraPos.x - sprite.basePos.x;
            float dz = cameraPos.z - sprite.basePos.z;
            float camAngle = std::atan2(dx, dz);
            int camFacing = static_cast<int>((camAngle + M_PI) * 1024.0f / M_PI) % 2048;
            if (camFacing < 0) camFacing += 2048;

            // Direction index is the difference between where the sprite is facing and where the camera is
            int relativeFacing = (facing - camFacing + 2048) % 2048;
            int directionIndex = ((relativeFacing + 128) >> 8) & 7;

            // e.g. "Goblina" -> "Goblin01"
            std::string prefix = baseName.substr(0, std::min<size_t>(baseName.length(), 6));

            // Use walk animation frames 1-8 (in MM7, walk frame index might be related to time)
            int animFrame = (ticks / 150) % 4 + 1; // 4 frames for walk? Just an example.
            
            // But we don't have the frame table. We just want 8-directional facing.
            // Let's use the direction index (0-7) mapped to the expected 1-8 sprite naming pattern
            sprite.textureName = std::format("{}{:02d}", prefix, directionIndex + 1); // Using base + direction
        }
    }

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

} // namespace

OutdoorRenderer::OutdoorRenderer(SDLRenderer& renderer, util::ILogger& logger)
    : renderer(renderer), logger(logger)
{
    gpuDevice = renderer.getGPUDevice();
    if (gpuDevice)
    {
        initGPUPipeline();
    }
    else
    {
        logger.warning(
            "OutdoorRenderer: No SDL_GPUDevice available. Falling back to software projection.");
    }
}

OutdoorRenderer::~OutdoorRenderer()
{
    if (gpuDevice)
    {
        if (terrainPipeline)
        {
            SDL_ReleaseGPUGraphicsPipeline(gpuDevice, terrainPipeline);
        }
        if (vertexShader)
        {
            SDL_ReleaseGPUShader(gpuDevice, vertexShader);
        }
        if (fragmentShader)
        {
            SDL_ReleaseGPUShader(gpuDevice, fragmentShader);
        }
        if (terrainVertexBuffer)
        {
            SDL_ReleaseGPUBuffer(gpuDevice, terrainVertexBuffer);
        }
        if (terrainIndexBuffer)
        {
            SDL_ReleaseGPUBuffer(gpuDevice, terrainIndexBuffer);
        }
        if (defaultSampler)
        {
            SDL_ReleaseGPUSampler(gpuDevice, defaultSampler);
        }
    }
}

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
                             const Frustum* frustumOverride, 
                             SDL_GPUTexture* colorTex, SDL_GPUTexture* depthTex, SDL_Texture* blitTex)
{
    const auto& odmData = scene.getODMData();
    if (odmData.heightmap.empty())
    {
        return;
    }

    if (gpuInitialized && terrainIndexCount == 0) {
        buildGPUTerrain(odmData);
    }

    const float blend = clamp01(nightBlend);

    // Draw the sky via SDL_Renderer (software/2D path)
    renderSky(runtimeConfig, blend);

    if (gpuInitialized && terrainIndexCount > 0 && colorTex && depthTex) {
        // Flush the 2D renderer to ensure any pending texture uploads are processed
        // before we try to sample those textures in the raw GPU pass.
        SDL_FlushRenderer(renderer.getSDLRenderer());

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
        if (cmdBuf) {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = colorTex;
            // Transparent background so the SDL sky shows through!
            colorTarget.clear_color = {0.0f, 0.0f, 0.0f, 0.0f}; 
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPUDepthStencilTargetInfo depthTarget = {};
            depthTarget.texture = depthTex;
            depthTarget.clear_depth = 1.0f; // Clear depth to far plane
            depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
            depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            depthTarget.cycle = false;

            SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, &depthTarget);

            renderTerrainGPU(odmData, camera, runtimeConfig, blend, frustumOverride, cmdBuf, renderPass);

            SDL_EndGPURenderPass(renderPass);
            SDL_SubmitGPUCommandBuffer(cmdBuf);

            // Blit the transparent GPU result over the SDL sky!
            if (blitTex) {
                // Ensure proper blend mode so transparent background works
                SDL_SetTextureBlendMode(blitTex, SDL_BLENDMODE_BLEND);

                float w, h;
                SDL_GetTextureSize(blitTex, &w, &h);
                renderer.renderTexture(blitTex, 0, 0, static_cast<int>(w), static_cast<int>(h));
            }
        }
    } else {
        renderTerrain(odmData, camera, runtimeConfig, blend, frustumOverride);
    }

    // These draw on top of the terrain/sky using SDL_Renderer
    renderBuildings(odmData, camera, runtimeConfig, blend, frustumOverride);
    renderSpawnBillboards(odmData, camera, runtimeConfig, blend, frustumOverride);
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

    const int width = renderer.getViewportWidth();
    const int height = std::max(1, renderer.getViewportHeight() / 2);
    for (int y = 0; y < height; y++)
    {
        const float t = static_cast<float>(y) / static_cast<float>(std::max(1, height - 1));
        const float r = (1.0f - t) * static_cast<float>(top[0]) + t * static_cast<float>(bottom[0]);
        const float g = (1.0f - t) * static_cast<float>(top[1]) + t * static_cast<float>(bottom[1]);
        const float b = (1.0f - t) * static_cast<float>(top[2]) + t * static_cast<float>(bottom[2]);
        SDL_SetRenderDrawColorFloat(renderer.getSDLRenderer(), r / 255.0f, g / 255.0f, b / 255.0f,
                                    1.0f);
        SDL_RenderLine(renderer.getSDLRenderer(), 0.0f, static_cast<float>(y),
                       static_cast<float>(width), static_cast<float>(y));
    }
}

void OutdoorRenderer::renderTerrain(const formats::ODMMapData& odmData, const Camera& camera,
                                    const game::RuntimeConfig* runtimeConfig, float nightBlend,
                                    const Frustum* frustumOverride)
{
    const auto& viewProjection = camera.getViewProjectionMatrix();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());
    const Vec3 cameraPos = camera.getPosition();
    const OutdoorLightingParams terrainLighting =
        makeOutdoorLightingParams(runtimeConfig, nightBlend, true);

    constexpr int SIZE = formats::ODMMapData::TERRAIN_SIZE;
    constexpr float CELL_SIZE = 512.0f;
    constexpr float HALF = SIZE / 2.0f;
    Frustum localFrustum;
    const Frustum* frustum = frustumOverride;
    if (!frustum)
    {
        localFrustum.extractFromMatrix(viewProjection);
        frustum = &localFrustum;
    }

    // Helper: get world position for grid cell
    // Coordinate system: X = east/west, Y = up (height), Z = north/south
    auto worldPos = [&](int gx, int gy) -> Vec3
    {
        float wx = (static_cast<float>(gx) - HALF) * CELL_SIZE;
        float wz = (static_cast<float>(gy) - HALF) * CELL_SIZE;
        float wy = 0.0f;

        size_t idx = static_cast<size_t>(gy * SIZE + gx);
        if (idx < odmData.heightmap.size())
        {
            wy = static_cast<float>(odmData.heightmap[idx].height);
        }

        return {wx, wy, wz};
    };

    // Transform world point to clip space (no perspective divide yet)
    auto toClip = [&](Vec3 wp) -> Vec4 { return viewProjection * Vec4(wp, 1.0f); };

    // Height-based vertex coloring
    auto heightColor = [](float height) -> SDL_FColor
    {
        float t = std::clamp(height / 8000.0f, 0.0f, 1.0f);
        float r = 0.30f + t * 0.35f;
        float g = 0.45f - t * 0.15f;
        float b = 0.20f + t * 0.05f;
        return {r, g, b, 1.0f};
    };
    auto litTerrainColor = [&](const Vec3& worldPos) -> SDL_FColor
    {
        const float dx = worldPos.x - cameraPos.x;
        const float dy = worldPos.y - cameraPos.y;
        const float dz = worldPos.z - cameraPos.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        return applyOutdoorLighting(heightColor(worldPos.y), dist, terrainLighting);
    };

    // Batch all terrain triangles for a single draw call
    std::vector<SDL_Vertex> batchVerts;
    std::vector<int> batchIndices;
    batchVerts.reserve(65536);
    batchIndices.reserve(65536);

    ClipVertex clipped[MAX_CLIP_VERTS];

    for (int gy = 0; gy < SIZE - 1; gy++)
    {
        int gx = 0;
        while (gx < SIZE - 1)
        {
            Vec3 p00 = worldPos(gx, gy);

            // Center of quad for distance check
            float cx = p00.x + CELL_SIZE * 0.5f;
            float cy = p00.y;
            float cz = p00.z + CELL_SIZE * 0.5f;
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

            // Clip-space vertices for the 4 corners
            Vec4 c00 = toClip(p00);
            Vec4 c10 = toClip(p10);
            Vec4 c01 = toClip(p01);
            Vec4 c11 = toClip(p11);

            SDL_FColor col00 = litTerrainColor(p00);
            SDL_FColor col10 = litTerrainColor(p10);
            SDL_FColor col01 = litTerrainColor(p01);
            SDL_FColor col11 = litTerrainColor(p11);

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

                // Tessellate clip-space polygons to minimize affine texture warping
                std::vector<ClipVertex> tessellatedVerts;
                for (int i = 1; i + 1 < count; i++)
                {
                    tessellateTriangle(clipped[0], clipped[i], clipped[i + 1], tessellatedVerts);
                }

                int baseIdx = static_cast<int>(batchVerts.size());
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
                    batchVerts.push_back(sv);
                }

                if (anyFailed)
                {
                    // Roll back partially added vertices
                    batchVerts.resize(static_cast<size_t>(baseIdx));
                    continue;
                }

                // Append indices for tessellated triangles
                for (size_t i = 0; i < tessellatedVerts.size(); i++)
                {
                    batchIndices.push_back(baseIdx + static_cast<int>(i));
                }
            }

            gx += step;
        }
    }

    // Single batched draw call for all terrain
    if (!batchVerts.empty() && !batchIndices.empty())
    {
        SDL_RenderGeometry(renderer.getSDLRenderer(), nullptr, batchVerts.data(),
                           static_cast<int>(batchVerts.size()), batchIndices.data(),
                           static_cast<int>(batchIndices.size()));
    }
}

void OutdoorRenderer::renderBuildings(const formats::ODMMapData& odmData, const Camera& camera,
                                      const game::RuntimeConfig* runtimeConfig, float nightBlend,
                                      const Frustum* frustumOverride)
{
    if (odmData.buildings.empty())
    {
        return;
    }

    logger.debug("Rendering " + std::to_string(odmData.buildings.size()) + " outdoor buildings");

    const auto& viewProjection = camera.getViewProjectionMatrix();
    const Vec3 cameraPos = camera.getPosition();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());
    const OutdoorLightingParams buildingLighting =
        makeOutdoorLightingParams(runtimeConfig, nightBlend, false);
    const bool animateWater = (runtimeConfig == nullptr) || !runtimeConfig->noWavyWater;
    const float waterTimeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;
    Frustum localFrustum;
    const Frustum* frustum = frustumOverride;
    if (!frustum)
    {
        localFrustum.extractFromMatrix(viewProjection);
        frustum = &localFrustum;
    }

    // Collect all building faces with their world-space info for painter's algorithm
    struct FaceRef
    {
        const formats::ParsedBuilding* building;
        uint32_t faceIndex;
        float distance;
    };

    std::vector<FaceRef> sortedFaces;

    for (const auto& building : odmData.buildings)
    {
        const float minX = static_cast<float>(building.worldX + building.minX);
        const float maxX = static_cast<float>(building.worldX + building.maxX);
        const float minY = static_cast<float>(building.worldZ + building.minZ);
        const float maxY = static_cast<float>(building.worldZ + building.maxZ);
        const float minZ = static_cast<float>(building.worldY + building.minY);
        const float maxZ = static_cast<float>(building.worldY + building.maxY);

        if (!frustum->testAABB(minX, minY, minZ, maxX, maxY, maxZ))
        {
            continue;
        }

        for (uint32_t fi = 0; fi < building.faces.size(); fi++)
        {
            const auto& face = building.faces[fi];
            if (face.numVertices < 3 || face.vertexIndices.empty())
            {
                continue;
            }
            if (face.isInvisible())
            {
                continue;
            }

            // Compute face centroid in world space
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
            float inv = 1.0f / static_cast<float>(valid);
            cx = cx * inv + static_cast<float>(building.worldX);
            cy = cy * inv + static_cast<float>(building.worldZ);
            cz = cz * inv + static_cast<float>(building.worldY);

            // Backface culling
            float viewX = cx - cameraPos.x;
            float viewY = cy - cameraPos.y;
            float viewZ = cz - cameraPos.z;
            const Vec3 faceNormal =
                gameplayToRenderDirection(face.normalFX, face.normalFY, face.normalFZ);
            float dot = faceNormal.x * viewX + faceNormal.y * viewY + faceNormal.z * viewZ;
            if (dot > 0.0f)
            {
                continue;
            }

            float dist = viewX * viewX + viewY * viewY + viewZ * viewZ;
            sortedFaces.push_back({&building, fi, dist});
        }
    }

    // Sort back-to-front
    std::sort(sortedFaces.begin(), sortedFaces.end(),
              [](const FaceRef& a, const FaceRef& b) { return a.distance > b.distance; });

    ClipVertex polyIn[MAX_CLIP_VERTS];
    ClipVertex polyOut[MAX_CLIP_VERTS];

    // Render each face
    for (const auto& fr : sortedFaces)
    {
        const auto& building = *fr.building;
        const auto& face = building.faces[fr.faceIndex];

        // Build clip-space polygon
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
            Vec3 wp = gameplayToRenderPosition(
                static_cast<float>(v.x) + static_cast<float>(building.worldX),
                static_cast<float>(v.y) + static_cast<float>(building.worldY),
                static_cast<float>(v.z) + static_cast<float>(building.worldZ));

            Vec4 clip = viewProjection * Vec4(wp, 1.0f);
            if (clip.w >= CLIP_NEAR_EPSILON)
            {
                allBehind = false;
            }

            // Surface-type coloring
            SDL_FColor color;
            if (face.isFloor())
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
            color = applyOutdoorLighting(color, dist, buildingLighting);
            if (face.isWater() && animateWater)
            {
                const float wave = std::sin(waterWavePhase(wp.x, wp.z, waterTimeSeconds));
                color.r = std::clamp(color.r + wave * 0.020f, 0.0f, 1.0f);
                color.g = std::clamp(color.g + wave * 0.035f, 0.0f, 1.0f);
                color.b = std::clamp(color.b + wave * 0.065f, 0.0f, 1.0f);
                color.a = std::clamp(color.a + 0.06f + wave * 0.03f, 0.0f, 1.0f);
            }

            float u = 0.0f, uv = 0.0f;
            if (i < face.uCoords.size() && i < face.vCoords.size())
            {
                u = static_cast<float>(face.uCoords[i]) / 256.0f;
                uv = static_cast<float>(face.vCoords[i]) / 256.0f;
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

        // Near-plane clip
        int clippedCount = clipPolygonNearPlane(polyIn, polyCount, polyOut);
        if (clippedCount < 3)
        {
            continue;
        }

        // Tessellate clip-space polygons to minimize affine texture warping
        std::vector<ClipVertex> tessellatedVerts;
        for (int i = 1; i + 1 < clippedCount; i++)
        {
            tessellateTriangle(polyOut[0], polyOut[i], polyOut[i + 1], tessellatedVerts);
        }

        // Project to screen
        std::vector<SDL_Vertex> verts;
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

        std::vector<int> indices;
        indices.reserve(verts.size());
        for (size_t i = 0; i < verts.size(); i++)
        {
            indices.push_back(static_cast<int>(i));
        }

        // Texture lookup
        SDL_Texture* texture = nullptr;
        if (textureLookup && !face.textureName.empty())
        {
            texture = textureLookup(face.textureName);
        }

        SDL_RenderGeometry(renderer.getSDLRenderer(), texture, verts.data(),
                           static_cast<int>(verts.size()), indices.data(),
                           static_cast<int>(indices.size()));
    }
}

void OutdoorRenderer::renderSpawnBillboards(const formats::ODMMapData& odmData,
                                            const Camera& camera,
                                            const game::RuntimeConfig* runtimeConfig,
                                            float nightBlend, const Frustum* frustumOverride)
{
    if (odmData.spawns.empty())
    {
        return;
    }

    if (runtimeConfig && runtimeConfig->noMonsters)
    {
        return;
    }

    const auto& viewProjection = camera.getViewProjectionMatrix();
    const Vec3 cameraPos = camera.getPosition();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());

    Frustum localFrustum;
    const Frustum* frustum = frustumOverride;
    if (!frustum)
    {
        localFrustum.extractFromMatrix(viewProjection);
        frustum = &localFrustum;
    }

    const OutdoorLightingParams billboardLighting =
        makeOutdoorLightingParams(runtimeConfig, nightBlend, false);

    std::vector<SpawnBillboard> sprites;
    sprites.reserve(odmData.spawns.size());
    for (const auto& spawn : odmData.spawns)
    {
        if (spawn.objectType == 0)
        {
            continue;
        }

        const float cullRadius = std::max(64.0f, static_cast<float>(spawn.radius) * 2.0f);
        const Vec3 renderPos = gameplayToRenderPosition(
            static_cast<float>(spawn.x), static_cast<float>(spawn.y), static_cast<float>(spawn.z));
        if (!frustum->testSphere(renderPos.x, renderPos.y, renderPos.z, cullRadius))
        {
            continue;
        }

        sprites.push_back(makeOutdoorSpawnBillboard(spawn, cameraPos, runtimeConfig,
                                                    monsterSpriteLookup, spriteFrameTable));
    }

    std::sort(sprites.begin(), sprites.end(), [](const SpawnBillboard& a, const SpawnBillboard& b)
              { return a.distanceSq > b.distanceSq; });

    if (!sprites.empty())
    {
        SDL_SetRenderDrawBlendMode(renderer.getSDLRenderer(), SDL_BLENDMODE_BLEND);
    }

    const Vec3 worldUp = Vec3::up();
    for (auto sprite : sprites)
    {
        Vec3 toCamera = cameraPos - sprite.basePos;
        toCamera.y = 0.0f;
        if (toCamera.lengthSquared() < 0.001f)
        {
            toCamera = Vec3::forward();
        }
        const Vec3 forward = toCamera.normalized();
        Vec3 right = worldUp.cross(forward);
        if (right.lengthSquared() < 0.001f)
        {
            right = Vec3::right();
        }
        right.normalize();

        const float distance = std::sqrt(sprite.distanceSq);
        SDL_FColor drawColor = applyOutdoorLighting(sprite.color, distance, billboardLighting);
        SDL_Texture* texture = nullptr;
        float actualHalfWidth = sprite.halfWidth;
        float actualHeight = sprite.height;

        if (textureLookup && !sprite.textureName.empty())
        {
            texture = textureLookup(sprite.textureName);
            if (texture)
            {
                float width = 0.0f;
                float height = 0.0f;
                if (SDL_GetTextureSize(texture, &width, &height))
                {
                    const float scale = 0.6f;
                    actualHalfWidth = (width * scale) * 0.5f;
                    actualHeight = height * scale;
                }
            }
        }

        Vec3 drawPos = sprite.basePos;
        uint32_t ticks = SDL_GetTicks();

        if (sprite.attributes & 0x80) // Oscillate
        {
            float offset = std::sin(ticks * 0.01f) * 4.0f;
            drawPos.y += offset;
        }
        if (sprite.attributes & 0x02) // Translucent
        {
            drawColor.a *= 0.5f;
        }
        if (sprite.attributes & 0x40) // Mirror / Shimmer
        {
            float shimmer = (std::sin(ticks * 0.005f) + 1.0f) * 0.5f;
            drawColor.r = std::clamp(drawColor.r + shimmer * 0.2f, 0.0f, 1.0f);
            drawColor.g = std::clamp(drawColor.g + shimmer * 0.2f, 0.0f, 1.0f);
            drawColor.b = std::clamp(drawColor.b + shimmer * 0.2f, 0.0f, 1.0f);
        }

        const Vec3 bottomLeft = drawPos - right * actualHalfWidth;
        const Vec3 bottomRight = drawPos + right * actualHalfWidth;
        const Vec3 topLeft = bottomLeft + worldUp * actualHeight;
        const Vec3 topRight = bottomRight + worldUp * actualHeight;

        const Vec4 clipBL = viewProjection * Vec4(bottomLeft, 1.0f);
        const Vec4 clipBR = viewProjection * Vec4(bottomRight, 1.0f);
        const Vec4 clipTL = viewProjection * Vec4(topLeft, 1.0f);
        const Vec4 clipTR = viewProjection * Vec4(topRight, 1.0f);
        if (clipBL.w < CLIP_NEAR_EPSILON || clipBR.w < CLIP_NEAR_EPSILON ||
            clipTL.w < CLIP_NEAR_EPSILON || clipTR.w < CLIP_NEAR_EPSILON)
        {
            continue;
        }

        float sxBL, syBL, sxBR, syBR, sxTL, syTL, sxTR, syTR;
        if (!projectClipToScreen(clipBL, vpW, vpH, sxBL, syBL) ||
            !projectClipToScreen(clipBR, vpW, vpH, sxBR, syBR) ||
            !projectClipToScreen(clipTL, vpW, vpH, sxTL, syTL) ||
            !projectClipToScreen(clipTR, vpW, vpH, sxTR, syTR))
        {
            continue;
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
        vertices[0].tex_coord = {0.0f, 1.0f};
        vertices[1].tex_coord = {1.0f, 1.0f};
        vertices[2].tex_coord = {0.0f, 0.0f};
        vertices[3].tex_coord = {1.0f, 0.0f};

        constexpr int indices[6] = {0, 1, 2, 2, 1, 3};
        SDL_RenderGeometry(renderer.getSDLRenderer(), texture, vertices, 4, indices, 6);
    }
}

void OutdoorRenderer::initGPUPipeline()
{
    if (!gpuDevice)
        return;

    if (!SDL_ShaderCross_Init())
    {
        logger.error("Failed to initialize SDL_shadercross: " + std::string(SDL_GetError()));
        return;
    }

    SDL_ShaderCross_SPIRV_Info vertexSpirv = {};
    vertexSpirv.bytecode = shaders::world_vert_data;
    vertexSpirv.bytecode_size = shaders::world_vert_size;
    vertexSpirv.entrypoint = "main";
    vertexSpirv.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;

    SDL_ShaderCross_GraphicsShaderResourceInfo vertexResInfo = {};
    vertexResInfo.num_samplers = 0;
    vertexResInfo.num_storage_textures = 0;
    vertexResInfo.num_storage_buffers = 0;
    vertexResInfo.num_uniform_buffers = 1;

    vertexShader =
        SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(gpuDevice, &vertexSpirv, &vertexResInfo, 0);

    SDL_ShaderCross_SPIRV_Info fragmentSpirv = {};
    fragmentSpirv.bytecode = shaders::world_frag_data;
    fragmentSpirv.bytecode_size = shaders::world_frag_size;
    fragmentSpirv.entrypoint = "main";
    fragmentSpirv.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

    SDL_ShaderCross_GraphicsShaderResourceInfo fragmentResInfo = {};
    fragmentResInfo.num_samplers = 1;
    fragmentResInfo.num_storage_textures = 0;
    fragmentResInfo.num_storage_buffers = 0;
    fragmentResInfo.num_uniform_buffers = 1;

    fragmentShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(gpuDevice, &fragmentSpirv,
                                                                    &fragmentResInfo, 0);

    if (!vertexShader || !fragmentShader)
    {
        logger.error("Failed to compile cross shaders: " + std::string(SDL_GetError()));
        SDL_ShaderCross_Quit();
        return;
    }

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    SDL_GPUVertexBufferDescription vertexBufferDesc[1] = {};
    vertexBufferDesc[0].slot = 0;
    vertexBufferDesc[0].pitch = sizeof(GPUVertex);
    vertexBufferDesc[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesc[0].instance_step_rate = 0;

    SDL_GPUVertexAttribute vertexAttributes[3] = {};
    // Position
    vertexAttributes[0].location = 0;
    vertexAttributes[0].buffer_slot = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].offset = offsetof(GPUVertex, x);
    // Color
    vertexAttributes[1].location = 1;
    vertexAttributes[1].buffer_slot = 0;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertexAttributes[1].offset = offsetof(GPUVertex, r);
    // Texcoord
    vertexAttributes[2].location = 2;
    vertexAttributes[2].buffer_slot = 0;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[2].offset = offsetof(GPUVertex, u);

    pipelineInfo.vertex_input_state.num_vertex_attributes = 3;
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesc;

    // Rasterizer state
    pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE; // Y-flip in shader reverses winding order

    // Depth Stencil state
    pipelineInfo.depth_stencil_state.enable_depth_test = true;
    pipelineInfo.depth_stencil_state.enable_depth_write = true;
    pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    // Target state (Assuming standard rendering out to window for now)
    pipelineInfo.target_info.num_color_targets = 1;
    SDL_GPUColorTargetDescription targetDesc = {};
    targetDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    
    // Enable blending so transparent pixels from the texture do not overwrite the background sky
    targetDesc.blend_state.enable_blend = true;
    targetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    targetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    targetDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    targetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    targetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    targetDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    targetDesc.blend_state.enable_color_write_mask = false; // default all channels
    
    pipelineInfo.target_info.color_target_descriptions = &targetDesc;
    pipelineInfo.target_info.has_depth_stencil_target = true;
    pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;

    terrainPipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &pipelineInfo);
    if (!terrainPipeline)
    {
        logger.error("Failed to create pipeline: " + std::string(SDL_GetError()));
    }

    SDL_GPUSamplerCreateInfo samplerInfo = {};
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    defaultSampler = SDL_CreateGPUSampler(gpuDevice, &samplerInfo);

    gpuInitialized = true;
}

void OutdoorRenderer::invalidateGPUCache()
{
    terrainIndexCount = 0;
    terrainDrawCalls.clear();
    if (gpuDevice)
    {
        if (terrainVertexBuffer)
        {
            SDL_ReleaseGPUBuffer(gpuDevice, terrainVertexBuffer);
            terrainVertexBuffer = nullptr;
        }
        if (terrainIndexBuffer)
        {
            SDL_ReleaseGPUBuffer(gpuDevice, terrainIndexBuffer);
            terrainIndexBuffer = nullptr;
        }
    }
}

void OutdoorRenderer::buildGPUTerrain(const formats::ODMMapData& odmData)
{
    if (!gpuDevice || odmData.heightmap.empty())
        return;

    terrainDrawCalls.clear();

    constexpr int SIZE = formats::ODMMapData::TERRAIN_SIZE;
    constexpr float CELL_SIZE = 512.0f;
    constexpr float HALF = SIZE / 2.0f;

    // We can have multiple textures (up to 4 tile sets usually in MM7)
    // We group the quads by their tileIndex to batch draw calls.
    std::unordered_map<uint8_t, std::vector<uint32_t>> tileQuads;

    for (int gy = 0; gy < SIZE - 1; gy++)
    {
        for (int gx = 0; gx < SIZE - 1; gx++)
        {
            size_t idx = static_cast<size_t>(gy * SIZE + gx);
            uint8_t tileIndex = odmData.heightmap[idx].tileIndex;
            tileQuads[tileIndex].push_back(gy * SIZE + gx);
        }
    }

    std::vector<GPUVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(SIZE * SIZE);

    // Precompute all vertices
    for (int gy = 0; gy < SIZE; gy++)
    {
        for (int gx = 0; gx < SIZE; gx++)
        {
            float wx = (static_cast<float>(gx) - HALF) * CELL_SIZE;
            float wz = (static_cast<float>(gy) - HALF) * CELL_SIZE;
            float wy = 0.0f;

            size_t idx = static_cast<size_t>(gy * SIZE + gx);
            if (idx < odmData.heightmap.size())
            {
                wy = static_cast<float>(odmData.heightmap[idx].height);
            }

            GPUVertex v;
            v.x = wx;
            v.y = wy;
            v.z = wz;

            float t = std::clamp(wy / 8000.0f, 0.0f, 1.0f);
            v.r = 0.30f + t * 0.35f;
            v.g = 0.45f - t * 0.15f;
            v.b = 0.20f + t * 0.05f;
            v.a = 1.0f;

            v.u = static_cast<float>(gx % 2); // Map 0..1..0 over grid points
            v.v = static_cast<float>(gy % 2); // Map 0..1..0 over grid points
            vertices.push_back(v);
        }
    }

    // Build indices per texture
    for (const auto& [tileIndex, quadList] : tileQuads)
    {
        std::string textureName;
        if (tileIndex < odmData.tileTextures.size())
        {
            textureName = odmData.tileTextures[tileIndex];
        }

        uint32_t indexStart = static_cast<uint32_t>(indices.size());

        for (uint32_t quadIdx : quadList)
        {
            int gy = quadIdx / SIZE;
            int gx = quadIdx % SIZE;

            uint32_t i00 = gy * SIZE + gx;
            uint32_t i10 = gy * SIZE + (gx + 1);
            uint32_t i01 = (gy + 1) * SIZE + gx;
            uint32_t i11 = (gy + 1) * SIZE + (gx + 1);

            indices.push_back(i00);
            indices.push_back(i01);
            indices.push_back(i10);

            indices.push_back(i10);
            indices.push_back(i01);
            indices.push_back(i11);
        }

        uint32_t indexCount = static_cast<uint32_t>(indices.size()) - indexStart;
        if (indexCount > 0)
        {
            terrainDrawCalls.push_back({textureName, indexStart, indexCount});
        }
    }

    terrainIndexCount = static_cast<uint32_t>(indices.size());

    if (terrainVertexBuffer)
        SDL_ReleaseGPUBuffer(gpuDevice, terrainVertexBuffer);
    if (terrainIndexBuffer)
        SDL_ReleaseGPUBuffer(gpuDevice, terrainIndexBuffer);

    SDL_GPUBufferCreateInfo vboInfo = {};
    vboInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vboInfo.size = vertices.size() * sizeof(GPUVertex);
    terrainVertexBuffer = SDL_CreateGPUBuffer(gpuDevice, &vboInfo);

    SDL_GPUBufferCreateInfo iboInfo = {};
    iboInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    iboInfo.size = indices.size() * sizeof(uint32_t);
    terrainIndexBuffer = SDL_CreateGPUBuffer(gpuDevice, &iboInfo);

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = vboInfo.size + iboInfo.size;
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);

    void* mapData = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
    memcpy(mapData, vertices.data(), vboInfo.size);
    memcpy(static_cast<uint8_t*>(mapData) + vboInfo.size, indices.data(), iboInfo.size);
    SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTransferBufferLocation srcVbo = {transferBuffer, 0};
    SDL_GPUBufferRegion dstVbo = {terrainVertexBuffer, 0, vboInfo.size};
    SDL_UploadToGPUBuffer(copyPass, &srcVbo, &dstVbo, false);

    SDL_GPUTransferBufferLocation srcIbo = {transferBuffer, static_cast<Uint32>(vboInfo.size)};
    SDL_GPUBufferRegion dstIbo = {terrainIndexBuffer, 0, iboInfo.size};
    SDL_UploadToGPUBuffer(copyPass, &srcIbo, &dstIbo, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
}

void OutdoorRenderer::renderTerrainGPU(const formats::ODMMapData& odmData, const Camera& camera,
                                       const game::RuntimeConfig* runtimeConfig, float nightBlend,
                                       const Frustum* frustumOverride, SDL_GPUCommandBuffer* cmdBuf,
                                       SDL_GPURenderPass* renderPass)
{
    (void)odmData;
    (void)runtimeConfig;
    (void)frustumOverride;

    if (!gpuInitialized || !terrainVertexBuffer || !terrainIndexBuffer || !terrainPipeline ||
        !cmdBuf || !renderPass)
    {
        return;
    }

    // Set Viewport
    SDL_GPUViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.w = static_cast<float>(renderer.getViewportWidth());
    viewport.h = static_cast<float>(renderer.getViewportHeight());
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(renderPass, &viewport);

    // Bind Pipeline
    SDL_BindGPUGraphicsPipeline(renderPass, terrainPipeline);

    // Bind Buffers
    SDL_GPUBufferBinding vboBinding = {terrainVertexBuffer, 0};
    SDL_BindGPUVertexBuffers(renderPass, 0, &vboBinding, 1);

    SDL_GPUBufferBinding iboBinding = {terrainIndexBuffer, 0};
    SDL_BindGPUIndexBuffer(renderPass, &iboBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // Upload Uniforms (View/Projection Matrix)
    Mat4 vp = camera.getViewProjectionMatrix();
    // SDL_GPU expects data to be pushed to a uniform buffer slot.
    SDL_PushGPUVertexUniformData(cmdBuf, 0, vp.m.data(), sizeof(float) * 16);

    // Push fragment uniform (night blend)
    float fragUniforms[4] = {nightBlend, 0.0f, 0.0f, 0.0f}; // padded to 16 bytes for alignment
    SDL_PushGPUFragmentUniformData(cmdBuf, 0, fragUniforms, sizeof(fragUniforms));

    for (const auto& drawCall : terrainDrawCalls)
    {
        SDL_GPUTexture* gpuTex = nullptr;
        if (textureLookup) {
            std::string queryName = drawCall.textureName;
            if (queryName.empty()) {
                // Fallback texture name if none specified (like grass)
                queryName = "grs1";
            }

            if (SDL_Texture* tex = textureLookup(queryName)) {
                SDL_PropertiesID texProps = SDL_GetTextureProperties(tex);
                gpuTex = (SDL_GPUTexture*)SDL_GetPointerProperty(texProps, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr);
                if (!gpuTex) {
                    logger.warning("No GPU texture for " + queryName);
                }
            } else {
                logger.warning("Texture lookup failed for " + queryName);
            }
        }

        if (gpuTex) {
            SDL_GPUTextureSamplerBinding samplerBinding = { gpuTex, defaultSampler };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &samplerBinding, 1);
            SDL_DrawGPUIndexedPrimitives(renderPass, drawCall.indexCount, 1, drawCall.indexStart, 0, 0);
        } else {
            // If we still don't have a texture, we must skip because the shader requires a sampler
            logger.warning("Skipping draw call due to missing GPU texture!");
        }
    }
}

} // namespace runeharbor::graphics
