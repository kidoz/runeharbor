// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

struct SDL_Renderer;

namespace runeharbor::graphics
{

class DebugText
{
  public:
    void drawText(SDL_Renderer* renderer, int x, int y, int scale, uint8_t r, uint8_t g, uint8_t b,
                  std::string_view text) const;

    int measureTextWidth(std::string_view text, int scale) const;
    int lineHeight(int scale) const { return (fontHeight + 1) * scale; }
    int charWidth(int scale) const { return (fontWidth + 1) * scale; }

  private:
    static constexpr int fontWidth = 5;
    static constexpr int fontHeight = 7;

    std::array<uint8_t, fontHeight> getGlyph(char c) const;
};

} // namespace runeharbor::graphics
