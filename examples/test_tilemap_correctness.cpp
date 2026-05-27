/// test_tilemap_correctness.cpp
/// Pure-data correctness tests for the Tilemap class. No canvas, no GPU.
/// Verifies inBounds at boundaries, setCell/cell round-trip, chunkData vs
/// chunkDataInto byte-equivalence, edge-chunk zero-fill, and per-chunk
/// generation counter behaviour.

#include <nothofagus.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

namespace
{

int failures = 0;

#define CHECK(cond, msg) do {                                                  \
    if (!(cond)) {                                                             \
        std::printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg);           \
        ++failures;                                                            \
    }                                                                          \
} while (0)

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

void testInBounds()
{
    std::printf("inBounds...\n");
    auto atlas = makeTrivialAtlas({8, 8}, 4);
    const Nothofagus::Tilemap tm({10, 6}, {4, 3}, {8, 8}, makeMinimalPalette(),
                                 std::span<const std::vector<std::uint8_t>>(atlas));

    // Corners
    CHECK( tm.inBounds({0, 0}),                  "(0,0) in bounds");
    CHECK( tm.inBounds({9, 5}),                  "(mapSize.x-1, mapSize.y-1) in bounds");
    CHECK( tm.inBounds({0, 5}),                  "(0, mapSize.y-1) in bounds");
    CHECK( tm.inBounds({9, 0}),                  "(mapSize.x-1, 0) in bounds");

    // Just outside each edge
    CHECK(!tm.inBounds({-1, 0}),                 "(-1, 0) out");
    CHECK(!tm.inBounds({0, -1}),                 "(0, -1) out");
    CHECK(!tm.inBounds({10, 0}),                 "(mapSize.x, 0) out");
    CHECK(!tm.inBounds({0, 6}),                  "(0, mapSize.y) out");

    // Far values
    CHECK(!tm.inBounds({-1000, -1000}),          "far-negative out");
    CHECK(!tm.inBounds({1000, 1000}),            "far-positive out");
}

void testRoundTrip()
{
    std::printf("setCell/cell round-trip...\n");
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Tilemap tm({12, 8}, {3, 2}, {4, 4}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 12; ++x)
            tm.setCell({x, y}, static_cast<std::uint8_t>((x + y) % 4));

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 12; ++x)
        {
            const std::uint8_t expected = static_cast<std::uint8_t>((x + y) % 4);
            CHECK(tm.cell({x, y}) == expected, "cell value matches what setCell wrote");
        }
}

void testChunkDataConsistency()
{
    std::printf("chunkData / chunkDataInto byte-equivalence...\n");
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
            CHECK(byValue.size() == intoBuf.size(), "chunkData size matches scratch buffer");
            CHECK(std::equal(byValue.begin(), byValue.end(), intoBuf.begin()),
                  "chunkData and chunkDataInto produce identical bytes");
        }
}

void testEdgeChunkZeroFill()
{
    std::printf("edge-chunk zero-fill (mapSize not divisible by chunkSize)...\n");
    auto atlas = makeTrivialAtlas({2, 2}, 4);
    Nothofagus::Tilemap tm({10, 6}, {4, 4}, {2, 2}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    CHECK(tm.chunkGridSize() == glm::ivec2(3, 2), "chunkGridSize is ceil-divided");

    // Paint every in-world cell with layer 3.
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
                CHECK(value == 3, "in-world cell carries assigned value");
            else
                CHECK(value == 0, "out-of-world cell is zero-filled");
        }
}

void testGenerationCounter()
{
    std::printf("per-chunk generation counter...\n");
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Tilemap tm({8, 8}, {4, 4}, {4, 4}, makeMinimalPalette(),
                           std::span<const std::vector<std::uint8_t>>(atlas));

    CHECK(tm.chunkGeneration({0, 0}) == 0, "initial gen is 0");
    CHECK(tm.chunkGeneration({1, 0}) == 0, "initial gen is 0");

    tm.setCell({2, 2}, 1);
    CHECK(tm.chunkGeneration({0, 0}) == 1, "chunk (0,0) gen bumped after edit inside it");
    CHECK(tm.chunkGeneration({1, 0}) == 0, "neighbouring chunk gen untouched");

    tm.setCell({0, 0}, 2);
    tm.setCell({3, 3}, 3);
    CHECK(tm.chunkGeneration({0, 0}) == 3, "chunk (0,0) gen reflects every edit");

    tm.setCell({5, 5}, 1);
    CHECK(tm.chunkGeneration({1, 1}) == 1, "chunk (1,1) gen bumped after edit inside it");
    CHECK(tm.chunkGeneration({0, 0}) == 3, "edits to other chunks do not affect (0,0)");
}

}  // namespace

int main()
{
    std::printf("Running Tilemap correctness tests...\n");

    testInBounds();
    testRoundTrip();
    testChunkDataConsistency();
    testEdgeChunkZeroFill();
    testGenerationCounter();

    if (failures == 0)
    {
        std::printf("OK — all Tilemap correctness tests passed.\n");
        return 0;
    }
    std::printf("FAILED — %d Tilemap correctness check(s) failed.\n", failures);
    return 1;
}
