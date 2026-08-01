// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL_gpu.h>

#include <functional>
#include <string>
#include <unordered_set>

#include "../engine/map_scene.hpp"
#include "../formats/frame_tables.hpp"
#include "camera.hpp"
#include "light_stack.hpp"
#include "live_actors.hpp"
#include "sdl_renderer.hpp"

struct SDL_Texture;
namespace runeharbor::game
{
struct RuntimeConfig;
}

namespace runeharbor::graphics
{
// Callback to resolve texture name -> SDL_Texture*
using TextureLookup = std::function<SDL_Texture*(const std::string& name)>;

class IndoorRenderer
{
  public:
    IndoorRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~IndoorRenderer();

    using MonsterSpriteLookup = std::function<std::string(uint16_t objectType)>;
    void setTextureLookup(TextureLookup lookup);
    void setMonsterSpriteLookup(MonsterSpriteLookup lookup);
    // Live roaming/combat monsters (see OutdoorRenderer::setLiveActorProvider).
    void setLiveActorProvider(LiveActorProvider provider)
    {
        liveActorProvider_ = std::move(provider);
    }
    // World-dropped items (loot piles).
    void setWorldItemProvider(WorldItemProvider provider)
    {
        worldItemProvider_ = std::move(provider);
    }
    void setWorldItemSpriteLookup(WorldItemSpriteLookup lookup)
    {
        worldItemSpriteLookup_ = std::move(lookup);
    }
    void setSpriteFrameTable(const formats::SpriteFrameTable* table);
    void render(const engine::MapScene& scene, const Camera& camera,
                const runeharbor::game::RuntimeConfig* runtimeConfig = nullptr,
                const std::unordered_set<uint16_t>* visibleSectors = nullptr,
                SDL_GPUTexture* colorTex = nullptr, SDL_GPUTexture* depthTex = nullptr,
                SDL_Texture* blitTex = nullptr, float nightBlend = 0.0f);
    void invalidateGPUCache();

    LightStack& getStationaryLightStack() { return stationaryLights_; }
    LightStack& getMobileLightStack() { return mobileLights_; }

  private:
    void initGPUPipeline();
    void buildGPUIndoor(const formats::BLVMapData& blvData);
    void renderIndoorGPU(const formats::BLVMapData& blvData, const Camera& camera,
                         const runeharbor::game::RuntimeConfig* runtimeConfig,
                         const std::unordered_set<uint16_t>* visibleSectors,
                         SDL_GPUCommandBuffer* cmdBuf, SDL_GPURenderPass* renderPass);

    struct GPUVertex
    {
        float x, y, z;
        float r, g, b, a;
        float u, v;
    };

    struct GPUDrawCall
    {
        std::string textureName;
        uint16_t sectorId;
        uint32_t indexStart;
        uint32_t indexCount;
    };

    SDLRenderer& renderer;
    [[maybe_unused]] util::ILogger& logger;
    TextureLookup textureLookup;
    MonsterSpriteLookup monsterSpriteLookup;
    LiveActorProvider liveActorProvider_;
    WorldItemProvider worldItemProvider_;
    WorldItemSpriteLookup worldItemSpriteLookup_;
    const formats::SpriteFrameTable* spriteFrameTable = nullptr;

    LightStack stationaryLights_;
    LightStack mobileLights_;

    // GPU State
    SDL_GPUDevice* gpuDevice = nullptr;
    SDL_GPUGraphicsPipeline* indoorPipeline = nullptr;
    SDL_GPUShader* vertexShader = nullptr;
    SDL_GPUShader* fragmentShader = nullptr;
    SDL_GPUBuffer* indoorVertexBuffer = nullptr;
    SDL_GPUBuffer* indoorIndexBuffer = nullptr;
    SDL_GPUSampler* defaultSampler = nullptr;
    uint32_t indoorIndexCount = 0;
    bool gpuInitialized = false;
    std::vector<GPUDrawCall> indoorDrawCalls;
};

} // namespace runeharbor::graphics
