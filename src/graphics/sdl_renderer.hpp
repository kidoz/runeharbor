// SPDX-License-Identifier: MIT
#pragma once

#include "../util/ilogger.hpp"
#include "irenderer.hpp"

struct SDL_Window;
struct SDL_Renderer;

namespace runeharbor::graphics
{

/// SDL3-based 2D renderer implementation
class SDLRenderer : public IRenderer
{
  public:
    /// Create renderer for an SDL window
    /// @param window SDL window to render to
    /// @param logger Logger for diagnostics
    explicit SDLRenderer(SDL_Window* window, util::ILogger& logger);
    ~SDLRenderer() override;

    // Non-copyable, non-movable (owns SDL_Renderer*)
    SDLRenderer(const SDLRenderer&) = delete;
    SDLRenderer& operator=(const SDLRenderer&) = delete;
    SDLRenderer(SDLRenderer&&) = delete;
    SDLRenderer& operator=(SDLRenderer&&) = delete;

    // IRenderer interface
    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255) override;
    void present() override;
    void* createTexture(const Image& image) override;
    void destroyTexture(void* texture) override;
    void renderTexture(void* texture, int x, int y, int width = 0, int height = 0) override;
    void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b,
                  uint8_t a = 255) override;
    void drawFilledRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b,
                        uint8_t a = 255) override;
    void renderTexturedPolygon(const std::vector<SDL_Vertex>& vertices,
                               SDL_Texture* texture) override;
    SDL_Renderer* getSDLRenderer() override { return renderer; }
    int getViewportWidth() const override;
    int getViewportHeight() const override;

  private:
    void updateViewport();

    util::ILogger& logger;
    SDL_Renderer* renderer = nullptr;
    int viewportWidth = 0;
    int viewportHeight = 0;
};

} // namespace runeharbor::graphics
