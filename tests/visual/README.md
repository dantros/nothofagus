# Visual tests

Catch2 tests that need a render backend. Built by `NOTHOFAGUS_BUILD_TESTS_VISUAL=ON`.

- `rendering_tests` — golden-image comparisons (see the main CLAUDE.md "Tests" section
  and "Golden images" / "SwiftShader" subsections).
- `present_mode_pixel_tests` / `present_mode_sync_tests` — present-mode regression
  tests (see below).

## Present-mode regression tests (local / windowed)

These guard the selectable present mode (`PresentMode::{Fifo,Mailbox,Immediate}`).
They are **only meaningful on a windowed Vulkan build with a display server** — that
is where the swapchain present mode actually takes effect. Under the headless backend
(`NOTHOFAGUS_HEADLESS_VULKAN`, the CI lane) present mode is a no-op, so the pixel test
passes trivially and the sync test SKIPs / is trivially invariant. They are therefore
**dev/local** checks, not part of the CI golden lane.

- **`present_mode_pixel_tests`** (Finding 1) — asserts the screenshot is byte-identical
  across all three present modes and run-to-run. No validation layer needed.
- **`present_mode_sync_tests`** (Finding 2) — asserts the set of distinct Vulkan
  validation IDs (`VUID-*` / `SYNC-HAZARD-*`) emitted while rendering/presenting is
  identical across present modes (mode-invariance, not "zero hazards" — there is a
  known pre-existing baseline). Captured in-process via `setVulkanValidationCallback`
  (`include/vulkan_validation.h`). **Requires the validation layer**; it `SKIP`s when
  none is loaded. The test requests synchronization validation itself.

They are split into two executables on purpose: the engine creates one Vulkan instance
per `Canvas`, and vk-bootstrap caches the debug-messenger function pointer per process,
so the messenger-using sync test must run in its own process.

### How to run

Build a windowed Vulkan test preset and run on a machine with a display:

```bash
cmake --preset linux-release-sdl3-vulkan-tests
cmake --build build/linux-release-sdl3-vulkan-tests \
  --target present_mode_pixel_tests present_mode_sync_tests --parallel

BUILD=build/linux-release-sdl3-vulkan-tests/tests/visual

# Finding 1 — no validation layer needed:
$BUILD/present_mode_pixel_tests

# Finding 2 — point the loader at the Vulkan SDK's validation layer (sourcing the
# SDK's setup-env.sh sets VK_LAYER_PATH for you). It SKIPs if the layer isn't found.
VK_LAYER_PATH=/path/to/vulkansdk/x86_64/share/vulkan/explicit_layer.d \
  $BUILD/present_mode_sync_tests
```

Or via CTest from the build dir (set `VK_LAYER_PATH` first for the sync test):

```bash
cd build/linux-release-sdl3-vulkan-tests && ctest -R present --output-on-failure
```

The pure-logic mapping test (`presentModeToSwapInterval`) lives in the **nonvisual**
group (`present_mode_tests`) and needs no GPU or display — it runs in any lane.
