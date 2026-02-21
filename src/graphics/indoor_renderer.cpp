// SPDX-License-Identifier: MIT
#include "indoor_renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <string_view>

#include <cctype>
#include <cmath>

#include "../game/game_world.hpp"
#include "clip_utils.hpp"
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
                                    const formats::ParsedFace& face, float x, float y, float z)
{
    SDL_FColor add = {0.0f, 0.0f, 0.0f, 0.0f};
    if ((face.attributes & kNoLightFaceBit) != 0)
    {
        return add;
    }

    auto addLight = [&](const formats::BLVLight& light)
    {
        const float radius = std::max(1.0f, static_cast<float>(light.radius));
        const float dx = static_cast<float>(light.x) - x;
        const float dy = static_cast<float>(light.y) - y;
        const float dz = static_cast<float>(light.z) - z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist >= radius)
        {
            return;
        }

        const float falloff = 1.0f - dist / radius;
        const float brightness =
            std::clamp(static_cast<float>(light.brightness) / 96.0f, 0.10f, 1.0f);
        const float intensity = falloff * brightness;
        add.r += (static_cast<float>(light.red) / 255.0f) * intensity;
        add.g += (static_cast<float>(light.green) / 255.0f) * intensity;
        add.b += (static_cast<float>(light.blue) / 255.0f) * intensity;
    };

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
            addLight(blvData.lights[lightId]);
        }
    }

    if (!usedSectorLights)
    {
        for (const auto& light : blvData.lights)
        {
            addLight(light);
        }
    }

    add.r = std::clamp(add.r, 0.0f, 1.0f);
    add.g = std::clamp(add.g, 0.0f, 1.0f);
    add.b = std::clamp(add.b, 0.0f, 1.0f);
    return add;
}

SDL_FColor litIndoorFaceColor(const formats::BLVMapData& blvData, const formats::ParsedFace& face,
                              float x, float y, float z)
{
    SDL_FColor color = surfaceColor(face);
    if ((face.attributes & kNoLightFaceBit) != 0)
    {
        return color;
    }

    const float ambient = sectorAmbientScale(blvData, face);
    const SDL_FColor dynamic = dynamicLightContribution(blvData, face, x, y, z);
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
};

BillboardSprite makeIndoorDecorationBillboard(const formats::ParsedDecoration& decoration,
                                              const Vec3& cameraPos)
{
    BillboardSprite sprite;
    sprite.basePos = {static_cast<float>(decoration.x), static_cast<float>(decoration.y),
                      static_cast<float>(decoration.z)};
    sprite.textureName = decoration.name;

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

BillboardSprite makeIndoorSpawnBillboard(const formats::BLVSpawnPoint& spawn, const Vec3& cameraPos)
{
    BillboardSprite sprite;
    sprite.basePos = {static_cast<float>(spawn.x), static_cast<float>(spawn.y),
                      static_cast<float>(spawn.z)};

    const float baseHeight = std::max(kMinBillboardHeight, static_cast<float>(spawn.radius) * 2.0f);
    sprite.height = std::clamp(baseHeight, 56.0f, 220.0f);
    sprite.halfWidth = std::max(kMinBillboardHalfWidth, sprite.height * 0.38f);

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
}

IndoorRenderer::~IndoorRenderer() = default;

void IndoorRenderer::setTextureLookup(TextureLookup lookup)
{
    textureLookup = std::move(lookup);
}

void IndoorRenderer::render(const engine::MapScene& scene, const Camera& camera,
                            const runeharbor::game::RuntimeConfig* runtimeConfig,
                            const std::unordered_set<uint16_t>* visibleSectors)
{
    const auto& blvData = scene.getBLVData();
    if (blvData.vertices.empty() || blvData.faces.empty())
    {
        return;
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
    struct FaceSort
    {
        uint32_t index;
        float distance;
        float centerX;
        float centerY;
        float centerZ;
    };

    std::vector<FaceSort> sortedFaces;
    sortedFaces.reserve(blvData.faces.size());

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
        sortedFaces.push_back({i, dist, cx, cy, cz});
    }

    // Sort back-to-front (far faces first)
    std::sort(sortedFaces.begin(), sortedFaces.end(),
              [](const FaceSort& a, const FaceSort& b) { return a.distance > b.distance; });

    ClipVertex polyIn[MAX_CLIP_VERTS];
    ClipVertex polyOut[MAX_CLIP_VERTS];

    // Render faces
    for (const auto& fs : sortedFaces)
    {
        const auto& face = blvData.faces[fs.index];
        const SDL_FColor faceColor =
            litIndoorFaceColor(blvData, face, fs.centerX, fs.centerY, fs.centerZ);

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

        // Project to screen
        std::vector<SDL_Vertex> vertices;
        vertices.reserve(static_cast<size_t>(clippedCount));
        bool anyFailed = false;

        for (int i = 0; i < clippedCount; i++)
        {
            float sx, sy;
            if (!projectClipToScreen(polyOut[i].clip, vpW, vpH, sx, sy))
            {
                anyFailed = true;
                break;
            }
            SDL_Vertex sv;
            sv.position = {sx, sy};
            sv.color = polyOut[i].color;
            sv.tex_coord = {polyOut[i].u, polyOut[i].v};
            vertices.push_back(sv);
        }

        if (anyFailed || vertices.size() < 3)
        {
            continue;
        }

        // Fan triangulation
        std::vector<int> indices;
        indices.reserve((vertices.size() - 2) * 3);
        for (size_t i = 1; i + 1 < vertices.size(); i++)
        {
            indices.push_back(0);
            indices.push_back(static_cast<int>(i));
            indices.push_back(static_cast<int>(i + 1));
        }

        // Look up texture if available
        SDL_Texture* texture = nullptr;
        if (textureLookup && !face.textureName.empty())
        {
            texture = textureLookup(face.textureName);
        }

        // Render triangulated polygon
        SDL_RenderGeometry(renderer.getSDLRenderer(), texture, vertices.data(),
                           static_cast<int>(vertices.size()), indices.data(),
                           static_cast<int>(indices.size()));
    }

    // Sprite billboards (decorations and spawn markers), sorted back-to-front.
    std::vector<BillboardSprite> billboards;
    billboards.reserve(blvData.decorations.size() + blvData.spawns.size());

    for (const auto& decoration : blvData.decorations)
    {
        if (runtimeConfig && runtimeConfig->noDecorations)
        {
            break;
        }
        if (decoration.hidden)
        {
            continue;
        }
        if (decoration.name.empty())
        {
            continue;
        }
        billboards.push_back(makeIndoorDecorationBillboard(decoration, cameraPos));
    }

    for (const auto& spawn : blvData.spawns)
    {
        if (runtimeConfig && runtimeConfig->noMonsters)
        {
            break;
        }
        if (spawn.objectType == 0)
        {
            continue;
        }
        billboards.push_back(makeIndoorSpawnBillboard(spawn, cameraPos));
    }

    std::sort(billboards.begin(), billboards.end(),
              [](const BillboardSprite& a, const BillboardSprite& b)
              { return a.distanceSq > b.distanceSq; });

    if (!billboards.empty())
    {
        SDL_SetRenderDrawBlendMode(renderer.getSDLRenderer(), SDL_BLENDMODE_BLEND);
    }

    const Vec3 worldUp = Vec3::up();
    for (const auto& sprite : billboards)
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

        const Vec3 bottomLeft = sprite.basePos - right * sprite.halfWidth;
        const Vec3 bottomRight = sprite.basePos + right * sprite.halfWidth;
        const Vec3 topLeft = bottomLeft + worldUp * sprite.height;
        const Vec3 topRight = bottomRight + worldUp * sprite.height;

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
            vertex.color = sprite.color;
        }
        vertices[0].tex_coord = {0.0f, 1.0f};
        vertices[1].tex_coord = {1.0f, 1.0f};
        vertices[2].tex_coord = {0.0f, 0.0f};
        vertices[3].tex_coord = {1.0f, 0.0f};

        SDL_Texture* texture = nullptr;
        if (textureLookup && !sprite.textureName.empty())
        {
            texture = textureLookup(sprite.textureName);
        }
        constexpr int indices[6] = {0, 1, 2, 2, 1, 3};
        SDL_RenderGeometry(renderer.getSDLRenderer(), texture, vertices, 4, indices, 6);
    }
}

} // namespace runeharbor::graphics
