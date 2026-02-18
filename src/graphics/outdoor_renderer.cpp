// SPDX-License-Identifier: MIT
#include "outdoor_renderer.hpp"

#include <algorithm>

#include <cmath>

namespace runeharbor::graphics
{

OutdoorRenderer::OutdoorRenderer(SDLRenderer& renderer, util::ILogger& logger)
    : renderer(renderer), logger(logger)
{
}

OutdoorRenderer::~OutdoorRenderer() = default;

void OutdoorRenderer::setTextureLookup(TextureLookup lookup)
{
    textureLookup = std::move(lookup);
}

void OutdoorRenderer::render(const engine::MapScene& scene, const Camera& camera)
{
    const auto& odmData = scene.getODMData();
    if (odmData.heightmap.empty())
    {
        return;
    }

    renderTerrain(odmData, camera);
    renderBuildings(odmData, camera);
}

void OutdoorRenderer::renderTerrain(const formats::ODMMapData& odmData, const Camera& camera)
{
    const auto& viewProjection = camera.getViewProjectionMatrix();
    const float vpW = static_cast<float>(renderer.getViewportWidth());
    const float vpH = static_cast<float>(renderer.getViewportHeight());
    const Vec3 cameraPos = camera.getPosition();

    constexpr int SIZE = formats::ODMMapData::TERRAIN_SIZE;
    constexpr float CELL_SIZE = 512.0f;
    constexpr float HALF = SIZE / 2.0f;

    // Frustum culling distance: skip cells too far from camera
    constexpr float MAX_RENDER_DIST_SQ = 40000.0f * 40000.0f;

    // Helper: get world position for grid cell
    auto worldPos = [&](int gx, int gy) -> Vec3
    {
        float wx = (static_cast<float>(gx) - HALF) * CELL_SIZE;
        float wy = (static_cast<float>(gy) - HALF) * CELL_SIZE;
        float wz = 0.0f;

        size_t idx = static_cast<size_t>(gy * SIZE + gx);
        if (idx < odmData.heightmap.size())
        {
            wz = static_cast<float>(odmData.heightmap[idx].height);
        }

        return {wx, wy, wz};
    };

    // Helper: project world point to screen
    auto project = [&](Vec3 wp, float& sx, float& sy, bool& behind) -> bool
    {
        Vec3 clipPos = viewProjection.transformPoint(wp);
        behind = (clipPos.z <= 0.01f);
        float invZ = (std::abs(clipPos.z) > 0.001f) ? 1.0f / clipPos.z : 1000.0f;
        sx = (clipPos.x * invZ * 0.5f + 0.5f) * vpW;
        sy = (0.5f - clipPos.y * invZ * 0.5f) * vpH;
        return !behind;
    };

    // Height-based vertex coloring
    auto heightColor = [](float height) -> SDL_FColor
    {
        // Map height to a green/brown gradient
        float t = std::clamp(height / 4000.0f, 0.0f, 1.0f);
        float r = 0.30f + t * 0.35f; // Brown at high altitude
        float g = 0.45f - t * 0.15f; // Green at low altitude
        float b = 0.20f + t * 0.05f;
        return {r, g, b, 1.0f};
    };

    // Render terrain as quads (2 triangles each)
    // Step by 1 cell; skip cells far from camera
    for (int gy = 0; gy < SIZE - 1; gy++)
    {
        for (int gx = 0; gx < SIZE - 1; gx++)
        {
            Vec3 p00 = worldPos(gx, gy);
            Vec3 p10 = worldPos(gx + 1, gy);
            Vec3 p01 = worldPos(gx, gy + 1);
            Vec3 p11 = worldPos(gx + 1, gy + 1);

            // Center of quad for distance check
            float cx = (p00.x + p11.x) * 0.5f;
            float cy = (p00.y + p11.y) * 0.5f;
            float cz = (p00.z + p11.z) * 0.5f;
            float dx = cx - cameraPos.x;
            float dy = cy - cameraPos.y;
            float dz = cz - cameraPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > MAX_RENDER_DIST_SQ)
            {
                continue;
            }

            // Project all 4 corners
            float sx00, sy00, sx10, sy10, sx01, sy01, sx11, sy11;
            bool b00, b10, b01, b11;
            project(p00, sx00, sy00, b00);
            project(p10, sx10, sy10, b10);
            project(p01, sx01, sy01, b01);
            project(p11, sx11, sy11, b11);

            // Triangle 1: p00, p10, p01
            if (!(b00 && b10 && b01))
            {
                SDL_FColor c00 = heightColor(p00.z);
                SDL_FColor c10 = heightColor(p10.z);
                SDL_FColor c01 = heightColor(p01.z);

                SDL_Vertex verts[3] = {
                    {{sx00, sy00}, c00, {0, 0}},
                    {{sx10, sy10}, c10, {1, 0}},
                    {{sx01, sy01}, c01, {0, 1}},
                };
                SDL_RenderGeometry(renderer.getSDLRenderer(), nullptr, verts, 3, nullptr, 0);
            }

            // Triangle 2: p10, p11, p01
            if (!(b10 && b11 && b01))
            {
                SDL_FColor c10 = heightColor(p10.z);
                SDL_FColor c11 = heightColor(p11.z);
                SDL_FColor c01 = heightColor(p01.z);

                SDL_Vertex verts[3] = {
                    {{sx10, sy10}, c10, {1, 0}},
                    {{sx11, sy11}, c11, {1, 1}},
                    {{sx01, sy01}, c01, {0, 1}},
                };
                SDL_RenderGeometry(renderer.getSDLRenderer(), nullptr, verts, 3, nullptr, 0);
            }
        }
    }
}

void OutdoorRenderer::renderBuildings(const formats::ODMMapData& odmData, const Camera& camera)
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
                    cy += static_cast<float>(v.y);
                    cz += static_cast<float>(v.z);
                    valid++;
                }
            }
            if (valid == 0)
            {
                continue;
            }
            float inv = 1.0f / static_cast<float>(valid);
            cx = cx * inv + static_cast<float>(building.worldX);
            cy = cy * inv + static_cast<float>(building.worldY);
            cz = cz * inv + static_cast<float>(building.worldZ);

            // Backface culling
            float viewX = cx - cameraPos.x;
            float viewY = cy - cameraPos.y;
            float viewZ = cz - cameraPos.z;
            float dot = face.normalFX * viewX + face.normalFY * viewY + face.normalFZ * viewZ;
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

    // Render each face
    for (const auto& fr : sortedFaces)
    {
        const auto& building = *fr.building;
        const auto& face = building.faces[fr.faceIndex];

        std::vector<SDL_Vertex> verts;
        verts.reserve(face.numVertices);
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

            const auto& v = building.vertices[vi];
            Vec3 wp = {static_cast<float>(v.x) + static_cast<float>(building.worldX),
                       static_cast<float>(v.y) + static_cast<float>(building.worldY),
                       static_cast<float>(v.z) + static_cast<float>(building.worldZ)};

            Vec3 clipPos = viewProjection.transformPoint(wp);
            if (clipPos.z > 0.01f)
            {
                allBehind = false;
            }

            float invZ = (std::abs(clipPos.z) > 0.001f) ? 1.0f / clipPos.z : 1000.0f;
            float sx = (clipPos.x * invZ * 0.5f + 0.5f) * vpW;
            float sy = (0.5f - clipPos.y * invZ * 0.5f) * vpH;

            SDL_Vertex sv;
            sv.position = {sx, sy};

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
            sv.color = color;

            // Texture coordinates
            if (i < face.uCoords.size() && i < face.vCoords.size())
            {
                sv.tex_coord = {static_cast<float>(face.uCoords[i]) / 256.0f,
                                static_cast<float>(face.vCoords[i]) / 256.0f};
            }
            else
            {
                sv.tex_coord = {0.0f, 0.0f};
            }

            verts.push_back(sv);
        }

        if (allBehind || verts.size() < 3)
        {
            continue;
        }

        // Fan triangulation
        std::vector<int> indices;
        indices.reserve((verts.size() - 2) * 3);
        for (size_t i = 1; i + 1 < verts.size(); i++)
        {
            indices.push_back(0);
            indices.push_back(static_cast<int>(i));
            indices.push_back(static_cast<int>(i + 1));
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

} // namespace runeharbor::graphics
