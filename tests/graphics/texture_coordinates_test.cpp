// SPDX-License-Identifier: MIT

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../../src/graphics/texture_coordinates.hpp"

using namespace runeharbor::graphics;
using runeharbor::formats::FaceAttribute;
using runeharbor::formats::ParsedFace;

namespace
{
ParsedFace makeFace(FaceAttribute attribute, float normalZ = 0.0f)
{
    ParsedFace face;
    face.attributes = static_cast<uint32_t>(attribute);
    face.normalFZ = normalZ;
    return face;
}
} // namespace

TEST_CASE("Texture coordinates use the actual texture extent and face offset",
          "[texture_coordinates]")
{
    CHECK(normalizeTextureCoordinate(64.0f, 16.0f, 128.0f) == Catch::Approx(0.625f));
    CHECK(normalizeTextureCoordinate(-32.0f, 8.0f, 64.0f) == Catch::Approx(-0.375f));
    CHECK(normalizeTextureCoordinate(128.0f, 0.0f, 0.0f) == Catch::Approx(0.5f));
}

TEST_CASE("Faces without a flow attribute never scroll", "[texture_coordinates]")
{
    // Texture alignment flags are set on nearly every hand-authored face; they
    // must not be mistaken for animation.
    for (const auto attribute : {FaceAttribute::TexAlignDown, FaceAttribute::TexAlignLeft,
                                 FaceAttribute::PlaneXY, FaceAttribute::Fluid})
    {
        const ParsedFace face = makeFace(attribute);
        CHECK_FALSE(face.hasTextureFlow());

        const TextureFlow flow =
            textureFlowOffset(faceTextureFlowDirection(face), 12.5f, 128.0f, 128.0f);
        CHECK(flow.u == Catch::Approx(0.0f));
        CHECK(flow.v == Catch::Approx(0.0f));
    }
}

TEST_CASE("Directional flow scrolls along one axis", "[texture_coordinates]")
{
    const float time = 1.0f; // 62.5 texels of advance
    const float expected = TEXTURE_FLOW_TEXELS_PER_SECOND;

    SECTION("horizontal")
    {
        const TextureFlow left = textureFlowOffset(
            faceTextureFlowDirection(makeFace(FaceAttribute::FlowLeft)), time, 128.0f, 128.0f);
        CHECK(left.u == Catch::Approx(-expected));
        CHECK(left.v == Catch::Approx(0.0f));

        const TextureFlow right = textureFlowOffset(
            faceTextureFlowDirection(makeFace(FaceAttribute::FlowRight)), time, 128.0f, 128.0f);
        CHECK(right.u == Catch::Approx(expected));
    }

    SECTION("vertical direction flips on horizontal surfaces")
    {
        const TextureFlow wall =
            textureFlowOffset(faceTextureFlowDirection(makeFace(FaceAttribute::FlowDown, 0.0f)),
                              time, 128.0f, 128.0f);
        CHECK(wall.v == Catch::Approx(-expected));

        const TextureFlow floor =
            textureFlowOffset(faceTextureFlowDirection(makeFace(FaceAttribute::FlowDown, 1.0f)),
                              time, 128.0f, 128.0f);
        CHECK(floor.v == Catch::Approx(expected));

        const TextureFlow up = textureFlowOffset(
            faceTextureFlowDirection(makeFace(FaceAttribute::FlowUp, 0.0f)), time, 128.0f, 128.0f);
        CHECK(up.v == Catch::Approx(expected));
    }
}

TEST_CASE("Flow offsets stay bounded by the texture extent", "[texture_coordinates]")
{
    const ParsedFace face = makeFace(FaceAttribute::FlowRight);

    // An offset that accumulated without wrapping would reach thousands of
    // texels after a few minutes and lose precision.
    for (const float time : {0.5f, 10.0f, 600.0f, 36000.0f})
    {
        const TextureFlow flow =
            textureFlowOffset(faceTextureFlowDirection(face), time, 64.0f, 64.0f);
        CHECK(flow.u >= 0.0f);
        CHECK(flow.u < 64.0f);
    }
}

TEST_CASE("Lava sweeps instead of scrolling", "[texture_coordinates]")
{
    const ParsedFace face = makeFace(FaceAttribute::Lava);
    CHECK(face.hasTextureFlow());
    CHECK(face.isLava());
    CHECK_FALSE(face.isWater()); // lava is fluid-like but not water

    const float height = 128.0f;
    CHECK(textureFlowOffset(faceTextureFlowDirection(face), 0.0f, height, height).v ==
          Catch::Approx(0.0f));
    CHECK(textureFlowOffset(faceTextureFlowDirection(face), LAVA_FLOW_PERIOD_SECONDS * 0.25f,
                            height, height)
              .v == Catch::Approx(height));
    CHECK(
        textureFlowOffset(faceTextureFlowDirection(face), LAVA_FLOW_PERIOD_SECONDS, height, height)
            .v == Catch::Approx(0.0f).margin(1e-3f));
}

TEST_CASE("Face surface type comes from the polygon type", "[texture_coordinates]")
{
    ParsedFace face;
    face.polygonType = static_cast<uint8_t>(runeharbor::formats::PolygonType::Floor);
    CHECK(face.isFloor());
    CHECK_FALSE(face.isCeiling());
    CHECK_FALSE(face.isWall());

    face.polygonType = static_cast<uint8_t>(runeharbor::formats::PolygonType::Ceiling);
    CHECK(face.isCeiling());
    CHECK_FALSE(face.isFloor());

    face.polygonType = static_cast<uint8_t>(runeharbor::formats::PolygonType::VerticalWall);
    CHECK(face.isWall());

    // Flow and alignment bits must not be read as a surface type.
    face.attributes = static_cast<uint32_t>(FaceAttribute::FlowDown) |
                      static_cast<uint32_t>(FaceAttribute::Outlined);
    CHECK(face.isWall());
    CHECK_FALSE(face.isFloor());
    CHECK_FALSE(face.isCeiling());
}
