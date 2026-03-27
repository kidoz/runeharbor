// SPDX-License-Identifier: MIT
#include "indoor_renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>
#include <string_view>

#include <SDL3_shadercross/SDL_shadercross.h>
#include <cctype>
#include <cmath>

#include "../game/game_world.hpp"
#include "clip_utils.hpp"
#include "shaders_compiled.hpp"
#include "visibility.hpp"

namespace runeharbor::graphics
{
namespace
{
constexpr uint32_t kNoLightFaceBit = 0x0800u;
constexpr float kMinBillboardHalfWidth = 24.0f;
constexpr float kMinBillboardHeight = 56.0f;

std::string toLowerCopy(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool containsToken(std::string_view haystack, std::string_view needle)
{
    if (needle.empty())
    {
        return false;
    }
    const std::string hayLower = toLowerCopy(haystack);
    const std::string needleLower = toLowerCopy(needle);
    return hayLower.find(needleLower) != std::string::npos;
}

float waterWavePhase(float worldX, float worldZ, float timeSeconds)
{
    return timeSeconds * 2.7f + worldX * 0.0022f + worldZ * 0.0017f;
}

SDL_FColor surfaceColor(const formats::ParsedFace& face)
{
    if (face.isFloor())
    {
        return {0.45f, 0.42f, 0.38f, 1.0f};
    }
    if (face.isCeiling())
    {
        return {0.30f, 0.28f, 0.25f, 1.0f};
    }
    if (face.isWater())
    {
        return {0.15f, 0.30f, 0.55f, 0.8f};
    }
    if (face.isLava())
    {
        return {0.70f, 0.20f, 0.05f, 1.0f};
    }
    if (face.isPortal())
    {
        return {0.10f, 0.10f, 0.10f, 0.3f};
    }
    return {0.55f, 0.50f, 0.42f, 1.0f};
}

float sectorAmbientScale(const formats::BLVMapData& blvData, const formats::ParsedFace& face)
{
    if (face.sectorId < blvData.sectors.size())
    {
        const int ambient =
            std::clamp(static_cast<int>(blvData.sectors[face.sectorId].minAmbientLight), 0, 255);
        return 0.30f + (static_cast<float>(ambient) / 255.0f) * 0.70f;
    }
    return 0.70f;
}

SDL_FColor dynamicLightContribution(const formats::BLVMapData& blvData,
                                    const formats::ParsedFace& face, float x, float y, float z,
                                    const LightStack& stationaryLights,
                                    const LightStack& mobileLights)
{
    SDL_FColor add = {0.0f, 0.0f, 0.0f, 0.0f};
    if ((face.attributes & kNoLightFaceBit) != 0)
    {
        return add;
    }

    auto addLight =
        [&](float lx, float ly, float lz, float radius, float brightness, float r, float g, float b)
    {
        const float distSq = (lx - x) * (lx - x) + (ly - y) * (ly - y) + (lz - z) * (lz - z);
        if (distSq >= radius * radius)
        {
            return;
        }

        const float dist = std::sqrt(distSq);
        const float falloff = 1.0f - dist / radius;
        const float intensity = falloff * brightness;
        add.r += r * intensity;
        add.g += g * intensity;
        add.b += b * intensity;
    };

    // 1. Process baked map lights
    bool usedSectorLights = false;
    if (face.sectorId < blvData.sectors.size())
    {
        const auto& lightIds = blvData.sectors[face.sectorId].lightIds;
        for (uint16_t lightId : lightIds)
        {
            if (lightId >= blvData.lights.size())
            {
                continue;
            }
            usedSectorLights = true;
            const auto& light = blvData.lights[lightId];
            addLight(static_cast<float>(light.x), static_cast<float>(light.y),
                     static_cast<float>(light.z), std::max(1.0f, static_cast<float>(light.radius)),
                     std::clamp(static_cast<float>(light.brightness) / 96.0f, 0.10f, 1.0f),
                     static_cast<float>(light.red) / 255.0f,
                     static_cast<float>(light.green) / 255.0f,
                     static_cast<float>(light.blue) / 255.0f);
        }
    }

    if (!usedSectorLights)
    {
        for (const auto& light : blvData.lights)
        {
            addLight(static_cast<float>(light.x), static_cast<float>(light.y),
                     static_cast<float>(light.z), std::max(1.0f, static_cast<float>(light.radius)),
                     std::clamp(static_cast<float>(light.brightness) / 96.0f, 0.10f, 1.0f),
                     static_cast<float>(light.red) / 255.0f,
                     static_cast<float>(light.green) / 255.0f,
                     static_cast<float>(light.blue) / 255.0f);
        }
    }

    // 2. Process Stationary Light Stack
    for (const auto& light : stationaryLights.getLights())
    {
        if (light.active)
        {
            addLight(light.position.x, light.position.y, light.position.z, light.radius,
                     light.brightness, light.color.r, light.color.g, light.color.b);
        }
    }

    // 3. Process Mobile Light Stack
    for (const auto& light : mobileLights.getLights())
    {
        if (light.active)
        {
            addLight(light.position.x, light.position.y, light.position.z, light.radius,
                     light.brightness, light.color.r, light.color.g, light.color.b);
        }
    }

    add.r = std::clamp(add.r, 0.0f, 1.0f);
    add.g = std::clamp(add.g, 0.0f, 1.0f);
    add.b = std::clamp(add.b, 0.0f, 1.0f);
    return add;
}

SDL_FColor litIndoorFaceColor(const formats::BLVMapData& blvData, const formats::ParsedFace& face,
                              float x, float y, float z, const LightStack& stationaryLights,
                              const LightStack& mobileLights)
{
    SDL_FColor color = surfaceColor(face);
    if ((face.attributes & kNoLightFaceBit) != 0)
    {
        return color;
    }

    const float ambient = sectorAmbientScale(blvData, face);
    const SDL_FColor dynamic =
        dynamicLightContribution(blvData, face, x, y, z, stationaryLights, mobileLights);
    color.r = std::clamp(color.r * ambient + dynamic.r * 0.75f, 0.0f, 1.0f);
    color.g = std::clamp(color.g * ambient + dynamic.g * 0.75f, 0.0f, 1.0f);
    color.b = std::clamp(color.b * ambient + dynamic.b * 0.75f, 0.0f, 1.0f);
    return color;
}

struct BillboardSprite
{
    Vec3 basePos;
    float halfWidth = kMinBillboardHalfWidth;
    float height = kMinBillboardHeight;
    float distanceSq = 0.0f;
    SDL_FColor color = {1.0f, 1.0f, 1.0f, 1.0f};
    std::string textureName;
    uint32_t attributes = 0;
};

BillboardSprite makeIndoorDecorationBillboard(const formats::ParsedDecoration& decoration,
                                              const Vec3& cameraPos,
                                              const formats::SpriteFrameTable* spriteFrameTable,
                                              [[maybe_unused]] uint32_t ticks)
{
    BillboardSprite sprite;
    sprite.basePos = {static_cast<float>(decoration.x), static_cast<float>(decoration.y),
                      static_cast<float>(decoration.z)};
    sprite.textureName = decoration.name;

    if (spriteFrameTable)
    {
        auto entry = spriteFrameTable->findEntryByIcon(decoration.name);
        if (entry && !entry->textureName.empty())
        {
            sprite.textureName = entry->textureName;
            sprite.attributes = entry->attributes;
        }
    }

    const std::string lowerName = toLowerCopy(decoration.name);
    sprite.color = {1.0f, 1.0f, 1.0f, 1.0f};
    if (containsToken(lowerName, "torch") || containsToken(lowerName, "fire"))
    {
        sprite.color = {1.0f, 0.82f, 0.50f, 0.78f};
    }
    else if (containsToken(lowerName, "water") || containsToken(lowerName, "mist"))
    {
        sprite.color = {0.66f, 0.82f, 1.0f, 0.72f};
    }
    else if (containsToken(lowerName, "effpar"))
    {
        sprite.color = {1.0f, 1.0f, 1.0f, 0.66f};
    }

    sprite.halfWidth = 36.0f;
    sprite.height = 108.0f;
    if (containsToken(lowerName, "tree"))
    {
        sprite.halfWidth = 64.0f;
        sprite.height = 196.0f;
    }

    const Vec3 delta = sprite.basePos - cameraPos;
    sprite.distanceSq = delta.lengthSquared();
    return sprite;
}

BillboardSprite makeIndoorSpawnBillboard(const formats::BLVSpawnPoint& spawn, const Vec3& cameraPos,
                                         [[maybe_unused]] const game::RuntimeConfig* config,
                                         const IndoorRenderer::MonsterSpriteLookup& monsterLookup,
                                         const formats::SpriteFrameTable* spriteFrameTable)
{
    BillboardSprite sprite;
    sprite.basePos = {static_cast<float>(spawn.x), static_cast<float>(spawn.y),
                      static_cast<float>(spawn.z)};

    const float baseHeight = std::max(kMinBillboardHeight, static_cast<float>(spawn.radius) * 2.0f);
    sprite.height = std::clamp(baseHeight, 56.0f, 220.0f);
    sprite.halfWidth = std::max(kMinBillboardHalfWidth, sprite.height * 0.38f);

    if (monsterLookup)
    {
        std::string baseName = monsterLookup(spawn.objectType);
        if (!baseName.empty())
        {
            // Implement 8-directional facing index based on time and camera angle
            uint32_t ticks = SDL_GetTicks();
            // Slowly spin stationary spawns for idle animation effect
            int facing = (ticks / 10) % 2048;

            // Camera relative angle
            float dx = cameraPos.x - sprite.basePos.x;
            float dz = cameraPos.z - sprite.basePos.z;
            float camAngle = std::atan2(dx, dz);

            // Convert to MM7 angle (0 to 2047)
            int camFacing = static_cast<int>((camAngle + M_PI) * 1024.0f / M_PI) % 2048;
            if (camFacing < 0)
                camFacing += 2048;

            // Difference mapped to 8 directions
            int directionIndex = ((facing - camFacing + 2048 + 128) >> 8) & 7;

            std::string frameName =
                std::format("{}{:02d}", baseName.substr(0, std::min<size_t>(baseName.length(), 6)),
                            directionIndex + 1);

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
            // Calculate 8-directional facing index based on time and angle
            // facing is probably 0-2047, but for now we'll just use a time offset for random
            // rotation
            uint32_t ticks = SDL_GetTicks();
            uint32_t animOffset = (ticks / 100) % 8; // Change frame every 100ms

            // Assuming static spawns just face forward for now if we don't have their rotation
            int directionIndex = animOffset; // fallback random rotation

            // Limit to max 11 chars + 2 digit suffix if needed, but std::format handles it
            sprite.textureName =
                std::format("{}{:02d}", baseName.substr(0, std::min<size_t>(baseName.length(), 6)),
                            directionIndex + 1); // frames are often 1-indexed? Wait, MM7 uses 00-07
                                                 // for directions maybe? Let's try 01
        }
    }

    const int group = std::max(0, static_cast<int>(spawn.group));
    const int seed = (static_cast<int>(spawn.objectType) * 131) ^
                     (static_cast<int>(spawn.objectIndex) * 73) ^ (group * 17);
    const float r = 0.45f + static_cast<float>((seed >> 0) & 0x7) * 0.06f;
    const float g = 0.40f + static_cast<float>((seed >> 3) & 0x7) * 0.06f;
    const float b = 0.35f + static_cast<float>((seed >> 6) & 0x7) * 0.06f;
    sprite.color = {std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f),
                    0.74f};

    const Vec3 delta = sprite.basePos - cameraPos;
    sprite.distanceSq = delta.lengthSquared();
    return sprite;
}
} // namespace

IndoorRenderer::IndoorRenderer(SDLRenderer& renderer, util::ILogger& logger)
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
            "IndoorRenderer: No SDL_GPUDevice available. Falling back to software projection.");
    }
}

IndoorRenderer::~IndoorRenderer()
{
    if (gpuDevice)
    {
        if (indoorPipeline)
            SDL_ReleaseGPUGraphicsPipeline(gpuDevice, indoorPipeline);
        if (vertexShader)
            SDL_ReleaseGPUShader(gpuDevice, vertexShader);
        if (fragmentShader)
            SDL_ReleaseGPUShader(gpuDevice, fragmentShader);
        if (indoorVertexBuffer)
            SDL_ReleaseGPUBuffer(gpuDevice, indoorVertexBuffer);
        if (indoorIndexBuffer)
            SDL_ReleaseGPUBuffer(gpuDevice, indoorIndexBuffer);
        if (defaultSampler)
            SDL_ReleaseGPUSampler(gpuDevice, defaultSampler);
    }
}

void IndoorRenderer::setTextureLookup(TextureLookup lookup)
{
    textureLookup = std::move(lookup);
}

void IndoorRenderer::setMonsterSpriteLookup(MonsterSpriteLookup lookup)
{
    monsterSpriteLookup = std::move(lookup);
}

void IndoorRenderer::setSpriteFrameTable(const formats::SpriteFrameTable* table)
{
    spriteFrameTable = table;
}

void IndoorRenderer::render(const engine::MapScene& scene, const Camera& camera,
                            const runeharbor::game::RuntimeConfig* runtimeConfig,
                            const std::unordered_set<uint16_t>* visibleSectors,
                            SDL_GPUTexture* colorTex, SDL_GPUTexture* depthTex,
                            SDL_Texture* blitTex)
{
    const auto& blvData = scene.getBLVData();
    if (blvData.vertices.empty() || blvData.faces.empty())
    {
        return;
    }

    if (gpuInitialized && indoorIndexCount == 0)
    {
        buildGPUIndoor(blvData);
    }

    bool gpuWallsDrawn = false;
    if (gpuInitialized && indoorIndexCount > 0 && colorTex && depthTex)
    {
        // Flush the 2D renderer to ensure any pending texture uploads are processed
        SDL_FlushRenderer(renderer.getSDLRenderer());

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
        if (cmdBuf)
        {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = colorTex;
            colorTarget.clear_color = {0.0f, 0.0f, 0.0f, 0.0f}; // Transparent
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPUDepthStencilTargetInfo depthTarget = {};
            depthTarget.texture = depthTex;
            depthTarget.clear_depth = 1.0f;
            depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
            depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            depthTarget.cycle = false;

            SDL_GPURenderPass* renderPass =
                SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, &depthTarget);

            renderIndoorGPU(blvData, camera, runtimeConfig, visibleSectors, cmdBuf, renderPass);

            SDL_EndGPURenderPass(renderPass);
            SDL_SubmitGPUCommandBuffer(cmdBuf);

            if (blitTex)
            {
                SDL_SetTextureBlendMode(blitTex, SDL_BLENDMODE_BLEND);
                float w, h;
                SDL_GetTextureSize(blitTex, &w, &h);
                renderer.renderTexture(blitTex, 0, 0, static_cast<int>(w), static_cast<int>(h));
            }
            gpuWallsDrawn = true;
        }
    }

    const auto& viewProjection = camera.getViewProjectionMatrix();
    const Vec3 cameraPos = camera.getPosition();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());
    const bool animateWater = (runtimeConfig == nullptr) || !runtimeConfig->noWavyWater;
    const float waterTimeSeconds = static_cast<float>(SDL_GetTicks()) * 0.001f;

    std::unordered_set<uint16_t> localVisibleSectors;
    if (!visibleSectors)
    {
        PortalVisibility portalVisibility;
        localVisibleSectors = portalVisibility.computeVisibleSectors(blvData, cameraPos, 8);
        visibleSectors = &localVisibleSectors;
    }

    if (!blvData.sectors.empty() && visibleSectors->empty())
    {
        // Camera sector is outside known indoor portal graph; skip indoor face rendering.
        return;
    }

    // Build sorted face indices for painter's algorithm (back-to-front).
    enum class RenderOpType
    {
        Face,
        Billboard
    };

    struct RenderOp
    {
        RenderOpType type;
        float distanceSq;
        uint32_t index;            // For Face
        float cx, cy, cz;          // For Face
        BillboardSprite billboard; // For Billboard
    };

    std::vector<RenderOp> renderOps;
    renderOps.reserve(blvData.faces.size() + blvData.decorations.size() + blvData.spawns.size());

    for (uint32_t i = 0; i < blvData.faces.size(); i++)
    {
        const auto& face = blvData.faces[i];
        if (face.numVertices < 3 || face.vertexIndices.empty())
        {
            continue;
        }

        // Skip invisible faces
        if (face.isInvisible())
        {
            continue;
        }

        // Portal visibility culling by sector graph.
        if (!visibleSectors->empty() && !visibleSectors->contains(face.sectorId))
        {
            continue;
        }

        // Compute face centroid for backface culling and depth sort
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        uint32_t validVerts = 0;
        for (uint8_t j = 0; j < face.numVertices && j < face.vertexIndices.size(); j++)
        {
            uint16_t vi = face.vertexIndices[j];
            if (vi < blvData.vertices.size())
            {
                const auto& v = blvData.vertices[vi];
                cx += static_cast<float>(v.x);
                cy += static_cast<float>(v.y);
                cz += static_cast<float>(v.z);
                validVerts++;
            }
        }
        if (validVerts == 0)
        {
            continue;
        }
        float inv = 1.0f / static_cast<float>(validVerts);
        cx *= inv;
        cy *= inv;
        cz *= inv;

        // Backface culling: dot(faceNormal, centroid - cameraPos)
        float viewDirX = cx - cameraPos.x;
        float viewDirY = cy - cameraPos.y;
        float viewDirZ = cz - cameraPos.z;

        float dot = face.normalFX * viewDirX + face.normalFY * viewDirY + face.normalFZ * viewDirZ;
        if (dot > 0.0f)
        {
            continue;
        }

        float dist = viewDirX * viewDirX + viewDirY * viewDirY + viewDirZ * viewDirZ;
        RenderOp op;
        op.type = RenderOpType::Face;
        op.distanceSq = dist;
        op.index = i;
        op.cx = cx;
        op.cy = cy;
        op.cz = cz;
        renderOps.push_back(std::move(op));
    }

    for (const auto& decoration : blvData.decorations)
    {
        if (runtimeConfig && runtimeConfig->noDecorations)
            break;
        if (decoration.hidden)
            continue;
        if (decoration.name.empty())
            continue;

        BillboardSprite sprite =
            makeIndoorDecorationBillboard(decoration, cameraPos, spriteFrameTable, SDL_GetTicks());
        if (sprite.distanceSq > 100000000.0f)
            continue;

        RenderOp op;
        op.type = RenderOpType::Billboard;
        op.distanceSq = sprite.distanceSq;
        op.billboard = std::move(sprite);
        renderOps.push_back(std::move(op));
    }

    for (const auto& spawn : blvData.spawns)
    {
        if (runtimeConfig && runtimeConfig->noMonsters)
            break;
        if (spawn.objectType == 0)
            continue;

        BillboardSprite sprite = makeIndoorSpawnBillboard(spawn, cameraPos, runtimeConfig,
                                                          monsterSpriteLookup, spriteFrameTable);
        if (sprite.distanceSq > 100000000.0f)
            continue;

        RenderOp op;
        op.type = RenderOpType::Billboard;
        op.distanceSq = sprite.distanceSq;
        op.billboard = std::move(sprite);
        renderOps.push_back(std::move(op));
    }

    std::sort(renderOps.begin(), renderOps.end(),
              [](const RenderOp& a, const RenderOp& b) { return a.distanceSq > b.distanceSq; });

    ClipVertex polyIn[MAX_CLIP_VERTS];
    ClipVertex polyOut[MAX_CLIP_VERTS];

    // Render operations
    for (const auto& op : renderOps)
    {
        if (op.type == RenderOpType::Face)
        {
            if (gpuWallsDrawn)
                continue; // GPU already drew the walls

            const auto& face = blvData.faces[op.index];
            const SDL_FColor faceColor = litIndoorFaceColor(blvData, face, op.cx, op.cy, op.cz,
                                                            stationaryLights_, mobileLights_);

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
                if (vi >= blvData.vertices.size())
                {
                    break;
                }
                if (polyCount >= MAX_CLIP_VERTS)
                {
                    break;
                }

                const auto& vertex = blvData.vertices[vi];
                Vec3 worldPos = {static_cast<float>(vertex.x), static_cast<float>(vertex.y),
                                 static_cast<float>(vertex.z)};

                Vec4 clip = viewProjection * Vec4(worldPos, 1.0f);
                if (clip.w >= CLIP_NEAR_EPSILON)
                {
                    allBehind = false;
                }

                float u = 0.0f, uv = 0.0f;
                if (i < face.uCoords.size() && i < face.vCoords.size())
                {
                    u = static_cast<float>(face.uCoords[i]) / 256.0f;
                    uv = static_cast<float>(face.vCoords[i]) / 256.0f;
                    if (face.isWater() && animateWater)
                    {
                        const float wave =
                            std::sin(waterWavePhase(worldPos.x, worldPos.z, waterTimeSeconds));
                        u += waterTimeSeconds * 0.09f + wave * 0.03f;
                        uv += waterTimeSeconds * 0.06f + wave * 0.02f;
                    }
                }

                polyIn[polyCount++] = {clip, faceColor, u, uv};
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
            std::vector<SDL_Vertex> vertices;
            vertices.reserve(tessellatedVerts.size());
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
                vertices.push_back(sv);
            }

            if (anyFailed || vertices.size() < 3)
            {
                continue;
            }

            std::vector<int> indices;
            indices.reserve(vertices.size());
            for (size_t i = 0; i < vertices.size(); i++)
            {
                indices.push_back(static_cast<int>(i));
            }

            // Look up texture if available
            SDL_Texture* texture = nullptr;
            if (textureLookup && !face.textureName.empty())
            {
                texture = textureLookup(face.textureName);
            }

            // Render triangulated polygon (base pass)
            SDL_SetRenderDrawBlendMode(renderer.getSDLRenderer(), SDL_BLENDMODE_BLEND);
            SDL_RenderGeometry(renderer.getSDLRenderer(), texture, vertices.data(),
                               static_cast<int>(vertices.size()), indices.data(),
                               static_cast<int>(indices.size()));

            // Additive light polygon pass
            if ((face.attributes & kNoLightFaceBit) == 0 && texture != nullptr)
            {
                // We re-render the same geometry but with SDL_BLENDMODE_ADD
                // The color of the vertices for this pass should be purely the dynamic light
                // contribution We've already computed it in litIndoorFaceColor, but let's
                // recalculate just the dynamic part here for the additive layer Actually, an easier
                // way is to just apply the dynamic light as an additive overlay We need to
                // re-generate the vertex colors for the additive pass. We'll calculate the dynamic
                // light contribution (without ambient) and use it.
                std::vector<SDL_Vertex> lightVertices = vertices;
                bool hasLight = false;
                for (size_t v_idx = 0; v_idx < lightVertices.size(); ++v_idx)
                {
                    // Re-project the 2D screen coordinate back to world? No, we don't have the
                    // original world pos easily here. Oh wait, we used op.cx, op.cy, op.cz for the
                    // face center lighting. We can just use the dynamic light computed for the face
                    // center and apply it to all vertices. To do it properly per-vertex, we'd need
                    // worldPos for each vertex. Let's just use the face center dynamic light for
                    // now.
                    SDL_FColor dynLight = dynamicLightContribution(
                        blvData, face, op.cx, op.cy, op.cz, stationaryLights_, mobileLights_);
                    if (dynLight.r > 0 || dynLight.g > 0 || dynLight.b > 0)
                    {
                        hasLight = true;
                    }
                    lightVertices[v_idx].color = {
                        dynLight.r, dynLight.g, dynLight.b,
                        1.0f}; // Additive needs alpha=1 for the texture blend if any
                }

                if (hasLight)
                {
                    SDL_SetTextureColorMod(texture, 255, 255, 255); // Reset any texture mod
                    SDL_SetRenderDrawBlendMode(renderer.getSDLRenderer(), SDL_BLENDMODE_ADD);
                    SDL_RenderGeometry(renderer.getSDLRenderer(), texture, lightVertices.data(),
                                       static_cast<int>(lightVertices.size()), indices.data(),
                                       static_cast<int>(indices.size()));
                }
            }
        }
        else if (op.type == RenderOpType::Billboard)
        {
            const auto& sprite = op.billboard;

            SDL_SetRenderDrawBlendMode(renderer.getSDLRenderer(), SDL_BLENDMODE_BLEND);

            Vec3 toCamera = cameraPos - sprite.basePos;
            toCamera.y = 0.0f;
            if (toCamera.lengthSquared() < 0.001f)
            {
                toCamera = Vec3::forward();
            }
            const Vec3 forward = toCamera.normalized();
            const Vec3 worldUp = Vec3::up();
            Vec3 right = worldUp.cross(forward);
            if (right.lengthSquared() < 0.001f)
            {
                right = Vec3::right();
            }
            right.normalize();

            SDL_Texture* texture = nullptr;
            float actualHalfWidth = sprite.halfWidth;
            float actualHeight = sprite.height;

            if (textureLookup && !sprite.textureName.empty())
            {
                texture = textureLookup(sprite.textureName);
                if (texture)
                {
                    float w, h;
                    if (SDL_GetTextureSize(texture, &w, &h))
                    {
                        const float scale = 0.6f;
                        actualHalfWidth = (w * scale) * 0.5f;
                        actualHeight = h * scale;
                    }
                }
            }

            Vec3 drawPos = sprite.basePos;
            SDL_FColor drawColor = sprite.color;
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
}

void IndoorRenderer::initGPUPipeline()
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
    pipelineInfo.rasterizer_state.cull_mode =
        SDL_GPU_CULLMODE_NONE; // Indoor BSP culling handles visibility, but faces are drawn direct.
                               // Let's do NONE for safety until ported fully.
    pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    // Depth Stencil state
    pipelineInfo.depth_stencil_state.enable_depth_test = true;
    pipelineInfo.depth_stencil_state.enable_depth_write = true;
    pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    // Target state
    pipelineInfo.target_info.num_color_targets = 1;
    SDL_GPUColorTargetDescription targetDesc = {};
    targetDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    // Enable blending for transparency
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

    indoorPipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &pipelineInfo);
    if (!indoorPipeline)
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

void IndoorRenderer::invalidateGPUCache()
{
    indoorIndexCount = 0;
    indoorDrawCalls.clear();
    if (gpuDevice)
    {
        if (indoorVertexBuffer)
        {
            SDL_ReleaseGPUBuffer(gpuDevice, indoorVertexBuffer);
            indoorVertexBuffer = nullptr;
        }
        if (indoorIndexBuffer)
        {
            SDL_ReleaseGPUBuffer(gpuDevice, indoorIndexBuffer);
            indoorIndexBuffer = nullptr;
        }
    }
}

void IndoorRenderer::buildGPUIndoor(const formats::BLVMapData& blvData)
{
    if (!gpuDevice || blvData.faces.empty())
        return;

    indoorDrawCalls.clear();

    // Group faces by (texture, sector), skipping invisible and portal faces
    struct TextureSectorKey
    {
        std::string textureName;
        uint16_t sectorId;
        bool operator==(const TextureSectorKey&) const = default;
    };
    struct KeyHash
    {
        size_t operator()(const TextureSectorKey& k) const
        {
            return std::hash<std::string>{}(k.textureName) ^
                   (std::hash<uint16_t>{}(k.sectorId) << 16);
        }
    };
    std::unordered_map<TextureSectorKey, std::vector<uint32_t>, KeyHash> groupedFaces;
    for (uint32_t i = 0; i < blvData.faces.size(); ++i)
    {
        const auto& face = blvData.faces[i];
        if (face.isInvisible() || face.isPortal())
            continue;
        TextureSectorKey key{face.textureName, face.sectorId};
        groupedFaces[key].push_back(i);
    }

    std::vector<GPUVertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto& [key, faceIndices] : groupedFaces)
    {
        uint32_t indexStart = static_cast<uint32_t>(indices.size());

        for (uint32_t fIdx : faceIndices)
        {
            const auto& face = blvData.faces[fIdx];
            if (face.vertexIndices.size() < 3)
                continue;

            uint32_t startIdx = static_cast<uint32_t>(vertices.size());

            // Compute face center for lighting
            float cx = 0.0f, cy = 0.0f, cz = 0.0f;
            for (uint16_t vIdx : face.vertexIndices)
            {
                if (vIdx < blvData.vertices.size())
                {
                    cx += static_cast<float>(blvData.vertices[vIdx].x);
                    cy += static_cast<float>(blvData.vertices[vIdx].y);
                    cz += static_cast<float>(blvData.vertices[vIdx].z);
                }
            }
            cx /= face.vertexIndices.size();
            cy /= face.vertexIndices.size();
            cz /= face.vertexIndices.size();

            // Apply lighting to get face color
            SDL_FColor faceColor =
                litIndoorFaceColor(blvData, face, cx, cy, cz, stationaryLights_, mobileLights_);

            for (size_t i = 0; i < face.vertexIndices.size(); ++i)
            {
                uint16_t vIdx = face.vertexIndices[i];
                if (vIdx >= blvData.vertices.size())
                    continue;

                const auto& vPos = blvData.vertices[vIdx];

                float u = 0.0f, v = 0.0f;
                if (i < face.uCoords.size() && i < face.vCoords.size())
                {
                    u = static_cast<float>(face.uCoords[i]) / 256.0f;
                    v = static_cast<float>(face.vCoords[i]) / 256.0f;
                }

                GPUVertex vertex = {static_cast<float>(vPos.x),
                                    static_cast<float>(vPos.y),
                                    static_cast<float>(vPos.z),
                                    faceColor.r,
                                    faceColor.g,
                                    faceColor.b,
                                    faceColor.a,
                                    u,
                                    v};
                vertices.push_back(vertex);
            }

            // Triangulate n-gon (triangle fan)
            for (size_t i = 1; i < face.vertexIndices.size() - 1; ++i)
            {
                indices.push_back(startIdx);
                indices.push_back(startIdx + i);
                indices.push_back(startIdx + i + 1);
            }
        }

        uint32_t indexCount = static_cast<uint32_t>(indices.size()) - indexStart;
        if (indexCount > 0)
        {
            indoorDrawCalls.push_back({key.textureName, key.sectorId, indexStart, indexCount});
        }
    }

    indoorIndexCount = static_cast<uint32_t>(indices.size());

    if (indoorIndexCount == 0)
        return;

    if (indoorVertexBuffer)
        SDL_ReleaseGPUBuffer(gpuDevice, indoorVertexBuffer);
    if (indoorIndexBuffer)
        SDL_ReleaseGPUBuffer(gpuDevice, indoorIndexBuffer);

    SDL_GPUBufferCreateInfo vboInfo = {};
    vboInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vboInfo.size = vertices.size() * sizeof(GPUVertex);
    indoorVertexBuffer = SDL_CreateGPUBuffer(gpuDevice, &vboInfo);

    SDL_GPUBufferCreateInfo iboInfo = {};
    iboInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    iboInfo.size = indices.size() * sizeof(uint32_t);
    indoorIndexBuffer = SDL_CreateGPUBuffer(gpuDevice, &iboInfo);

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
    SDL_GPUBufferRegion dstVbo = {indoorVertexBuffer, 0, vboInfo.size};
    SDL_UploadToGPUBuffer(copyPass, &srcVbo, &dstVbo, false);

    SDL_GPUTransferBufferLocation srcIbo = {transferBuffer, static_cast<Uint32>(vboInfo.size)};
    SDL_GPUBufferRegion dstIbo = {indoorIndexBuffer, 0, iboInfo.size};
    SDL_UploadToGPUBuffer(copyPass, &srcIbo, &dstIbo, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
}

void IndoorRenderer::renderIndoorGPU(const formats::BLVMapData& blvData, const Camera& camera,
                                     const runeharbor::game::RuntimeConfig* runtimeConfig,
                                     const std::unordered_set<uint16_t>* visibleSectors,
                                     SDL_GPUCommandBuffer* cmdBuf, SDL_GPURenderPass* renderPass)
{
    (void)blvData;
    (void)runtimeConfig;

    if (!gpuInitialized || !indoorVertexBuffer || !indoorIndexBuffer || !indoorPipeline ||
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
    SDL_BindGPUGraphicsPipeline(renderPass, indoorPipeline);

    // Bind Buffers
    SDL_GPUBufferBinding vboBinding = {indoorVertexBuffer, 0};
    SDL_BindGPUVertexBuffers(renderPass, 0, &vboBinding, 1);

    SDL_GPUBufferBinding iboBinding = {indoorIndexBuffer, 0};
    SDL_BindGPUIndexBuffer(renderPass, &iboBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // Upload Uniforms (View/Projection Matrix)
    Mat4 vp = camera.getViewProjectionMatrix();
    SDL_PushGPUVertexUniformData(cmdBuf, 0, vp.m.data(), sizeof(float) * 16);

    // Push fragment uniform (no night blend for indoor)
    float fragUniforms[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    SDL_PushGPUFragmentUniformData(cmdBuf, 0, fragUniforms, sizeof(fragUniforms));

    for (const auto& drawCall : indoorDrawCalls)
    {
        // Sector visibility culling — skip draw calls for sectors not visible
        if (visibleSectors && !visibleSectors->empty() &&
            !visibleSectors->contains(drawCall.sectorId))
        {
            continue;
        }

        SDL_GPUTexture* gpuTex = nullptr;
        if (textureLookup && !drawCall.textureName.empty())
        {
            if (SDL_Texture* tex = textureLookup(drawCall.textureName))
            {
                SDL_PropertiesID texProps = SDL_GetTextureProperties(tex);
                gpuTex = (SDL_GPUTexture*)SDL_GetPointerProperty(
                    texProps, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr);
            }
        }

        if (gpuTex)
        {
            SDL_GPUTextureSamplerBinding samplerBinding = {gpuTex, defaultSampler};
            SDL_BindGPUFragmentSamplers(renderPass, 0, &samplerBinding, 1);
            SDL_DrawGPUIndexedPrimitives(renderPass, drawCall.indexCount, 1, drawCall.indexStart, 0,
                                         0);
        }
    }
}

} // namespace runeharbor::graphics
