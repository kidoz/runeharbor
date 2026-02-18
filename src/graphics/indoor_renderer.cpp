// SPDX-License-Identifier: MIT
#include "indoor_renderer.hpp"

#include <algorithm>

#include <cmath>

namespace runeharbor::graphics
{

IndoorRenderer::IndoorRenderer(SDLRenderer& renderer, util::ILogger& logger)
    : renderer(renderer), logger(logger)
{
}

IndoorRenderer::~IndoorRenderer() = default;

void IndoorRenderer::setTextureLookup(TextureLookup lookup)
{
    textureLookup = std::move(lookup);
}

void IndoorRenderer::render(const engine::MapScene& scene, const Camera& camera)
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

    // Build sorted face indices for painter's algorithm (back-to-front).
    // Compute centroid distance to camera for each face.
    struct FaceSort
    {
        uint32_t index;
        float distance;
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
        // If positive, the face points away from camera
        float viewDirX = cx - cameraPos.x;
        float viewDirY = cy - cameraPos.y;
        float viewDirZ = cz - cameraPos.z;

        float dot = face.normalFX * viewDirX + face.normalFY * viewDirY + face.normalFZ * viewDirZ;
        if (dot > 0.0f)
        {
            continue; // Face points away from camera
        }

        // Distance for painter's sort
        float dist = viewDirX * viewDirX + viewDirY * viewDirY + viewDirZ * viewDirZ;
        sortedFaces.push_back({i, dist});
    }

    // Sort back-to-front (far faces first)
    std::sort(sortedFaces.begin(), sortedFaces.end(),
              [](const FaceSort& a, const FaceSort& b) { return a.distance > b.distance; });

    // Render faces
    for (const auto& fs : sortedFaces)
    {
        const auto& face = blvData.faces[fs.index];

        // Build SDL vertices
        std::vector<SDL_Vertex> vertices;
        vertices.reserve(face.numVertices);
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

            const auto& vertex = blvData.vertices[vi];
            Vec3 worldPos = {static_cast<float>(vertex.x), static_cast<float>(vertex.y),
                             static_cast<float>(vertex.z)};

            Vec3 clipPos = viewProjection.transformPoint(worldPos);

            if (clipPos.z > 0.01f)
            {
                allBehind = false;
            }

            // Perspective divide
            float invZ = (std::abs(clipPos.z) > 0.001f) ? 1.0f / clipPos.z : 1000.0f;
            float sx = (clipPos.x * invZ * 0.5f + 0.5f) * vpW;
            float sy = (0.5f - clipPos.y * invZ * 0.5f) * vpH;

            SDL_Vertex sdlVert;
            sdlVert.position = {sx, sy};

            // Surface-type coloring (used as fallback or vertex color)
            SDL_FColor color;
            if (face.isFloor())
            {
                color = {0.45f, 0.42f, 0.38f, 1.0f}; // Stone gray
            }
            else if (face.isCeiling())
            {
                color = {0.30f, 0.28f, 0.25f, 1.0f}; // Dark gray
            }
            else if (face.isWater())
            {
                color = {0.15f, 0.30f, 0.55f, 0.8f}; // Blue translucent
            }
            else if (face.isLava())
            {
                color = {0.70f, 0.20f, 0.05f, 1.0f}; // Orange-red
            }
            else if (face.isPortal())
            {
                color = {0.10f, 0.10f, 0.10f, 0.3f}; // Nearly transparent
            }
            else
            {
                color = {0.55f, 0.50f, 0.42f, 1.0f}; // Wall beige
            }
            sdlVert.color = color;

            // Texture coordinates
            if (i < face.uCoords.size() && i < face.vCoords.size())
            {
                sdlVert.tex_coord = {static_cast<float>(face.uCoords[i]) / 256.0f,
                                     static_cast<float>(face.vCoords[i]) / 256.0f};
            }
            else
            {
                sdlVert.tex_coord = {0.0f, 0.0f};
            }

            vertices.push_back(sdlVert);
        }

        if (allBehind || vertices.size() < 3)
        {
            continue;
        }

        // Fan triangulation: generate indices for (n-2) triangles
        std::vector<int> indices;
        indices.reserve((vertices.size() - 2) * 3);
        for (size_t i = 1; i + 1 < vertices.size(); i++)
        {
            indices.push_back(0);
            indices.push_back(static_cast<int>(i));
            indices.push_back(static_cast<int>(i + 1));
        }

        // Look up texture if a lookup function is available
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
}

} // namespace runeharbor::graphics
