// SPDX-License-Identifier: MIT
#pragma once

#include <span>
#include <unordered_set>
#include <vector>

#include "../engine/map_scene.hpp"
#include "camera.hpp"
#include "indoor_renderer.hpp"
#include "outdoor_renderer.hpp"
#include "sdl_renderer.hpp"
#include "visibility.hpp"

struct SDL_GPUTexture;
struct SDL_Texture;

namespace runeharbor::game
{
struct RuntimeConfig;
}

namespace runeharbor::graphics
{

class WorldRenderer
{
  public:
    WorldRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~WorldRenderer();

    using MonsterSpriteLookup = std::function<std::string(uint16_t objectType)>;
    void setTextureLookup(TextureLookup lookup);
    void setMonsterSpriteLookup(MonsterSpriteLookup lookup);
    void setSpriteFrameTable(const formats::SpriteFrameTable* table);
    void setExtraPickCandidates(std::vector<PickCandidate> candidates);
    void clearExtraPickCandidates();
    void refreshPickCache(const engine::MapScene& scene, const Camera& camera);
    std::optional<PickHit> pickMapObject(int viewportWidth, int viewportHeight, int mouseX,
                                         int mouseY, float pickRadiusPx = 36.0f,
                                         const PickSelectionFilter& filter = {}) const;
    int pickMapEvent(int viewportWidth, int viewportHeight, int mouseX, int mouseY,
                     float pickRadiusPx = 36.0f) const;
    std::span<const PickCandidate> mapEventPickCandidates() const;
    bool visibilityCacheValid() const { return visibilityCacheValid_; }
    const std::unordered_set<uint16_t>& visibleIndoorSectors() const
    {
        return visibleIndoorSectors_;
    }
    void render(const engine::MapScene& scene, const Camera& camera,
                const game::RuntimeConfig* runtimeConfig = nullptr, float nightBlend = 0.0f);

  private:
    void refreshVisibilityCache(const engine::MapScene& scene, const Camera& camera);
    void ensureOffscreenTarget(int width, int height);

    SDLRenderer& renderer_;
    std::unique_ptr<IndoorRenderer> indoorRenderer;
    std::unique_ptr<OutdoorRenderer> outdoorRenderer;
    Mat4 pickViewProjection_ = Mat4::identity();
    Mat4 visibilityViewProjection_ = Mat4::identity();
    Frustum visibilityFrustum_;
    std::unordered_set<uint16_t> visibleIndoorSectors_;
    std::vector<PickCandidate> extraPickCandidates_;
    std::vector<PickCandidate> mapEventPickCandidates_;
    bool visibilityCacheValid_ = false;
    bool pickCacheValid_ = false;
    std::string lastSceneName_;

    SDL_GPUTexture* offscreenGpuTexture_ = nullptr;
    SDL_GPUTexture* offscreenDepthTexture_ = nullptr;
    SDL_Texture* offscreenSdlTexture_ = nullptr;
    int offscreenTargetWidth_ = 0;
    int offscreenTargetHeight_ = 0;
};

} // namespace runeharbor::graphics
