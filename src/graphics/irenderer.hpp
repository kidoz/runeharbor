// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>

// Forward declarations
struct SDL_Renderer;
struct SDL_Texture;

namespace runeharbor::graphics
{

class Image;

/// Abstract renderer interface for drawing 2D graphics
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    /// Clear the screen with a color
    /// @param r Red component (0-255)
    /// @param g Green component (0-255)
    /// @param b Blue component (0-255)
    /// @param a Alpha component (0-255)
    virtual void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255) = 0;

    /// Present the rendered frame to the screen
    virtual void present() = 0;

    /// Create a texture from an RGBA image
    /// @param image Image data to upload to GPU
    /// @return Opaque texture handle (SDL_Texture*) or nullptr on failure
    virtual void* createTexture(const Image& image) = 0;

    /// Destroy a texture created by createTexture()
    /// @param texture Opaque texture handle (SDL_Texture*)
    virtual void destroyTexture(void* texture) = 0;

    /// Render a texture at the specified position
    /// @param texture Opaque texture handle (SDL_Texture*)
    /// @param x X position (screen coordinates)
    /// @param y Y position (screen coordinates)
    /// @param width Width to render (0 = use texture width)
    /// @param height Height to render (0 = use texture height)
    virtual void renderTexture(void* texture, int x, int y, int width = 0, int height = 0) = 0;

    /// Get the underlying SDL renderer (for advanced use)
    virtual SDL_Renderer* getSDLRenderer() = 0;
};

} // namespace runeharbor::graphics
