#include <catch2/catch_test_macros.hpp>
#include <nothofagus.h>
#include <algorithm>
#include <climits>
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
// Sparsemap::chunkInBounds — empty by default, true after addChunk, false after removeChunk
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::chunkInBounds tracks chunk lifecycle", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({3, 2}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    SECTION("empty sparsemap has no chunks anywhere")
    {
        CHECK_FALSE(sm.chunkInBounds({0, 0}));
        CHECK_FALSE(sm.chunkInBounds({1, 1}));
        CHECK_FALSE(sm.chunkInBounds({-1, -1}));
        CHECK_FALSE(sm.chunkInBounds({1000, -1000}));
        CHECK(sm.chunkCount() == 0);
    }

    SECTION("addChunk makes chunkInBounds return true for that coord only")
    {
        sm.addChunk({2, 3});
        CHECK(sm.chunkInBounds({2, 3}));
        CHECK_FALSE(sm.chunkInBounds({2, 2}));
        CHECK_FALSE(sm.chunkInBounds({3, 3}));
        CHECK(sm.chunkCount() == 1);
    }

    SECTION("removeChunk reverses addChunk")
    {
        sm.addChunk({2, 3});
        sm.removeChunk({2, 3});
        CHECK_FALSE(sm.chunkInBounds({2, 3}));
        CHECK(sm.chunkCount() == 0);
    }

    SECTION("removeChunk on a never-present coord is a no-op")
    {
        sm.removeChunk({5, 5});
        CHECK_FALSE(sm.chunkInBounds({5, 5}));
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

    REQUIRE_FALSE(sm.chunkInBounds({0, 0}));

    sm.setCell({1, 2}, 3);

    CHECK(sm.chunkInBounds({0, 0}));
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

    CHECK(sm.chunkInBounds({0, 0}));
    CHECK(sm.chunkInBounds({1, 0}));
    CHECK(sm.chunkInBounds({0, 1}));
    CHECK(sm.chunkInBounds({1, 1}));
    CHECK_FALSE(sm.chunkInBounds({2, 0}));

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

    REQUIRE(sm.chunkInBounds({4, -2}));
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

    REQUIRE(sm.chunkInBounds({0, 0}));
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
    CHECK_FALSE(sm.chunkInBounds({7, 7}));  // reading doesn't materialize the chunk
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
    CHECK_FALSE(sm.chunkInBounds({0, 0}));
    CHECK(sm.chunkGeneration({0, 0}) == 0);
    CHECK(sm.cell({0, 0}) == 0);

    sm.addChunk({0, 0});
    CHECK(sm.chunkInBounds({0, 0}));
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

// ---------------------------------------------------------------------------
// Negative-coord coverage for `setCell` / `cell` / `chunkInBounds`. Exercises
// the floor-division helper that maps world coords (potentially negative) onto
// {chunk coord, intra-chunk index}. Without these, the negative branch of the
// helper would be dark — the rest of the suite only crosses negatives through
// `addChunk` (which takes chunk coords) and `cell` against missing chunks.
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap::setCell / cell round-trip across negative world coords", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    // chunkSize = 4 → chunk -1 spans cells [-4..-1]; chunk -2 spans [-8..-5].
    sm.setCell({-1, -1}, 1);   // chunk (-1, -1), local (3, 3)
    sm.setCell({-4, -4}, 2);   // chunk (-1, -1), local (0, 0)
    sm.setCell({-5, -5}, 3);   // chunk (-2, -2), local (3, 3)
    sm.setCell({-8, 0},  1);   // chunk (-2,  0), local (0, 0)
    sm.setCell({ 0, -8}, 2);   // chunk ( 0, -2), local (0, 0)

    CHECK(sm.cell({-1, -1}) == 1);
    CHECK(sm.cell({-4, -4}) == 2);
    CHECK(sm.cell({-5, -5}) == 3);
    CHECK(sm.cell({-8, 0})  == 1);
    CHECK(sm.cell({ 0, -8}) == 2);

    CHECK(sm.chunkInBounds({-1, -1}));
    CHECK(sm.chunkInBounds({-2, -2}));
    CHECK(sm.chunkInBounds({-2,  0}));
    CHECK(sm.chunkInBounds({ 0, -2}));
    CHECK_FALSE(sm.chunkInBounds({-3, -3})); // nothing wrote there

    // Sibling cells inside an already-created negative chunk are zero-init.
    CHECK(sm.cell({-2, -2}) == 0);
    CHECK(sm.cell({-3, -3}) == 0);
}

TEST_CASE("Sparsemap::setCell straddles the 0 / -1 boundary cleanly", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    // Cells immediately on either side of zero must land in distinct chunks
    // (no off-by-one collapsing them into the same chunk).
    sm.setCell({ 0,  0}, 1);
    sm.setCell({-1,  0}, 2);
    sm.setCell({ 0, -1}, 3);
    sm.setCell({-1, -1}, 1);

    CHECK(sm.chunkInBounds({ 0,  0}));
    CHECK(sm.chunkInBounds({-1,  0}));
    CHECK(sm.chunkInBounds({ 0, -1}));
    CHECK(sm.chunkInBounds({-1, -1}));

    CHECK(sm.cell({ 0,  0}) == 1);
    CHECK(sm.cell({-1,  0}) == 2);
    CHECK(sm.cell({ 0, -1}) == 3);
    CHECK(sm.cell({-1, -1}) == 1);

    // Each of the four quadrant chunks bumps its own generation independently.
    CHECK(sm.chunkGeneration({ 0,  0}) == 1);
    CHECK(sm.chunkGeneration({-1,  0}) == 1);
    CHECK(sm.chunkGeneration({ 0, -1}) == 1);
    CHECK(sm.chunkGeneration({-1, -1}) == 1);
}

TEST_CASE("Sparsemap::setCell handles INT_MIN-adjacent coords without overflow UB", "[sparsemap]")
{
    auto atlas = makeTrivialAtlas({4, 4}, 4);
    Nothofagus::Sparsemap sm({4, 4}, {4, 4}, makeMinimalPalette(),
                             std::span<const std::vector<std::uint8_t>>(atlas));

    // The pre-helper implementation negated worldCell.x to compute the chunk
    // coord, which is UB at INT_MIN. The current floorDivMod helper avoids the
    // negation and stays defined for every representable int. These writes must
    // round-trip even at the bottom of the int range.
    sm.setCell({INT_MIN,     0}, 1);
    sm.setCell({INT_MIN + 1, 0}, 2);
    sm.setCell({INT_MIN + 3, 0}, 3);
    sm.setCell({0, INT_MIN},     1);

    CHECK(sm.cell({INT_MIN,     0}) == 1);
    CHECK(sm.cell({INT_MIN + 1, 0}) == 2);
    CHECK(sm.cell({INT_MIN + 3, 0}) == 3);
    CHECK(sm.cell({0, INT_MIN})     == 1);

    // INT_MIN is exactly a chunk boundary (INT_MIN % 4 == 0), so INT_MIN..INT_MIN+3
    // share one chunk; INT_MIN/4 is the chunk coord, which fits comfortably in int.
    CHECK(sm.chunkInBounds({INT_MIN / 4, 0}));
    CHECK(sm.chunkInBounds({0,            INT_MIN / 4}));
}

// ---------------------------------------------------------------------------
// Public-API surface coverage for methods the explorer doesn't consume. These
// are part of the `TilemapLike` concept and the Sparsemap public surface, so
// they're worth a light round-trip even though the chunk-sync hot path never
// calls them. Without this coverage, dropping or renaming them would slip past
// the existing test suite.
// ---------------------------------------------------------------------------
TEST_CASE("Sparsemap accessors echo construction parameters", "[sparsemap]")
{
    const glm::ivec2 chunkSize{5, 3};
    const glm::ivec2 tileSize{8, 16};
    const Nothofagus::ColorPallete palette = makeMinimalPalette();
    auto atlas = makeTrivialAtlas(tileSize, 2);

    Nothofagus::Sparsemap sm(chunkSize, tileSize, palette,
                             std::span<const std::vector<std::uint8_t>>(atlas));

    CHECK(sm.chunkSize()      == chunkSize);
    CHECK(sm.tileSize()       == tileSize);
    CHECK(sm.chunkPixelSize() == chunkSize * tileSize);   // 40 × 48
    CHECK(sm.chunkCount()     == 0);

    // Palette round-trip: each registered color comes back through the cache template.
    const auto& storedPalette = sm.palette();
    REQUIRE(storedPalette.colors.size() == palette.colors.size());
    for (std::size_t i = 0; i < palette.colors.size(); ++i)
        CHECK(storedPalette.colors[i] == palette.colors[i]);
}
