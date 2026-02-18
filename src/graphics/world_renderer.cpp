// SPDX-License-Identifier: MIT
#include "world_renderer.hpp"

#include "../util/ilogger.hpp"
#include "indoor_renderer.hpp"
#include "outdoor_renderer.hpp"

namespace runeharbor::graphics
{

WorldRenderer::WorldRenderer(SDLRenderer& renderer, util::ILogger& logger)
{
    indoorRenderer = std::make_unique<IndoorRenderer>(renderer, logger);
    outdoorRenderer = std::make_unique<OutdoorRenderer>(renderer, logger);
}

WorldRenderer::~WorldRenderer() = default;

void WorldRenderer::setTextureLookup(TextureLookup lookup)
{
    if (indoorRenderer)
    {
        indoorRenderer->setTextureLookup(lookup);
    }
    if (outdoorRenderer)
    {
        outdoorRenderer->setTextureLookup(std::move(lookup));
    }
}

void WorldRenderer::render(const engine::MapScene& scene, const Camera& camera)
{
    if (!scene.isLoaded())
    {
        return;
    }

    // Check if the scene is indoor (BLV) or outdoor (ODM)
    if (!scene.getBLVData().vertices.empty())
    {
        // Indoor scene
        if (indoorRenderer)
        {
            indoorRenderer->render(scene, camera);
        }
    }
    else if (!scene.getODMData().heightmap.empty())
    {
        // Outdoor scene
        if (outdoorRenderer)
        {
            outdoorRenderer->render(scene, camera);
        }
    }
}

} // namespace runeharbor::graphics
