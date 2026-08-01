// SPDX-License-Identifier: MIT
#include "world_renderer.hpp"

#include "../game/game_world.hpp"
#include "../util/ilogger.hpp"
#include "indoor_renderer.hpp"
#include "outdoor_renderer.hpp"
#include "visibility.hpp"
#include "world_coordinates.hpp"

namespace runeharbor::graphics
{

WorldRenderer::WorldRenderer(SDLRenderer& renderer, util::ILogger& logger) : renderer_(renderer)
{
    indoorRenderer = std::make_unique<IndoorRenderer>(renderer, logger);
    outdoorRenderer = std::make_unique<OutdoorRenderer>(renderer, logger);
}

WorldRenderer::~WorldRenderer()
{
    if (auto* gpu = renderer_.getGPUDevice())
    {
        if (offscreenGpuTexture_)
        {
            SDL_ReleaseGPUTexture(gpu, offscreenGpuTexture_);
        }
        if (offscreenDepthTexture_)
        {
            SDL_ReleaseGPUTexture(gpu, offscreenDepthTexture_);
        }
    }
    if (offscreenSdlTexture_)
    {
        renderer_.destroyTexture(offscreenSdlTexture_);
    }
}

void WorldRenderer::ensureOffscreenTarget(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    if (offscreenTargetWidth_ == width && offscreenTargetHeight_ == height && offscreenGpuTexture_)
        return;

    auto* gpu = renderer_.getGPUDevice();
    if (!gpu)
        return;

    if (offscreenGpuTexture_)
    {
        SDL_ReleaseGPUTexture(gpu, offscreenGpuTexture_);
        offscreenGpuTexture_ = nullptr;
    }
    if (offscreenDepthTexture_)
    {
        SDL_ReleaseGPUTexture(gpu, offscreenDepthTexture_);
        offscreenDepthTexture_ = nullptr;
    }
    if (offscreenSdlTexture_)
    {
        renderer_.destroyTexture(offscreenSdlTexture_);
        offscreenSdlTexture_ = nullptr;
    }

    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = static_cast<Uint32>(width);
    texInfo.height = static_cast<Uint32>(height);
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    offscreenGpuTexture_ = SDL_CreateGPUTexture(gpu, &texInfo);

    SDL_GPUTextureCreateInfo depthInfo = texInfo;
    depthInfo.format =
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT; // Apple Metal only supports 32-bit depth formats
    depthInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    offscreenDepthTexture_ = SDL_CreateGPUTexture(gpu, &depthInfo);

    if (!offscreenGpuTexture_ || !offscreenDepthTexture_)
    {
        return;
    }

    SDL_PropertiesID texProps = SDL_CreateProperties();
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_RGBA32);
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                          SDL_TEXTUREACCESS_TARGET);
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, height);
    SDL_SetPointerProperty(texProps, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER,
                           offscreenGpuTexture_);

    SDL_Texture* sdlTex = SDL_CreateTextureWithProperties(renderer_.getSDLRenderer(), texProps);
    SDL_DestroyProperties(texProps);

    if (sdlTex)
    {
        offscreenSdlTexture_ = sdlTex;
        offscreenTargetWidth_ = width;
        offscreenTargetHeight_ = height;
    }
    else
    {
        SDL_ReleaseGPUTexture(gpu, offscreenGpuTexture_);
        offscreenGpuTexture_ = nullptr;
        SDL_ReleaseGPUTexture(gpu, offscreenDepthTexture_);
        offscreenDepthTexture_ = nullptr;
    }
}

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

void WorldRenderer::setMonsterSpriteLookup(MonsterSpriteLookup lookup)
{
    if (indoorRenderer)
    {
        indoorRenderer->setMonsterSpriteLookup(lookup);
    }
    if (outdoorRenderer)
    {
        outdoorRenderer->setMonsterSpriteLookup(std::move(lookup));
    }
}

void WorldRenderer::setLiveActorProvider(LiveActorProvider provider)
{
    if (indoorRenderer)
    {
        indoorRenderer->setLiveActorProvider(provider);
    }
    if (outdoorRenderer)
    {
        outdoorRenderer->setLiveActorProvider(std::move(provider));
    }
}

void WorldRenderer::setWorldItemProvider(WorldItemProvider provider)
{
    if (indoorRenderer)
    {
        indoorRenderer->setWorldItemProvider(provider);
    }
    if (outdoorRenderer)
    {
        outdoorRenderer->setWorldItemProvider(std::move(provider));
    }
}

void WorldRenderer::setWorldItemSpriteLookup(WorldItemSpriteLookup lookup)
{
    if (indoorRenderer)
    {
        indoorRenderer->setWorldItemSpriteLookup(lookup);
    }
    if (outdoorRenderer)
    {
        outdoorRenderer->setWorldItemSpriteLookup(std::move(lookup));
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
        // The sector graph is in gameplay space (Z up); the camera is render space.
        visibleIndoorSectors_ = portalVisibility.computeVisibleSectors(
            blvData, renderToGameplayPosition(camera.getPosition()), 8);
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

    // Invalidate GPU geometry caches when the map changes
    if (scene.getName() != lastSceneName_)
    {
        lastSceneName_ = scene.getName();
        if (indoorRenderer)
            indoorRenderer->invalidateGPUCache();
        if (outdoorRenderer)
            outdoorRenderer->invalidateGPUCache();
    }

    auto* gpu = renderer_.getGPUDevice();
    if (gpu)
    {
        ensureOffscreenTarget(renderer_.getViewportWidth(), renderer_.getViewportHeight());
    }

    refreshVisibilityCache(scene, camera);

    // Skip the pick-cache refresh when the camera hasn't moved (it iterates all
    // faces/decorations/building-faces and allocates vectors — only changes when
    // the view does). A small epsilon handles float jitter.
    const Vec3 camPos = camera.getPosition();
    const float posEps = 0.5f;
    const bool cameraMoved = std::abs(camPos.x - lastCameraPosition_.x) > posEps ||
                             std::abs(camPos.y - lastCameraPosition_.y) > posEps ||
                             std::abs(camPos.z - lastCameraPosition_.z) > posEps;
    if (cameraMoved || !pickCacheValid_)
    {
        lastCameraPosition_ = camPos;
        refreshPickCache(scene, camera);
    }

    // Check if the scene is indoor (BLV) or outdoor (ODM)
    if (!scene.getBLVData().vertices.empty())
    {
        // Indoor scene
        if (indoorRenderer)
        {
            indoorRenderer->render(scene, camera, runtimeConfig, &visibleIndoorSectors_,
                                   offscreenGpuTexture_, offscreenDepthTexture_,
                                   offscreenSdlTexture_, nightBlend);
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
