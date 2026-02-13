// SPDX-License-Identifier: MIT
#include "sdl_renderer.hpp"

#include <SDL3/SDL.h>
#include <vector>
#include "image.hpp"

namespace runeharbor::graphics
{

SDLRenderer::SDLRenderer(SDL_Window* window, util::ILogger& logger) : logger(logger)
{
    if (!window)
    {
        logger.error("Cannot create renderer: null window");
        return;
    }

    // Create SDL3 renderer with GPU acceleration
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
    {
        logger.error("Failed to create SDL renderer: " + std::string(SDL_GetError()));
        return;
    }

    // Enable VSync by default
    SDL_SetRenderVSync(renderer, 1);

    logger.info("Created SDL3 renderer");
}

SDLRenderer::~SDLRenderer()
{
    if (renderer)
    {
        logger.debug("Destroying SDL renderer");
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
}

void SDLRenderer::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!renderer)
    {
        logger.error("Cannot clear: renderer not initialized");
        return;
    }

    updateViewport();

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderClear(renderer);
}

void SDLRenderer::present()
{
    if (!renderer)
    {
        logger.error("Cannot present: renderer not initialized");
        return;
    }

    SDL_RenderPresent(renderer);
}

void* SDLRenderer::createTexture(const Image& image)
{
    if (!renderer)
    {
        logger.error("Cannot create texture: renderer not initialized");
        return nullptr;
    }

    uint32_t width = image.getWidth();
    uint32_t height = image.getHeight();

    // Create SDL texture (RGBA format, streaming access)
    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                          static_cast<int>(width), static_cast<int>(height));

    if (!texture)
    {
        logger.error("Failed to create texture: " + std::string(SDL_GetError()));
        return nullptr;
    }

    // Upload RGBA data to texture
    const uint8_t* pixels = image.data();
    int pitch = static_cast<int>(width * 4); // 4 bytes per pixel (RGBA)

    if (!SDL_UpdateTexture(texture, nullptr, pixels, pitch))
    {
        logger.error("Failed to update texture: " + std::string(SDL_GetError()));
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    // Enable alpha blending for textures with transparency
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    logger.debug("Created texture: " + std::to_string(width) + "x" + std::to_string(height));
    return texture;
}

void SDLRenderer::destroyTexture(void* texture)
{
    if (texture)
    {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(texture));
    }
}

void SDLRenderer::renderTexture(void* texture, int x, int y, int width, int height)
{
    if (!renderer)
    {
        logger.error("Cannot render texture: renderer not initialized");
        return;
    }

    if (!texture)
    {
        logger.warning("Attempted to render null texture");
        return;
    }

    SDL_Texture* sdlTexture = static_cast<SDL_Texture*>(texture);

    // Query texture size if width/height not specified
    if (width == 0 || height == 0)
    {
        float w, h;
        SDL_GetTextureSize(sdlTexture, &w, &h);
        width = static_cast<int>(w);
        height = static_cast<int>(h);
    }

    // Set destination rectangle
    SDL_FRect dstRect;
    dstRect.x = static_cast<float>(x);
    dstRect.y = static_cast<float>(y);
    dstRect.w = static_cast<float>(width);
    dstRect.h = static_cast<float>(height);

    // Render texture
    SDL_RenderTexture(renderer, sdlTexture, nullptr, &dstRect);
}

void SDLRenderer::renderTexturedPolygon(const std::vector<SDL_Vertex>& vertices, SDL_Texture* texture)
{
    if (!renderer)
    {
        logger.error("Cannot render polygon: renderer not initialized");
        return;
    }

    if (vertices.size() < 3)
    {
        return; // Not a valid polygon
    }

    if (texture)
    {
        SDL_RenderGeometry(renderer, texture, vertices.data(), vertices.size(), nullptr, 0);
    }
    else
    {
        // Render a solid white polygon if no texture is provided
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderGeometry(renderer, nullptr, vertices.data(), vertices.size(), nullptr, 0);
    }
}

int SDLRenderer::getViewportWidth() const
{
    return viewportWidth;
}

int SDLRenderer::getViewportHeight() const
{
    return viewportHeight;
}

void SDLRenderer::updateViewport()
{
    if (!renderer)
    {
        return;
    }

    SDL_GetRenderOutputSize(renderer, &viewportWidth, &viewportHeight);
}

} // namespace runeharbor::graphics