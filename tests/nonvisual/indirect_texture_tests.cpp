#include <catch2/catch_test_macros.hpp>
#include <nothofagus.h>
#include <array>
#include <cstdint>
#include <span>

// These tests lock the CPU-side palette-flatten contract that the markdown
// inline-image bridge (ImguiImageManager) relies on: IndirectTexture pixels
// are palette indices, and generateTextureData() must resolve them to RGBA8.

namespace
{

std::uint8_t toByte(float channel)
{
    // Mirror IndirectTexture::generateTextureData()'s float->byte conversion
    // (multiply by 255, truncate toward zero on assignment to uint8_t).
    return static_cast<std::uint8_t>(255.0f * channel);
}

}  // namespace

TEST_CASE("IndirectTexture::generateTextureData resolves palette indices to RGBA8", "[texture][indirect]")
{
    const Nothofagus::ColorPallete palette{
        {0.0f, 0.0f, 0.0f, 0.0f},   // 0: transparent
        {1.0f, 0.0f, 0.0f, 1.0f},   // 1: red
        {0.0f, 1.0f, 0.0f, 1.0f},   // 2: green
        {0.2f, 0.4f, 0.6f, 1.0f},   // 3: mixed
    };

    Nothofagus::IndirectTexture texture(glm::ivec2{2, 2}, glm::vec4{0.0f});
    texture.setPallete(palette);
    const std::array<Nothofagus::Pixel::ColorId, 4> indices{1, 2, 3, 0};
    texture.setPixels({indices[0], indices[1], indices[2], indices[3]});

    Nothofagus::TextureData data = texture.generateTextureData();

    REQUIRE(data.width()  == 2);
    REQUIRE(data.height() == 2);
    REQUIRE(data.layers() == 1);

    std::span<std::uint8_t> bytes = data.getDataSpan();
    REQUIRE(bytes.size() == static_cast<std::size_t>(2 * 2 * 4));

    for (std::size_t pixel = 0; pixel < indices.size(); ++pixel)
    {
        const glm::vec4& expected = palette.colors.at(indices[pixel]);
        CHECK(bytes[pixel * 4 + 0] == toByte(expected.r));
        CHECK(bytes[pixel * 4 + 1] == toByte(expected.g));
        CHECK(bytes[pixel * 4 + 2] == toByte(expected.b));
        CHECK(bytes[pixel * 4 + 3] == toByte(expected.a));
    }
}

TEST_CASE("IndirectTexture classification distinguishes flattenable from RTT-only sources", "[texture][indirect]")
{
    // The bridge flattens a DirectTexture or a plain single-layer, non-tile-map
    // IndirectTexture; animated (multi-layer) and tile-map ones are declined.
    SECTION("plain single-layer indirect texture is flattenable")
    {
        Nothofagus::IndirectTexture simple(glm::ivec2{4, 4}, glm::vec4{0.0f});
        CHECK(simple.layers() == 1);
        CHECK_FALSE(simple.hasMap());
    }

    SECTION("multi-layer (animated) indirect texture is not single-frame")
    {
        Nothofagus::IndirectTexture animated(glm::ivec2{4, 4}, glm::vec4{0.0f}, /*layers=*/3);
        CHECK(animated.layers() == 3);
        CHECK_FALSE(animated.hasMap());
    }

    SECTION("tile-map indirect texture reports a cell grid")
    {
        Nothofagus::IndirectTexture tilemap(glm::ivec2{4, 4}, glm::vec4{0.0f}, /*layers=*/2);
        tilemap.setMap({3, 3});
        CHECK(tilemap.hasMap());
    }
}
