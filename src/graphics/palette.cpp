// SPDX-License-Identifier: MIT
#include "palette.hpp"

#include <stdexcept>

namespace runeharbor::graphics
{

Palette::Palette()
{
    // Initialize with default grayscale
    for (int i = 0; i < 256; i++)
    {
        colors[i] = Color(i, i, i, 255);
    }
}

Palette Palette::fromRGBData(const std::vector<uint8_t>& data)
{
    if (data.size() != 768)
    {
        throw std::runtime_error("Palette data must be exactly 768 bytes (256 × 3 RGB)");
    }

    Palette palette;

    for (int i = 0; i < 256; i++)
    {
        palette.colors[i].r = data[i * 3 + 0];
        palette.colors[i].g = data[i * 3 + 1];
        palette.colors[i].b = data[i * 3 + 2];
        palette.colors[i].a = 255; // Fully opaque
    }

    return palette;
}

Palette Palette::createDefaultPalette()
{
    Palette palette;

    // Create a simple default palette with basic colors
    // This is a fallback if we can't load the actual palette

    // Color 0: Black (often used for transparency)
    palette.colors[0] = Color(0, 0, 0, 0); // Transparent black

    // Colors 1-15: Standard EGA colors
    palette.colors[1] = Color(0, 0, 170);      // Blue
    palette.colors[2] = Color(0, 170, 0);      // Green
    palette.colors[3] = Color(0, 170, 170);    // Cyan
    palette.colors[4] = Color(170, 0, 0);      // Red
    palette.colors[5] = Color(170, 0, 170);    // Magenta
    palette.colors[6] = Color(170, 85, 0);     // Brown
    palette.colors[7] = Color(170, 170, 170);  // Light gray
    palette.colors[8] = Color(85, 85, 85);     // Dark gray
    palette.colors[9] = Color(85, 85, 255);    // Light blue
    palette.colors[10] = Color(85, 255, 85);   // Light green
    palette.colors[11] = Color(85, 255, 255);  // Light cyan
    palette.colors[12] = Color(255, 85, 85);   // Light red
    palette.colors[13] = Color(255, 85, 255);  // Light magenta
    palette.colors[14] = Color(255, 255, 85);  // Yellow
    palette.colors[15] = Color(255, 255, 255); // White

    // Colors 16-231: 6×6×6 RGB cube
    int index = 16;
    for (int r = 0; r < 6; r++)
    {
        for (int g = 0; g < 6; g++)
        {
            for (int b = 0; b < 6; b++)
            {
                palette.colors[index++] = Color(r * 51, g * 51, b * 51);
            }
        }
    }

    // Colors 232-255: Grayscale ramp
    for (int i = 0; i < 24; i++)
    {
        uint8_t gray = 8 + i * 10;
        palette.colors[232 + i] = Color(gray, gray, gray);
    }

    return palette;
}

const Palette::Color& Palette::getColor(uint8_t index) const
{
    return colors[index];
}

void Palette::setColor(uint8_t index, const Color& color)
{
    colors[index] = color;
}

const Palette::Color* Palette::data() const
{
    return colors;
}

} // namespace runeharbor::graphics
