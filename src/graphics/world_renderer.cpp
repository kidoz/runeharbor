// SPDX-License-Identifier: MIT
#include "world_renderer.hpp"

#include "../game/game_world.hpp"
#include "../util/ilogger.hpp"
#include "indoor_renderer.hpp"
#include "outdoor_renderer.hpp"
#include "visibility.hpp"

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

void WorldRenderer::setSpriteFrameTable(const formats::SpriteFrameTable* table)
{
    if (indoorRenderer)
    {
        indoorRenderer->setSpriteFrameTable(table);
    }
    if (outdoorRenderer)
    {
        outdoorRenderer->setSpriteFrameTable(table);
    }
}

void WorldRenderer::setExtraPickCandidates(std::vector<PickCandidate> candidates)
{
    extraPickCandidates_ = std::move(candidates);
}

void WorldRenderer::clearExtraPickCandidates()
{
    extraPickCandidates_.clear();
}

void WorldRenderer::refreshVisibilityCache(const engine::MapScene& scene, const Camera& camera)
{
    const bool hasGeometry =
        !scene.getBLVData().faces.empty() || !scene.getBLVData().vertices.empty() ||
        !scene.getODMData().heightmap.empty() || !scene.getODMData().buildings.empty();
    if (!scene.isLoaded() && !hasGeometry)
    {
        visibleIndoorSectors_.clear();
        visibilityCacheValid_ = false;
        return;
    }

    visibilityViewProjection_ = camera.getViewProjectionMatrix();
    visibilityFrustum_.extractFromMatrix(visibilityViewProjection_);
    visibleIndoorSectors_.clear();

    const auto& blvData = scene.getBLVData();
    if (!blvData.sectors.empty())
    {
        PortalVisibility portalVisibility;
        visibleIndoorSectors_ =
            portalVisibility.computeVisibleSectors(blvData, camera.getPosition(), 8);
    }

    visibilityCacheValid_ = true;
}

void WorldRenderer::refreshPickCache(const engine::MapScene& scene, const Camera& camera)
{
    const bool hasAnyPickableData =
        !scene.getBLVData().faces.empty() || !scene.getBLVData().decorations.empty() ||
        !scene.getODMData().buildings.empty() || !extraPickCandidates_.empty();
    if (!scene.isLoaded() && !hasAnyPickableData)
    {
        mapEventPickCandidates_.clear();
        pickCacheValid_ = false;
        return;
    }

    refreshVisibilityCache(scene, camera);

    pickViewProjection_ = camera.getViewProjectionMatrix();
    if (visibilityCacheValid_)
    {
        mapEventPickCandidates_ = collectMapEventCandidates(
            scene.getBLVData(), scene.getODMData(), visibilityFrustum_, visibleIndoorSectors_);
    }
    else
    {
        mapEventPickCandidates_ = collectMapEventCandidates(
            scene.getBLVData(), scene.getODMData(), pickViewProjection_, camera.getPosition());
    }
    mapEventPickCandidates_.insert(mapEventPickCandidates_.end(), extraPickCandidates_.begin(),
                                   extraPickCandidates_.end());
    pickCacheValid_ = true;
}

int WorldRenderer::pickMapEvent(int viewportWidth, int viewportHeight, int mouseX, int mouseY,
                                float pickRadiusPx) const
{
    PickSelectionFilter filter;
    filter.requireEventId = true;
    const auto hit =
        pickMapObject(viewportWidth, viewportHeight, mouseX, mouseY, pickRadiusPx, filter);
    return hit.has_value() ? hit->eventId : 0;
}

std::optional<PickHit> WorldRenderer::pickMapObject(int viewportWidth, int viewportHeight,
                                                    int mouseX, int mouseY, float pickRadiusPx,
                                                    const PickSelectionFilter& filter) const
{
    if (!pickCacheValid_)
    {
        return std::nullopt;
    }

    return pickClosestProjectedPoint(pickViewProjection_, viewportWidth, viewportHeight, mouseX,
                                     mouseY, mapEventPickCandidates_, pickRadiusPx, filter);
}

std::span<const PickCandidate> WorldRenderer::mapEventPickCandidates() const
{
    return mapEventPickCandidates_;
}

void WorldRenderer::render(const engine::MapScene& scene, const Camera& camera,
                           const game::RuntimeConfig* runtimeConfig, float nightBlend)
{
    if (!scene.isLoaded())
    {
        mapEventPickCandidates_.clear();
        visibleIndoorSectors_.clear();
        visibilityCacheValid_ = false;
        pickCacheValid_ = false;
        return;
    }

    refreshVisibilityCache(scene, camera);
    refreshPickCache(scene, camera);

    // Check if the scene is indoor (BLV) or outdoor (ODM)
    if (!scene.getBLVData().vertices.empty())
    {
        // Indoor scene
        if (indoorRenderer)
        {
            indoorRenderer->render(scene, camera, runtimeConfig, &visibleIndoorSectors_);
        }
    }
    else if (!scene.getODMData().heightmap.empty())
    {
        if (runtimeConfig)
        {
            TerrainLOD::configureFromGridBands(runtimeConfig->gridBand1, runtimeConfig->gridBand2,
                                               runtimeConfig->gridBand3);
        }
        else
        {
            TerrainLOD::resetDefaults();
        }

        // Outdoor scene
        if (outdoorRenderer)
        {
            outdoorRenderer->render(scene, camera, runtimeConfig, nightBlend, &visibilityFrustum_);
        }
    }
}

} // namespace runeharbor::graphics
