// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace runeharbor::graphics
{

/// Bitmap font renderer for MM7 .fnt format files.
/// Parses the binary .fnt format, builds an SDL texture atlas,
/// and renders text strings glyph-by-glyph.
class BitmapFont
{
  public:
    BitmapFont();
    ~BitmapFont();

    // Non-copyable, movable
    BitmapFont(const BitmapFont&) = delete;
    BitmapFont& operator=(const BitmapFont&) = delete;
    BitmapFont(BitmapFont&& other) noexcept;
    BitmapFont& operator=(BitmapFont&& other) noexcept;

    /// Parse a .fnt file and FONTPAL palette data.
    /// @param fntData Raw .fnt file bytes
    /// @param fontPalRGB 768-byte RGB palette (256 colors x 3 bytes)
    /// @return true if parsed successfully
    bool load(std::span<const uint8_t> fntData, std::span<const uint8_t> fontPalRGB);

    /// Build the GPU texture atlas. Must be called after load().
    /// @param sdlRenderer The SDL renderer to create the texture on
    /// @return true if atlas was created successfully
    bool createAtlas(SDL_Renderer* sdlRenderer);

    /// Render a text string at the given position with optional scale.
    /// @param renderer SDL renderer
    /// @param x X position in screen coordinates
    /// @param y Y position in screen coordinates
    /// @param scale Uniform scale factor (1.0 = native font size)
    /// @param text The string to render
    /// @param r Red color component (0-255)
    /// @param g Green color component (0-255)
    /// @param b Blue color component (0-255)
    void renderText(SDL_Renderer* renderer, int x, int y, float scale, std::string_view text,
                    uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) const;

    /// Measure the pixel width of a text string (in native font pixels).
    int measureText(std::string_view text) const;

    /// Measure the scaled pixel width of a text string.
    int measureTextScaled(std::string_view text, float scale) const;

    /// Get the font height in pixels.
    int height() const { return fontHeight_; }

    /// Check if the font is loaded and ready to render.
    bool isLoaded() const { return loaded_; }

    /// Check if the atlas texture has been created.
    bool hasAtlas() const { return atlas_ != nullptr; }

  private:
    struct Glyph
    {
        int32_t leftSpacing = 0;
        int32_t width = 0;
        int32_t rightSpacing = 0;
        uint32_t pixelOffset = 0;
    };

    struct AtlasRect
    {
        float x = 0, y = 0, w = 0, h = 0;
    };

    uint8_t firstChar_ = 0;
    uint8_t lastChar_ = 0;
    uint8_t fontHeight_ = 0;
    bool loaded_ = false;

    Glyph glyphs_[256] = {};
    std::vector<uint8_t> pixelData_;  // raw grayscale pixel data
    std::vector<uint8_t> paletteRGB_; // 768-byte FONTPAL copy

    SDL_Texture* atlas_ = nullptr;
    AtlasRect atlasRects_[256] = {};
    int atlasWidth_ = 0;
    int atlasHeight_ = 0;
};

} // namespace runeharbor::graphics
