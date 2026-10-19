// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace runeharbor::game
{

// Damage element types (matches MM7 binary layout: Fire=0 through Physical=10)
enum class DamageElement : uint8_t
{
    Fire = 0,
    Air = 1,
    Water = 2,
    Earth = 3,
    Spirit = 4,
    Unused = 5,
    Body = 6,
    Mind = 7,
    Light = 8,
    Dark = 9,
    Physical = 10,
};

} // namespace runeharbor::game
