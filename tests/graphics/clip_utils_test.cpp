// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>

#include "../../src/graphics/clip_utils.hpp"

using namespace runeharbor::graphics;

TEST_CASE("Software texture tessellation has camera-independent topology", "[clip_utils]")
{
    const SDL_FColor white = {1.0f, 1.0f, 1.0f, 1.0f};
    const ClipVertex near0{{-1.0f, -1.0f, 0.0f, 1.0f}, white, 0.0f, 0.0f};
    const ClipVertex near1{{1.0f, -1.0f, 0.0f, 2.0f}, white, 1.0f, 0.0f};
    const ClipVertex near2{{0.0f, 1.0f, 0.0f, 4.0f}, white, 0.5f, 1.0f};
    const ClipVertex far0{{-1.0f, -1.0f, 0.0f, 100.0f}, white, 0.0f, 0.0f};
    const ClipVertex far1{{1.0f, -1.0f, 0.0f, 101.0f}, white, 1.0f, 0.0f};
    const ClipVertex far2{{0.0f, 1.0f, 0.0f, 102.0f}, white, 0.5f, 1.0f};

    std::vector<ClipVertex> nearVertices;
    std::vector<ClipVertex> farVertices;
    tessellateTriangle(near0, near1, near2, nearVertices);
    tessellateTriangle(far0, far1, far2, farVertices);

    constexpr size_t EXPECTED_VERTEX_COUNT = 3 * 4 * 4 * 4;
    CHECK(nearVertices.size() == EXPECTED_VERTEX_COUNT);
    CHECK(farVertices.size() == EXPECTED_VERTEX_COUNT);
}
