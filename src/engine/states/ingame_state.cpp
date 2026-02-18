// SPDX-License-Identifier: MIT
#include "ingame_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "../../formats/odm_map.hpp"
#include "../../graphics/camera.hpp"
#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"
#include "../../graphics/world_renderer.hpp"

namespace runeharbor::engine
{

namespace
{
const char* onOff(bool value)
{
    return value ? "ON" : "OFF";
}

std::string toUpper(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}
} // namespace

InGameState::InGameState(StateContext& ctx) : ctx(ctx) {}

InGameState::~InGameState() = default;

void InGameState::enter() {}

void InGameState::exit() {}

std::optional<GameStateId> InGameState::update()
{
    updateCameraInput();

    // Toggle render options
    if (ctx.isKeyPressed(SDL_SCANCODE_F))
    {
        renderOptions.showFloors = !renderOptions.showFloors;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_V))
    {
        renderOptions.showWalls = !renderOptions.showWalls;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_C))
    {
        renderOptions.showCeilings = !renderOptions.showCeilings;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_P))
    {
        renderOptions.showPortals = !renderOptions.showPortals;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_L))
    {
        renderOptions.showLights = !renderOptions.showLights;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_G))
    {
        showGrid = !showGrid;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_X))
    {
        showAxes = !showAxes;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_H))
    {
        showHelpOverlay = !showHelpOverlay;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_R) && ctx.camera)
    {
        if (ctx.shared && ctx.shared->mapScene && ctx.shared->mapScene->isLoaded())
        {
            const auto& bounds = ctx.shared->mapScene->getBounds();
            if (bounds.valid)
            {
                float distance = std::max(bounds.radius() * 2.5f, 1000.0f);
                ctx.camera->lookAt(bounds.center(), distance);
            }
        }
    }

    return std::nullopt;
}

void InGameState::render()
{
    if (!ctx.shared)
    {
        return;
    }

    bool mapLoaded = ctx.shared->mapScene && ctx.shared->mapScene->isLoaded();
    if (mapLoaded && ctx.worldRenderer && ctx.camera)
    {
        ctx.worldRenderer->render(*ctx.shared->mapScene, *ctx.camera);
    }

    renderOverlay();
}

void InGameState::updateCameraInput()
{
    if (!ctx.camera || !ctx.shared || !ctx.shared->mapScene)
    {
        return;
    }

    float orbitSpeed = 0.015f;
    float zoomSpeed = 50.0f;
    float panSpeed = 8.0f;

    if (ctx.isKeyDown(SDL_SCANCODE_LSHIFT) || ctx.isKeyDown(SDL_SCANCODE_RSHIFT))
    {
        orbitSpeed *= 2.0f;
        zoomSpeed *= 2.0f;
        panSpeed *= 2.0f;
    }

    if (ctx.isKeyDown(SDL_SCANCODE_LEFT))
    {
        ctx.camera->orbit(-orbitSpeed, 0.0f);
    }
    if (ctx.isKeyDown(SDL_SCANCODE_RIGHT))
    {
        ctx.camera->orbit(orbitSpeed, 0.0f);
    }
    if (ctx.isKeyDown(SDL_SCANCODE_UP))
    {
        ctx.camera->orbit(0.0f, orbitSpeed);
    }
    if (ctx.isKeyDown(SDL_SCANCODE_DOWN))
    {
        ctx.camera->orbit(0.0f, -orbitSpeed);
    }

    if (ctx.isKeyDown(SDL_SCANCODE_Q))
    {
        ctx.camera->zoom(zoomSpeed);
    }
    if (ctx.isKeyDown(SDL_SCANCODE_E))
    {
        ctx.camera->zoom(-zoomSpeed);
    }

    if (ctx.isKeyDown(SDL_SCANCODE_W))
    {
        ctx.camera->pan(0.0f, panSpeed);
    }
    if (ctx.isKeyDown(SDL_SCANCODE_S))
    {
        ctx.camera->pan(0.0f, -panSpeed);
    }
    if (ctx.isKeyDown(SDL_SCANCODE_A))
    {
        ctx.camera->pan(-panSpeed, 0.0f);
    }
    if (ctx.isKeyDown(SDL_SCANCODE_D))
    {
        ctx.camera->pan(panSpeed, 0.0f);
    }
}

void InGameState::renderOverlay()
{
    if (!showHelpOverlay || !ctx.debugText || !ctx.renderer || !ctx.renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    std::vector<std::string> lines;

    if (ctx.shared && ctx.shared->mapScene && ctx.shared->mapScene->isLoaded())
    {
        auto& mapScene = *ctx.shared->mapScene;
        if (mapScene.getODMData().heightmap.empty())
        {
            const auto& data = mapScene.getBLVData();
            lines.push_back("MAP: " + toUpper(mapScene.getName()));
            lines.push_back(std::format("VERTS: {}  FACES: {}  LIGHTS: {}", data.vertices.size(),
                                        data.faces.size(), data.lights.size()));
        }
        else
        {
            const auto& data = mapScene.getODMData();
            lines.push_back("MAP: " + toUpper(mapScene.getName()));
            lines.push_back(
                std::format("TERRAIN: {}x{}  BUILDINGS: {}",
                            data.heightmap.size() > 0 ? formats::ODMMapData::TERRAIN_SIZE : 0,
                            data.heightmap.size() > 0 ? formats::ODMMapData::TERRAIN_SIZE : 0,
                            data.buildings.size()));
        }
    }
    else
    {
        lines.push_back("MAP: (NONE)");
    }

    lines.push_back("ARROWS ORBIT  Q/E ZOOM  WASD PAN  R RESET");
    lines.push_back(std::format("F FLOORS:{}  V WALLS:{}  C CEIL:{}  P PORTAL:{}",
                                onOff(renderOptions.showFloors), onOff(renderOptions.showWalls),
                                onOff(renderOptions.showCeilings),
                                onOff(renderOptions.showPortals)));
    lines.push_back(std::format("L LIGHTS:{}  G GRID:{}  X AXES:{}  H HELP:{}",
                                onOff(renderOptions.showLights), onOff(showGrid), onOff(showAxes),
                                onOff(showHelpOverlay)));

    const int scale = 2;
    int maxLen = 0;
    for (const auto& line : lines)
    {
        maxLen = std::max(maxLen, static_cast<int>(line.size()));
    }

    const int padding = 8;
    int boxWidth = ctx.debugText->charWidth(scale) * maxLen + padding * 2;
    int boxHeight = ctx.debugText->lineHeight(scale) * static_cast<int>(lines.size()) + padding * 2;

    SDL_FRect panel = {10.0f, 10.0f, static_cast<float>(boxWidth), static_cast<float>(boxHeight)};
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 180);
    SDL_RenderFillRect(sdlRenderer, &panel);

    int cursorY = static_cast<int>(panel.y) + padding;
    for (const auto& line : lines)
    {
        ctx.debugText->drawText(sdlRenderer, static_cast<int>(panel.x) + padding, cursorY, scale,
                                230, 230, 230, line);
        cursorY += ctx.debugText->lineHeight(scale);
    }
}

} // namespace runeharbor::engine
