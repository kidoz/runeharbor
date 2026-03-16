// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "math3d.hpp"

namespace runeharbor::graphics
{

struct LightSource
{
    Vec3 position;
    float radius = 0.0f;
    float brightness = 1.0f;
    SDL_FColor color = {1.0f, 1.0f, 1.0f, 1.0f};
    bool active = true;
};

// MM7 uses separate stacks for mobile (spell effects, torches) and stationary (map) lights.
// Each stack has a hard capacity limit matching the original engine's overflow protection.
class LightStack
{
  public:
    static constexpr int kDefaultMaxLights = 400;

    explicit LightStack(int maxLights = kDefaultMaxLights) : maxLights_(maxLights) {}

    // Push a light onto the stack. Returns false (and drops the light) if at capacity.
    bool pushLight(const LightSource& light)
    {
        if (static_cast<int>(lights_.size()) >= maxLights_)
        {
            overflowCount_++;
            return false;
        }
        lights_.push_back(light);
        return true;
    }

    void clear()
    {
        lights_.clear();
        overflowCount_ = 0;
    }

    const std::vector<LightSource>& getLights() const { return lights_; }
    int count() const { return static_cast<int>(lights_.size()); }
    int maxLights() const { return maxLights_; }
    int overflowCount() const { return overflowCount_; }
    bool isFull() const { return count() >= maxLights_; }

    // Compute distance-based light attenuation for a point.
    // Returns RGB contribution from all active lights in this stack.
    // Uses inverse-square falloff clamped to [0, brightness] within the light's radius.
    static SDL_FColor computeAttenuation(const std::vector<LightSource>& lights, float px, float py,
                                         float pz)
    {
        float r = 0.0f, g = 0.0f, b = 0.0f;

        for (const auto& light : lights)
        {
            if (!light.active || light.radius <= 0.0f)
                continue;

            float dx = px - light.position.x;
            float dy = py - light.position.y;
            float dz = pz - light.position.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            float radiusSq = light.radius * light.radius;

            if (distSq >= radiusSq)
                continue;

            // Linear falloff: 1.0 at center, 0.0 at radius edge
            float dist = std::sqrt(distSq);
            float factor = light.brightness * (1.0f - dist / light.radius);

            r += light.color.r * factor;
            g += light.color.g * factor;
            b += light.color.b * factor;
        }

        return {std::min(r, 1.0f), std::min(g, 1.0f), std::min(b, 1.0f), 1.0f};
    }

    // Convenience: compute attenuation from this stack's lights
    SDL_FColor attenuateAt(float px, float py, float pz) const
    {
        return computeAttenuation(lights_, px, py, pz);
    }

  private:
    int maxLights_;
    int overflowCount_ = 0;
    std::vector<LightSource> lights_;
};

// Per-sector ambient lighting (indoor maps).
// MM7 stores separate R/G/B ambient values per sector.
struct AmbientLight
{
    float r = 0.3f;
    float g = 0.3f;
    float b = 0.3f;

    SDL_FColor toColor() const { return {r, g, b, 1.0f}; }

    static AmbientLight fromByte(uint8_t level)
    {
        float f = 0.30f + (static_cast<float>(level) / 255.0f) * 0.70f;
        return {f, f, f};
    }
};

} // namespace runeharbor::graphics
