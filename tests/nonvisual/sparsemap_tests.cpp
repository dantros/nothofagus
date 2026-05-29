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
// Sparsemap::hasChunk — empty by default, true after addChunk, false after removeChunk
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::hasChunk tracks chunk lifecycle", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({3, 2}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    SECTION("empty sparsemap has no chunks anywhere")
    {
        CHECK_FALSE(sm.hasChunk({0, 0}));
        CHECK_FALSE(sm.hasChunk({1, 1}));
        CHECK_FALSE(sm.hasChunk({-1, -1}));
        CHECK_FALSE(sm.hasChunk({1000, -1000}));
        CHECK(sm.chunkCount() == 0);
    }

    SECTION("addChunk makes hasChunk return true for that coord only")
    {
        sm.addChunk({2, 3});
        CHECK(sm.hasChunk({2, 3}));
        CHECK_FALSE(sm.hasChunk({2, 2}));
        CHECK_FALSE(sm.hasChunk({3, 3}));
        CHECK(sm.chunkCount() == 1);
    }

    SECTION("removeChunk reverses addChunk")
    {
        sm.addChunk({2, 3});
        sm.removeChunk({2, 3});
        CHECK_FALSE(sm.hasChunk({2, 3}));
        CHECK(sm.chunkCount() == 0);
    }

    SECTION("removeChunk on a never-present coord is a no-op")
    {
        sm.removeChunk({5, 5});
        CHECK_FALSE(sm.hasChunk({5, 5}));
        CHECK(sm.chunkCount() == 0);
    }
}

// ---------------------------------------------------------------------------
// Sparsemap::setCell — lazy-creates the owning chunk on first write
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::setCell lazy-creates the owning chunk", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    REQUIRE_FALSE(sm.hasChunk({0, 0}));

    sm.setCell({1, 2}, 3);

    CHECK(sm.hasChunk({0, 0}));
    CHECK(sm.cell({1, 2}) == 3);
    // The non-written cells inside the freshly created chunk are zero.
    CHECK(sm.cell({0, 0}) == 0);
    CHECK(sm.cell({3, 3}) == 0);
    // First write into a chunk bumps its generation to 1.
    CHECK(sm.chunkGeneration({0, 0}) == 1);
}

TEST_CASE("Sparsemap::setCell routes coordinates to the correct chunk", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    // Spread writes across four chunk quadrants.
    sm.setCell({0,  0}, 1);   // chunk (0, 0), local (0, 0)
    sm.setCell({4,  0}, 2);   // chunk (1, 0), local (0, 0)
    sm.setCell({0,  4}, 3);   // chunk (0, 1), local (0, 0)
    sm.setCell({7,  7}, 1);   // chunk (1, 1), local (3, 3)

    CHECK(sm.hasChunk({0, 0}));
    CHECK(sm.hasChunk({1, 0}));
    CHECK(sm.hasChunk({0, 1}));
    CHECK(sm.hasChunk({1, 1}));
    CHECK_FALSE(sm.hasChunk({2, 0}));

    CHECK(sm.cell({0, 0}) == 1);
    CHECK(sm.cell({4, 0}) == 2);
    CHECK(sm.cell({0, 4}) == 3);
    CHECK(sm.cell({7, 7}) == 1);
}

// ---------------------------------------------------------------------------
// Sparsemap::cell — returns 0 when the owning chunk is not present
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::cell returns 0 for missing chunks", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    CHECK(sm.cell({0, 0}) == 0);
    CHECK(sm.cell({100, -50}) == 0);

    sm.addChunk({0, 0});
    CHECK(sm.cell({0, 0}) == 0);  // zero-initialized
    sm.setCell({0, 0}, 2);
    CHECK(sm.cell({0, 0}) == 2);
}

// ---------------------------------------------------------------------------
// Sparsemap::addChunk — round-trip with explicit cellData, generation bumps
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::addChunk with cellData round-trips through chunkDataInto", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({3, 2}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    const std::vector<std::uint8_t> input{1, 2, 3, 0, 1, 2};  // 3 cols × 2 rows
    sm.addChunk({4, -2}, std::span<const std::uint8_t>(input));

    REQUIRE(sm.hasChunk({4, -2}));
    CHECK(sm.chunkGeneration({4, -2}) == 1);

    std::vector<std::uint8_t> out(input.size());
    sm.chunkDataInto({4, -2}, std::span<std::uint8_t>(out));
    CHECK(std::equal(out.begin(), out.end(), input.begin()));
}

TEST_CASE("Sparsemap::addChunk with empty cellData zero-initializes the chunk", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    sm.addChunk({0, 0});  // default cellData = {}

    REQUIRE(sm.hasChunk({0, 0}));
    std::vector<std::uint8_t> out(16);
    sm.chunkDataInto({0, 0}, std::span<std::uint8_t>(out));
    for (std::size_t i = 0; i < out.size(); ++i)
        CHECK(out[i] == 0);
}

// ---------------------------------------------------------------------------
// Sparsemap::chunkDataInto — zero-fills when the chunk is not present
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::chunkDataInto zero-fills missing chunks", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    // Prime the buffer with a sentinel so a successful zero-fill is observable.
    std::vector<std::uint8_t> out(16, static_cast<std::uint8_t>(0xAB));
    sm.chunkDataInto({7, 7}, std::span<std::uint8_t>(out));

    for (std::size_t i = 0; i < out.size(); ++i)
        CHECK(out[i] == 0);
    CHECK_FALSE(sm.hasChunk({7, 7}));  // reading doesn't materialize the chunk
}

// ---------------------------------------------------------------------------
// Sparsemap::chunkGeneration — independent per chunk, bumps locally
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap per-chunk generation counter bumps independently", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    CHECK(sm.chunkGeneration({0, 0}) == 0);  // missing → 0
    CHECK(sm.chunkGeneration({1, 1}) == 0);

    sm.setCell({0, 0}, 1);    // creates chunk (0,0), gen 0 -> 1
    CHECK(sm.chunkGeneration({0, 0}) == 1);
    CHECK(sm.chunkGeneration({1, 1}) == 0);

    sm.setCell({1, 1}, 2);    // edit inside chunk (0,0) again
    CHECK(sm.chunkGeneration({0, 0}) == 2);
    CHECK(sm.chunkGeneration({1, 1}) == 0);

    sm.setCell({4, 4}, 3);    // creates chunk (1, 1), gen 0 -> 1
    CHECK(sm.chunkGeneration({0, 0}) == 2);
    CHECK(sm.chunkGeneration({1, 1}) == 1);

    sm.addChunk({0, 0});      // overwriting an existing chunk via addChunk bumps generation again
    CHECK(sm.chunkGeneration({0, 0}) == 3);
}

// ---------------------------------------------------------------------------
// Sparsemap::removeChunk drops state — re-adding starts fresh at generation 1
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::removeChunk drops generation; re-adding restarts at 1", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    sm.setCell({0, 0}, 1);
    sm.setCell({1, 1}, 2);
    REQUIRE(sm.chunkGeneration({0, 0}) == 2);

    sm.removeChunk({0, 0});
    CHECK_FALSE(sm.hasChunk({0, 0}));
    CHECK(sm.chunkGeneration({0, 0}) == 0);
    CHECK(sm.cell({0, 0}) == 0);

    sm.addChunk({0, 0});
    CHECK(sm.hasChunk({0, 0}));
    CHECK(sm.chunkGeneration({0, 0}) == 1);
    CHECK(sm.cell({1, 1}) == 0);  // previous data is gone
}

// ---------------------------------------------------------------------------
// debugCheck-rejection cases (e.g., chunkDataInto with a wrong-sized span,
// setCell with an out-of-range layer index) are NOT testable via Catch2 — see
// the note at the bottom of tilemap_tests.cpp. We exercise only happy paths.
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::chunkDataInto accepts a correctly-sized span", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    sm.addChunk({0, 0});
    std::vector<std::uint8_t> buf(4 * 4);
    CHECK_NOTHROW(sm.chunkDataInto({0, 0}, std::span<std::uint8_t>(buf)));
    CHECK_NOTHROW(sm.chunkDataInto({1, 1}, std::span<std::uint8_t>(buf))); // missing chunk → zero-fill
}
