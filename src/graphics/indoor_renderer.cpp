// SPDX-License-Identifier: MIT
#include "indoor_renderer.hpp"
#include <format>
#include "../util/ilogger.hpp"

namespace runeharbor::graphics
{

IndoorRenderer::IndoorRenderer(SDLRenderer& renderer, util::ILogger& logger)
    : renderer(renderer), logger(logger)
{
}

IndoorRenderer::~IndoorRenderer() = default;

void IndoorRenderer::render(const engine::MapScene& scene, const Camera& camera)
{
    const auto& blvData = scene.getBLVData();
    if (blvData.vertices.empty() || blvData.faces.empty())
    {
        return;
    }

    const auto& viewProjection = camera.getViewProjectionMatrix();

    logger.info(std::format("Rendering {} faces", blvData.faces.size()));

    for (const auto& face : blvData.faces)
    {
        if (face.numVertices < 3)
        {
            continue;
        }

        std::vector<SDL_Vertex> vertices;
        vertices.reserve(face.numVertices);

        for (size_t i = 0; i < face.numVertices; ++i)
        {
            const auto& vertex = blvData.vertices[face.vertexIndices[i]];
            Vec3 worldPos = {(float)vertex.x, (float)vertex.y, (float)vertex.z};

            // Transform to screen coordinates
            Vec3 screenPos = viewProjection.transformPoint(worldPos);

            if (screenPos.z < 0) {
                // trivial clip
                // continue;
            }

            // For now, simple perspective divide
            float invW = 1.0f / screenPos.z;
            float x = (screenPos.x * invW + 0.5f) * renderer.getViewportWidth();
            float y = (0.5f - screenPos.y * invW) * renderer.getViewportHeight();

            SDL_Vertex sdlVertex;
            sdlVertex.position = { x, y };
            // placeholder color
            sdlVertex.color = { 255, 255, 255, 255 }; 
            
            // TODO: Proper texture coordinates
            if (i < face.uCoords.size() && i < face.vCoords.size()) {
                sdlVertex.tex_coord = { (float)face.uCoords[i] / 256.0f, (float)face.vCoords[i] / 256.0f };
            } else {
                sdlVertex.tex_coord = { 0.0f, 0.0f };
            }

            vertices.push_back(sdlVertex);
        }

        // TODO: This is a placeholder. Need a proper texture manager to get the texture for the face.
        SDL_Texture* texture = nullptr;
        
        renderer.renderTexturedPolygon(vertices, texture);
    }
}

} // namespace runeharbor::graphics
