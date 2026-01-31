// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

namespace runeharbor::graphics
{

/// Represents a 256-color palette for converting paletted images to RGBA
class Palette
{
public:
    struct Color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;

        Color() : r(0), g(0), b(0), a(255) {}
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    };

    Palette();

    /// Load palette from 768-byte RGB data (256 colors × 3 bytes)
    static Palette fromRGBData(const std::vector<uint8_t>& data);

    /// Create a default VGA-style palette for testing
    static Palette createDefaultPalette();

    /// Get color at index (0-255)
    const Color& getColor(uint8_t index) const;

    /// Set color at index
    void setColor(uint8_t index, const Color& color);

    /// Get pointer to color array
    const Color* data() const;

private:
    Color colors[256];
};

} // namespace runeharbor::graphics
