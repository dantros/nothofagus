// Pure-logic tests for the present-mode → OpenGL swap-interval mapping. No render
// backend or display required (nonvisual group).
#include <catch2/catch_test_macros.hpp>
#include <present_mode.h>

using Nothofagus::PresentMode;
using Nothofagus::presentModeToSwapInterval;

TEST_CASE("presentModeToSwapInterval maps each mode to the right GL swap interval", "[present_mode]")
{
    // GL has no Mailbox: Fifo and Mailbox both request vsync (interval 1);
    // Immediate disables it (interval 0).
    CHECK(presentModeToSwapInterval(PresentMode::Fifo)      == 1);
    CHECK(presentModeToSwapInterval(PresentMode::Mailbox)   == 1);
    CHECK(presentModeToSwapInterval(PresentMode::Immediate) == 0);
}

TEST_CASE("present-mode mapping is constexpr-evaluable", "[present_mode]")
{
    static_assert(presentModeToSwapInterval(PresentMode::Fifo)      == 1);
    static_assert(presentModeToSwapInterval(PresentMode::Mailbox)   == 1);
    static_assert(presentModeToSwapInterval(PresentMode::Immediate) == 0);
    SUCCEED();
}

TEST_CASE("PresentMode enum values are stable (ABI / serialization)", "[present_mode]")
{
    // These underlie any persisted/serialized present-mode value; pin them.
    CHECK(static_cast<std::uint8_t>(PresentMode::Fifo)      == 0);
    CHECK(static_cast<std::uint8_t>(PresentMode::Mailbox)   == 1);
    CHECK(static_cast<std::uint8_t>(PresentMode::Immediate) == 2);
}
