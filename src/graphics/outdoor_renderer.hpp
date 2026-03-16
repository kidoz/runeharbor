// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL_gpu.h>

#include <functional>
#include <string>

#include "../engine/map_scene.hpp"
#include "../formats/frame_tables.hpp"
#include "camera.hpp"
#include "sdl_renderer.hpp"
#include "visibility.hpp"

struct SDL_Texture;

namespace runeharbor::game
{
struct RuntimeConfig;
}

namespace runeharbor::graphics
{

class OutdoorRenderer
{
  public:
    using TextureLookup = std::function<SDL_Texture*(const std::string& name)>;

    OutdoorRenderer(SDLRenderer& renderer, util::ILogger& logger);
    ~OutdoorRenderer();

    using MonsterSpriteLookup = std::function<std::string(uint16_t objectType)>;
    void setTextureLookup(TextureLookup lookup);
    void setMonsterSpriteLookup(MonsterSpriteLookup lookup);
    void setSpriteFrameTable(const formats::SpriteFrameTable* table);
    void render(const engine::MapScene& scene, const Camera& camera,
                const game::RuntimeConfig* runtimeConfig = nullptr, float nightBlend = 0.0f,
                const Frustum* frustumOverride = nullptr,
                SDL_GPUTexture* colorTex = nullptr, SDL_GPUTexture* depthTex = nullptr, SDL_Texture* blitTex = nullptr);
    void invalidateGPUCache();

  private:
    void renderSky(const game::RuntimeConfig* runtimeConfig, float nightBlend);
    void renderTerrain(const formats::ODMMapData& odmData, const Camera& camera,
                       const game::RuntimeConfig* runtimeConfig, float nightBlend,
                       const Frustum* frustumOverride);
    void renderTerrainGPU(const formats::ODMMapData& odmData, const Camera& camera,
                          const game::RuntimeConfig* runtimeConfig, float nightBlend,
                          const Frustum* frustumOverride, SDL_GPUCommandBuffer* cmdBuf,
                          SDL_GPURenderPass* renderPass);
    void renderBuildings(const formats::ODMMapData& odmData, const Camera& camera,
                         const game::RuntimeConfig* runtimeConfig, float nightBlend,
                         const Frustum* frustumOverride);
    void renderSpawnBillboards(const formats::ODMMapData& odmData, const Camera& camera,
                               const game::RuntimeConfig* runtimeConfig, float nightBlend,
                               const Frustum* frustumOverride);

    void initGPUPipeline();
    void buildGPUTerrain(const formats::ODMMapData& odmData);

    struct GPUVertex
    {
        float x, y, z;
        float r, g, b, a;
        float u, v;
    };

    struct GPUDrawCall
    {
        std::string textureName;
        uint32_t indexStart;
        uint32_t indexCount;
    };

    SDLRenderer& renderer;
    util::ILogger& logger;
    TextureLookup textureLookup;
    MonsterSpriteLookup monsterSpriteLookup;
    const formats::SpriteFrameTable* spriteFrameTable = nullptr;

    // GPU State
    SDL_GPUDevice* gpuDevice = nullptr;
    SDL_GPUGraphicsPipeline* terrainPipeline = nullptr;
    SDL_GPUShader* vertexShader = nullptr;
    SDL_GPUShader* fragmentShader = nullptr;
    SDL_GPUBuffer* terrainVertexBuffer = nullptr;
    SDL_GPUBuffer* terrainIndexBuffer = nullptr;
    SDL_GPUSampler* defaultSampler = nullptr;
    uint32_t terrainIndexCount = 0;
    bool gpuInitialized = false;
    std::vector<GPUDrawCall> terrainDrawCalls;
};

} // namespace runeharbor::graphics
