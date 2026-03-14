// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL.h>

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

class LightStack
{
  public:
    void pushLight(const LightSource& light) { lights_.push_back(light); }

    void clear() { lights_.clear(); }

    const std::vector<LightSource>& getLights() const { return lights_; }

  private:
    std::vector<LightSource> lights_;
};

} // namespace runeharbor::graphics
