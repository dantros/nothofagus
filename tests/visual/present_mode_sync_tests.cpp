// Finding 2 — present mode introduces no NEW Vulkan synchronization hazards.
//
// The set of distinct validation IDs (VUID-* / SYNC-HAZARD-*) emitted while
// rendering + presenting + screenshotting is identical across the three present
// modes. A mode-specific extra ID would mean that present mode introduced a new
// hazard. The assertion is mode-INVARIANCE (not "zero hazards") because there is a
// known pre-existing baseline (depth WAW, write-after-present, screenshot
// TRANSFER_SRC) that this commit does not change.
//
// Requires the validation layer to be loaded (e.g. the Vulkan SDK on VK_LAYER_PATH);
// synchronization validation is requested by the test itself. SKIPs when no
// validation messages are captured (layer not present). Captures messages in-process
// via Canvas's setVulkanValidationCallback hook (include/vulkan_validation.h).
//
// Intended for LOCAL / windowed runs. See tests/visual/README.md.

#include <catch2/catch_test_macros.hpp>

#include "present_mode_test_common.h"

#include <vulkan_validation.h>

#include <array>
#include <cstdlib>
#include <set>
#include <string>

namespace
{

void setEnvVar(const char* name, const char* value)
{
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

// Extract distinct validation identifiers ("VUID-..." / "SYNC-HAZARD-...") from a
// message so the assertion is robust to message wording, counts, and ordering. The
// engine hook prefixes each message with the debug-utils pMessageIdName, which is
// where these tokens live.
void collectIds(const std::string& message, std::set<std::string>& out)
{
    static const std::array<const char*, 2> prefixes{"VUID-", "SYNC-HAZARD-"};
    for (const char* prefix : prefixes)
    {
        const std::string needle = prefix;
        std::string::size_type pos = 0;
        while ((pos = message.find(needle, pos)) != std::string::npos)
        {
            std::string::size_type end = pos;
            while (end < message.size())
            {
                const char c = message[end];
                const bool ident = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                   (c >= '0' && c <= '9') || c == '-';
                if (!ident) break;
                ++end;
            }
            out.insert(message.substr(pos, end - pos));
            pos = end;
        }
    }
}

std::set<std::string> validationIdsForMode(Nothofagus::PresentMode mode)
{
    std::set<std::string> ids;
    Nothofagus::setVulkanValidationCallback(
        [&ids](const std::string& message) { collectIds(message, ids); });

    {
        // Render + present several frames (acquire/submit/present + per-image
        // semaphore cycling), then a screenshot (present->screenshot path) — all
        // under this present mode, while the callback is active. Canvas tears down at
        // end of scope.
        Nothofagus::Canvas canvas(
            {15, 10}, "present_mode_test", {0.0f, 0.0f, 0.0f}, 1, 14,
            /*headless=*/true, mode);
        PresentModeTest::buildSceneAndTick(canvas, /*ticks=*/24);
        (void)canvas.takeScreenshot();
    }

    Nothofagus::setVulkanValidationCallback(nullptr);
    return ids;
}

} // namespace

TEST_CASE("present mode introduces no new validation hazards (Finding 2)", "[present_mode][validation]")
{
    // Turn on synchronization validation for the instances this test creates. The
    // loader re-reads this per vkCreateInstance. Requires the validation layer to be
    // discoverable (e.g. VK_LAYER_PATH from the Vulkan SDK).
    setEnvVar("VK_LAYER_ENABLES", "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT");

    std::array<std::set<std::string>, PresentModeTest::kAllModes.size()> idsByMode;
    std::set<std::string> unionIds;
    for (std::size_t i = 0; i < PresentModeTest::kAllModes.size(); ++i)
    {
        idsByMode[i] = validationIdsForMode(PresentModeTest::kAllModes[i]);
        unionIds.insert(idsByMode[i].begin(), idsByMode[i].end());
    }

    if (unionIds.empty())
        SKIP("No Vulkan validation messages captured — validation layer not loaded. "
             "Run with the Vulkan SDK on VK_LAYER_PATH.");

    // The set of distinct validation IDs must be identical across present modes.
    for (std::size_t i = 1; i < PresentModeTest::kAllModes.size(); ++i)
    {
        INFO("comparing " << PresentModeTest::modeName(PresentModeTest::kAllModes[0])
             << " vs " << PresentModeTest::modeName(PresentModeTest::kAllModes[i]));
        CHECK(idsByMode[i] == idsByMode[0]);
    }
}
