#include <catch2/catch_test_macros.hpp>
#include <nothofagus.h>
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace
{

Nothofagus::ColorPallete makeMinimalPalette()
{
    return Nothofagus::ColorPallete{
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
    };
}

std::vector<std::vector<std::uint8_t>> makeTrivialAtlas(glm::ivec2 tileSize, std::size_t layerCount)
{
    const std::size_t pixels = static_cast<std::size_t>(tileSize.x * tileSize.y);
    std::vector<std::vector<std::uint8_t>> out;
    out.reserve(layerCount);
    for (std::size_t i = 0; i < layerCount; ++i)
        out.emplace_back(pixels, static_cast<std::uint8_t>(i % 4));
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tilemap::inBounds — corners, just-outside-each-edge, far-out values
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap::inBounds reports world-cell membership correctly", "[tilemap]")
{
    auto atlas = makeTrivialAtlas({8, 8}, 4);
    const Nothofagus::Tilemap tm({10, 6}, {4, 3}, {8, 8}, makeMinimalPalette(),
                                 std::span<const std::vector<std::uint8_t>>(atlas));

    SECTION("corners are inside")
    {
        CHECK(tm.inBounds({0, 0}));
        CHECK(tm.inBounds({9, 5}));
        CHECK(tm.inBounds({0, 5}));
        CHECK(tm.inBounds({9, 0}));
    }

    SECTION("one step outside each edge is rejected")
    {
        CHECK_FALSE(tm.inBounds({-1, 0}));
        CHECK_FALSE(tm.inBounds({0, -1}));
        CHECK_FALSE(tm.inBounds({10, 0}));
        CHECK_FALSE(tm.inBounds({0, 6}));
    }

    SECTION("far-out values are rejected")
    {
        CHECK_FALSE(tm.inBounds({-1000, -1000}));
        CHECK_FALSE(tm.inBounds({1000, 1000}));
    }
}

// ---------------------------------------------------------------------------
// Tilemap::setCell / cell round-trip across the whole world
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap::cell returns what setCell wrote", "[tilemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Tilemap tm({12, 8}, {3, 2}, {4, 4}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 12; ++x)
            tm.setCell({x, y}, static_cast<std::uint8_t>((x + y) % 4));

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 12; ++x)
            CHECK(tm.cell({x, y}) == static_cast<std::uint8_t>((x + y) % 4));
}

// ---------------------------------------------------------------------------
// Tilemap::chunkData and chunkDataInto produce identical bytes
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap::chunkData and chunkDataInto produce identical bytes", "[tilemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Tilemap tm({12, 8}, {3, 2}, {4, 4}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 12; ++x)
            tm.setCell({x, y}, static_cast<std::uint8_t>((x + y * 3) % 4));

    const glm::ivec2 chunkGrid = tm.chunkGridSize();
    const std::size_t cellsPerChunk = 3 * 2;
    std::vector<std::uint8_t> intoBuf(cellsPerChunk);

    for (int cy = 0; cy < chunkGrid.y; ++cy)
        for (int cx = 0; cx < chunkGrid.x; ++cx)
        {
            std::vector<std::uint8_t> byValue = tm.chunkData({cx, cy});
            tm.chunkDataInto({cx, cy}, std::span<std::uint8_t>(intoBuf));
            REQUIRE(byValue.size() == intoBuf.size());
            CHECK(std::equal(byValue.begin(), byValue.end(), intoBuf.begin()));
        }
}

// ---------------------------------------------------------------------------
// Tilemap edge chunks zero-fill out-of-world cells when mapSize is not a
// multiple of chunkSize
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap edge chunks zero-fill out-of-world cells", "[tilemap]")
{
    auto atlas = makeTrivialAtlas({2, 2}, 4);
    Nothofagus::Tilemap tm({10, 6}, {4, 4}, {2, 2}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    REQUIRE(tm.chunkGridSize() == glm::ivec2(3, 2));

    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 10; ++x)
            tm.setCell({x, y}, 3);

    // Bottom-right chunk (2, 1): world cols 8..11, rows 4..7. World is 10×6, so
    // cols 10–11 and rows 6–7 fall outside and must zero-fill.
    std::vector<std::uint8_t> buf(16);
    tm.chunkDataInto({2, 1}, std::span<std::uint8_t>(buf));

    for (int localRow = 0; localRow < 4; ++localRow)
        for (int localCol = 0; localCol < 4; ++localCol)
        {
            const int worldCol = 8 + localCol;
            const int worldRow = 4 + localRow;
            const std::uint8_t value = buf[static_cast<std::size_t>(localRow * 4 + localCol)];
            const bool inWorld = worldCol < 10 && worldRow < 6;
            if (inWorld)
                CHECK(value == 3);
            else
                CHECK(value == 0);
        }
}

// ---------------------------------------------------------------------------
// Per-chunk generation counter — bumps only for the chunk receiving an edit
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap per-chunk generation counter bumps locally", "[tilemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Tilemap tm({8, 8}, {4, 4}, {4, 4}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    CHECK(tm.chunkGeneration({0, 0}) == 0);
    CHECK(tm.chunkGeneration({1, 0}) == 0);

    tm.setCell({2, 2}, 1);
    CHECK(tm.chunkGeneration({0, 0}) == 1);
    CHECK(tm.chunkGeneration({1, 0}) == 0);

    tm.setCell({0, 0}, 2);
    tm.setCell({3, 3}, 3);
    CHECK(tm.chunkGeneration({0, 0}) == 3);

    tm.setCell({5, 5}, 1);
    CHECK(tm.chunkGeneration({1, 1}) == 1);
    CHECK(tm.chunkGeneration({0, 0}) == 3);
}
