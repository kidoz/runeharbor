// SPDX-License-Identifier: MIT
#include "debug_text.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

#include <cctype>

namespace runeharbor::graphics
{

void DebugText::drawText(SDL_Renderer* renderer, int x, int y, int scale, uint8_t r, uint8_t g,
                         uint8_t b, std::string_view text) const
{
    if (!renderer || scale <= 0)
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    int cursorX = x;
    int cursorY = y;

    for (char c : text)
    {
        if (c == '\n')
        {
            cursorX = x;
            cursorY += lineHeight(scale);
            continue;
        }

        const auto glyph = getGlyph(c);
        for (int row = 0; row < fontHeight; row++)
        {
            uint8_t rowBits = glyph[static_cast<size_t>(row)];
            for (int col = 0; col < fontWidth; col++)
            {
                bool on = (rowBits >> (fontWidth - 1 - col)) & 0x1;
                if (!on)
                {
                    continue;
                }

                SDL_FRect rect = {static_cast<float>(cursorX + col * scale),
                                  static_cast<float>(cursorY + row * scale),
                                  static_cast<float>(scale), static_cast<float>(scale)};
                SDL_RenderFillRect(renderer, &rect);
            }
        }

        cursorX += charWidth(scale);
    }
}

int DebugText::measureTextWidth(std::string_view text, int scale) const
{
    if (scale <= 0)
    {
        return 0;
    }

    int width = 0;
    int lineWidth = 0;

    for (char c : text)
    {
        if (c == '\n')
        {
            width = std::max(width, lineWidth);
            lineWidth = 0;
            continue;
        }

        (void)c;
        lineWidth += charWidth(scale);
    }

    width = std::max(width, lineWidth);
    return width;
}

std::array<uint8_t, DebugText::fontHeight> DebugText::getGlyph(char c) const
{
    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (upper)
    {
    case 'A':
        return {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    case 'B':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110};
    case 'C':
        return {0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111};
    case 'D':
        return {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
    case 'E':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
    case 'F':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000};
    case 'G':
        return {0b01111, 0b10000, 0b10000, 0b10011, 0b10001, 0b10001, 0b01111};
    case 'H':
        return {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001};
    case 'I':
        return {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
    case 'J':
        return {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100};
    case 'K':
        return {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001};
    case 'L':
        return {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
    case 'M':
        return {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001};
    case 'N':
        return {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001};
    case 'O':
        return {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    case 'P':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000};
    case 'Q':
        return {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101};
    case 'R':
        return {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
    case 'S':
        return {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110};
    case 'T':
        return {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};
    case 'U':
        return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
    case 'V':
        return {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100};
    case 'W':
        return {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010};
    case 'X':
        return {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001};
    case 'Y':
        return {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100};
    case 'Z':
        return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111};
    case '0':
        return {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110};
    case '1':
        return {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110};
    case '2':
        return {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111};
    case '3':
        return {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110};
    case '4':
        return {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010};
    case '5':
        return {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110};
    case '6':
        return {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110};
    case '7':
        return {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000};
    case '8':
        return {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110};
    case '9':
        return {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110};
    case '.':
        return {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00100};
    case ':':
        return {0b00000, 0b00100, 0b00000, 0b00000, 0b00100, 0b00000, 0b00000};
    case '/':
        return {0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b00000, 0b00000};
    case '-':
        return {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000};
    case '_':
        return {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111};
    case ' ':
        return {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000};
    default:
        return {0b01110, 0b10001, 0b00010, 0b00100, 0b00100, 0b00000, 0b00100};
    }
}

} // namespace runeharbor::graphics
