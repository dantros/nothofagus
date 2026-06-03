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

// ---------------------------------------------------------------------------
// IndirectTexture::setMapBulk — round-trip + dirty flag. This is the hot-path
// upload primitive used by TilemapExplorer slot syncs in tilemap_manager.cpp.
// ---------------------------------------------------------------------------
TEST_CASE("IndirectTexture::setMapBulk overwrites the entire cell grid", "[tilemap]")
{
    Nothofagus::IndirectTexture tex({4, 4}, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), 4);
    tex.setPallete(makeMinimalPalette());
    tex.setMap({3, 2});
    tex.clearMapDirty();  // setMap marks dirty; reset so we can observe setMapBulk's effect

    const std::vector<std::uint8_t> input{1, 2, 3, 0, 1, 2};  // row-major 3 cols × 2 rows
    tex.setMapBulk(std::span<const std::uint8_t>(input));

    CHECK(tex.isMapDirty());

    const std::vector<std::uint8_t> roundTripped = tex.generateMapData();
    REQUIRE(roundTripped.size() == input.size());
    CHECK(std::equal(roundTripped.begin(), roundTripped.end(), input.begin()));

    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 3; ++col)
            CHECK(tex.cell(col, row) == input[static_cast<std::size_t>(row * 3 + col)]);
}

TEST_CASE("IndirectTexture::setMapBulk preserves atlas and palette state", "[tilemap]")
{
    Nothofagus::IndirectTexture tex({4, 4}, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), 4);
    tex.setPallete(makeMinimalPalette());
    tex.setMap({2, 2});

    const std::size_t originalLayers = tex.layers();
    const glm::ivec2  originalSize   = tex.size();

    const std::vector<std::uint8_t> input{0, 1, 2, 3};
    tex.setMapBulk(std::span<const std::uint8_t>(input));

    CHECK(tex.layers()  == originalLayers);
    CHECK(tex.size()    == originalSize);
    CHECK(tex.mapSize() == glm::ivec2(2, 2));
}

// ---------------------------------------------------------------------------
// debugCheck-rejection cases (e.g., chunkDataInto with a wrong-sized span,
// setMapBulk with a size mismatch, setCell out-of-bounds) are NOT testable
// via Catch2: source/check.h's debugCheck calls `throw;` with no active
// exception, which invokes std::terminate(). The process aborts before any
// REQUIRE_THROWS / CHECK_THROWS handler runs.
//
// If/when debugCheck is changed to throw a typed exception (or aborts are
// trapped via a custom assertion handler), these cases can be tested with
// REQUIRE_THROWS_AS / similar. Until then we exercise only the happy path.
// ---------------------------------------------------------------------------
TEST_CASE("Tilemap::chunkDataInto accepts a correctly-sized span", "[tilemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Tilemap tm({8, 8}, {4, 4}, {4, 4}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    std::vector<std::uint8_t> buf(4 * 4);  // matches chunkSize.x * chunkSize.y
    CHECK_NOTHROW(tm.chunkDataInto({0, 0}, std::span<std::uint8_t>(buf)));
    CHECK_NOTHROW(tm.chunkDataInto({1, 1}, std::span<std::uint8_t>(buf)));
}

// ---------------------------------------------------------------------------
// TilemapExplorer pool-grid-size formula edge cases.
//
// Mirrors the formula at source/tilemap_manager.cpp:84-87:
//   poolGridSize.x = ceil_div(screenSize.x, chunkPixelSize.x) + 2
//   poolGridSize.y = ceil_div(screenSize.y, chunkPixelSize.y) + 2
//
// If the source formula changes, this helper must be updated in lockstep.
// The "camera exactly on a chunk boundary" and "world smaller than viewport"
// edge cases listed in the original test gap are integration scenarios that
// require a live Canvas + TilemapExplorer; they belong with the visual /
// integration suite (or a future explorer integration file), not here.
// ---------------------------------------------------------------------------
namespace
{
glm::ivec2 poolGridSizeFor(glm::ivec2 screenSize, glm::ivec2 chunkPixelSize)
{
    return {
        (screenSize.x + chunkPixelSize.x - 1) / chunkPixelSize.x + 2,
        (screenSize.y + chunkPixelSize.y - 1) / chunkPixelSize.y + 2
    };
}
}

TEST_CASE("TilemapExplorer pool grid size handles small viewports", "[tilemap]")
{
    SECTION("viewport smaller than one chunk yields the +2 margin only")
    {
        // 16-px screen / 32-px chunk → ceil(16/32) = 1, so pool = 1 + 2 = 3 on each axis.
        CHECK(poolGridSizeFor({16, 16}, {32, 32}) == glm::ivec2(3, 3));
    }

    SECTION("viewport exactly matches one chunk")
    {
        CHECK(poolGridSizeFor({32, 32}, {32, 32}) == glm::ivec2(3, 3));
    }

    SECTION("viewport one pixel past a chunk boundary needs the next chunk")
    {
        // 33-px / 32-px → ceil = 2, so pool = 2 + 2 = 4. Verifies +2 margin
        // is added on top of the ceiling, not the floor.
        CHECK(poolGridSizeFor({33, 33}, {32, 32}) == glm::ivec2(4, 4));
    }

    SECTION("typical 256x240 canvas with 32x32-px chunks")
    {
        // ceil(256/32) = 8, ceil(240/32) = 8 → 10x10 pool.
        CHECK(poolGridSizeFor({256, 240}, {32, 32}) == glm::ivec2(10, 10));
    }

    SECTION("non-square viewport against square chunks")
    {
        // ceil(300/32) = 10 → 12; ceil(100/32) = 4 → 6.
        CHECK(poolGridSizeFor({300, 100}, {32, 32}) == glm::ivec2(12, 6));
    }
}
