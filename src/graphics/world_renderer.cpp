// SPDX-License-Identifier: MIT
#include "world_renderer.hpp"
#include "indoor_renderer.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::graphics
{

WorldRenderer::WorldRenderer(SDLRenderer& renderer, util::ILogger& logger)
    : logger(logger)
{
    indoorRenderer = std::make_unique<IndoorRenderer>(renderer, logger);
}

WorldRenderer::~WorldRenderer() = default;

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
        // TODO: Implement OutdoorRenderer
        logger.warning("Outdoor rendering is not yet implemented.");
    }
}

} // namespace runeharbor::graphics
