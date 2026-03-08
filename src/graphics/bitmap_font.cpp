// SPDX-License-Identifier: MIT
#include "bitmap_font.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

#include <cstring>

namespace runeharbor::graphics
{

// .fnt file layout (MM7 binary font format):
//   Header (32 bytes):
//     offset 0: firstChar(u8)
//     offset 1: lastChar(u8)
//     offset 2: bitsPerPixel(u8) = 8
//     offset 3: reserved(u8) = 0
//     offset 4: reserved(u8) = 0
//     offset 5: fontHeight(u8)
//     offset 6-31: reserved(26 bytes, zeroed)
//   GlyphMetrics[256]: leftSpacing(i32), width(i32), rightSpacing(i32) = 3072 bytes
//   PixelOffsets[256]: offset(u32) = 1024 bytes
//   Pixel data: palette-indexed (0=transparent, nonzero=FONTPAL index)
//
// Total header+atlas = 32 + 3072 + 1024 = 4128 bytes before pixel data

static constexpr size_t kHeaderSize = 32;
static constexpr size_t kGlyphMetricsSize = 256 * 12; // 256 * (3 * i32)
static constexpr size_t kPixelOffsetsSize = 256 * 4;
static constexpr size_t kAtlasHeaderSize = kHeaderSize + kGlyphMetricsSize + kPixelOffsetsSize;

BitmapFont::BitmapFont() = default;

BitmapFont::~BitmapFont()
{
    if (atlas_)
    {
        SDL_DestroyTexture(atlas_);
        atlas_ = nullptr;
    }
}

BitmapFont::BitmapFont(BitmapFont&& other) noexcept
    : firstChar_(other.firstChar_), lastChar_(other.lastChar_), fontHeight_(other.fontHeight_),
      loaded_(other.loaded_), pixelData_(std::move(other.pixelData_)),
      paletteRGB_(std::move(other.paletteRGB_)), atlas_(other.atlas_),
      atlasWidth_(other.atlasWidth_), atlasHeight_(other.atlasHeight_)
{
    std::memcpy(glyphs_, other.glyphs_, sizeof(glyphs_));
    std::memcpy(atlasRects_, other.atlasRects_, sizeof(atlasRects_));
    other.atlas_ = nullptr;
    other.loaded_ = false;
}

BitmapFont& BitmapFont::operator=(BitmapFont&& other) noexcept
{
    if (this != &other)
    {
        if (atlas_)
        {
            SDL_DestroyTexture(atlas_);
        }
        firstChar_ = other.firstChar_;
        lastChar_ = other.lastChar_;
        fontHeight_ = other.fontHeight_;
        loaded_ = other.loaded_;
        pixelData_ = std::move(other.pixelData_);
        paletteRGB_ = std::move(other.paletteRGB_);
        atlas_ = other.atlas_;
        atlasWidth_ = other.atlasWidth_;
        atlasHeight_ = other.atlasHeight_;
        std::memcpy(glyphs_, other.glyphs_, sizeof(glyphs_));
        std::memcpy(atlasRects_, other.atlasRects_, sizeof(atlasRects_));
        other.atlas_ = nullptr;
        other.loaded_ = false;
    }
    return *this;
}

static uint32_t readU32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static int32_t readI32(const uint8_t* p)
{
    return static_cast<int32_t>(readU32(p));
}

bool BitmapFont::load(std::span<const uint8_t> fntData, std::span<const uint8_t> fontPalRGB)
{
    loaded_ = false;

    if (fntData.size() < kAtlasHeaderSize)
    {
        return false;
    }

    if (fontPalRGB.size() < 768)
    {
        return false;
    }

    // Parse header
    firstChar_ = fntData[0];
    lastChar_ = fntData[1];
    // fntData[2] is bitsPerPixel (typically 8)
    // fntData[3..4] are reserved (zero)
    fontHeight_ = fntData[5];

    if (fontHeight_ == 0)
    {
        return false;
    }

    // Parse glyph metrics (256 entries, starting at offset 32)
    const uint8_t* metricsBase = fntData.data() + kHeaderSize;
    for (int i = 0; i < 256; i++)
    {
        const uint8_t* gm = metricsBase + i * 12;
        glyphs_[i].leftSpacing = readI32(gm);
        glyphs_[i].width = readI32(gm + 4);
        glyphs_[i].rightSpacing = readI32(gm + 8);
    }

    // Parse pixel offsets (256 entries, starting at offset 32 + 3072)
    const uint8_t* offsetBase = fntData.data() + kHeaderSize + kGlyphMetricsSize;
    for (int i = 0; i < 256; i++)
    {
        glyphs_[i].pixelOffset = readU32(offsetBase + i * 4);
    }

    // Copy pixel data (everything after the atlas header)
    if (fntData.size() > kAtlasHeaderSize)
    {
        pixelData_.assign(fntData.begin() + kAtlasHeaderSize, fntData.end());
    }
    else
    {
        pixelData_.clear();
    }

    // Copy palette
    paletteRGB_.assign(fontPalRGB.begin(), fontPalRGB.begin() + 768);

    loaded_ = true;
    return true;
}

bool BitmapFont::createAtlas(SDL_Renderer* sdlRenderer)
{
    if (!loaded_ || !sdlRenderer || fontHeight_ == 0)
    {
        return false;
    }

    // Destroy old atlas if any
    if (atlas_)
    {
        SDL_DestroyTexture(atlas_);
        atlas_ = nullptr;
    }

    // Calculate total atlas width
    int totalWidth = 0;
    for (int i = firstChar_; i <= lastChar_; i++)
    {
        int w = std::max(glyphs_[i].width, static_cast<int32_t>(0));
        totalWidth += w;
    }

    if (totalWidth == 0)
    {
        return false;
    }

    atlasWidth_ = totalWidth;
    atlasHeight_ = fontHeight_;

    // Build RGBA pixel buffer for the atlas
    std::vector<uint8_t> rgba(static_cast<size_t>(atlasWidth_) * atlasHeight_ * 4, 0);

    int curX = 0;
    for (int i = 0; i < 256; i++)
    {
        atlasRects_[i] = {0, 0, 0, 0};

        if (i < firstChar_ || i > lastChar_)
        {
            continue;
        }

        int gw = std::max(glyphs_[i].width, static_cast<int32_t>(0));
        if (gw == 0)
        {
            continue;
        }

        atlasRects_[i] = {static_cast<float>(curX), 0.0f, static_cast<float>(gw),
                          static_cast<float>(fontHeight_)};

        // Copy glyph pixels into atlas
        uint32_t offset = glyphs_[i].pixelOffset;
        for (int row = 0; row < fontHeight_; row++)
        {
            for (int col = 0; col < gw; col++)
            {
                size_t srcIdx = offset + static_cast<size_t>(row) * gw + col;
                uint8_t value = 0;
                if (srcIdx < pixelData_.size())
                {
                    value = pixelData_[srcIdx];
                }

                size_t dstIdx = (static_cast<size_t>(row) * atlasWidth_ + curX + col) * 4;

                if (value == 0)
                {
                    // Transparent
                    rgba[dstIdx] = 0;
                    rgba[dstIdx + 1] = 0;
                    rgba[dstIdx + 2] = 0;
                    rgba[dstIdx + 3] = 0;
                }
                else if (value == 1)
                {
                    // Shadow pixel: opaque black drop shadow
                    rgba[dstIdx] = 0;
                    rgba[dstIdx + 1] = 0;
                    rgba[dstIdx + 2] = 0;
                    rgba[dstIdx + 3] = 255;
                }
                else
                {
                    // Body pixel: white so SDL_SetTextureColorMod produces exact text color
                    rgba[dstIdx] = 255;
                    rgba[dstIdx + 1] = 255;
                    rgba[dstIdx + 2] = 255;
                    rgba[dstIdx + 3] = 255;
                }
            }
        }

        curX += gw;
    }

    // Create SDL texture
    atlas_ = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                               atlasWidth_, atlasHeight_);
    if (!atlas_)
    {
        return false;
    }

    SDL_SetTextureBlendMode(atlas_, SDL_BLENDMODE_BLEND);

    if (!SDL_UpdateTexture(atlas_, nullptr, rgba.data(), atlasWidth_ * 4))
    {
        SDL_DestroyTexture(atlas_);
        atlas_ = nullptr;
        return false;
    }

    return true;
}

void BitmapFont::renderText(SDL_Renderer* renderer, int x, int y, float scale,
                            std::string_view text, uint8_t r, uint8_t g, uint8_t b) const
{
    if (!atlas_ || !renderer || text.empty() || scale <= 0.0f)
    {
        return;
    }

    // Apply color modulation to the atlas texture
    SDL_SetTextureColorMod(atlas_, r, g, b);

    float curX = static_cast<float>(x);
    float curY = static_cast<float>(y);

    for (char ch : text)
    {
        uint8_t c = static_cast<uint8_t>(ch);
        if (c < firstChar_ || c > lastChar_)
        {
            // Use space width as fallback
            float advance = static_cast<float>(glyphs_[' '].leftSpacing + glyphs_[' '].width +
                                               glyphs_[' '].rightSpacing);
            curX += advance * scale;
            continue;
        }

        const auto& glyph = glyphs_[c];
        const auto& rect = atlasRects_[c];

        curX += glyph.leftSpacing * scale;

        if (rect.w > 0 && rect.h > 0)
        {
            SDL_FRect srcRect = {rect.x, rect.y, rect.w, rect.h};
            SDL_FRect dstRect = {curX, curY, rect.w * scale, rect.h * scale};
            SDL_RenderTexture(renderer, atlas_, &srcRect, &dstRect);
        }

        curX += (glyph.width + glyph.rightSpacing) * scale;
    }
}

int BitmapFont::measureText(std::string_view text) const
{
    if (!loaded_ || text.empty())
    {
        return 0;
    }

    int totalWidth = 0;
    for (char ch : text)
    {
        uint8_t c = static_cast<uint8_t>(ch);
        if (c < firstChar_ || c > lastChar_)
        {
            totalWidth += glyphs_[' '].leftSpacing + glyphs_[' '].width + glyphs_[' '].rightSpacing;
            continue;
        }
        totalWidth += glyphs_[c].leftSpacing + glyphs_[c].width + glyphs_[c].rightSpacing;
    }
    return totalWidth;
}

int BitmapFont::measureTextScaled(std::string_view text, float scale) const
{
    return static_cast<int>(measureText(text) * scale);
}

} // namespace runeharbor::graphics
