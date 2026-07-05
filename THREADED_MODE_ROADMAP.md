# Threaded mode — completion roadmap

> Status of the sim/render threaded path (Option A). What's done, what's left to reach full
> feature parity with the single-threaded path, and related work tracked separately.
> Branch: `thread_aware_phase2_unification`. Companion: `nothofagus_threading_plan.md`,
> `multithreading_design_discussion.md`.
>
> `main` was merged in (commit `7a10d63`): it brought the **Vulkan present-mode fix** (now done — see
> group D) and the **ImGui image registry** (`registerImage`/`drawImage`, `imgui_image_manager`); the
> threaded `produce`/`consume` integrate both (the per-frame ImGui-image pre-pass is single-thread
> only, null-guarded on the threaded path).

## Foundation (done & committed)

- **Step 1 — unify frame paths** (`c00b634`): one `produce(FrameMode)` + one `consume(FrameMode)`
  (`Single` = run/tick, `Threaded` = commit/renderFrame). Behavior-preserving.
- **M5 — gamepad on the sim thread** (`4966785`): two-controller model (render + sim) via
  harvest→POD→feed.
- **Stats + PerformanceMonitor dt on the threaded path** (`13c7402`, `9e44844`, `e16648a`).
- **Step 2 (scoped) — bellota mutation unify** (`645c278`): one mode-aware `addBellota`/`removeBellota`;
  deleted `spawnBellota`/`despawnBellota`.

## A. Feature-parity gaps (single-threaded can; threaded couldn't)

1. **Keyboard + mouse game input on the sim thread** — ✅ DONE (`6b5d32a`). `Controller` gained
   `isKeyDown`/`isMouseButtonDown` + a scroll accumulator; `GameInputSnapshot` harvest/feed; the
   `commit(dt, update, Controller&)` overload now feeds gamepad + keyboard + mouse.
2. **Explorers (Dense/Sparse land) on the threaded path** — ✅ DONE (`4174e3e`). `updateExplorers`
   runs in `produce(Threaded)` under the asset mutex; `Canvas*` captured in `beginThreadedSession`.
3. **Runtime create/destroy of textures / meshes / render targets** — ✅ DONE (`d45063c`). Mode-aware
   asset wrappers (lazy adds + deferred removes), tolerant `retireTexture`/`retireMesh`, a new
   render-target deferred-free queue (drain also releases the per-RTT ImGui context).
4. **Threaded-safe `setScreenSize` (explorer pool resize during a live session)** — ✅ DONE.
   `mScreenSize` is now `std::atomic<ScreenSize>` (lock-free 8-byte); `screenSize()` returns by
   value, every reader loads atomically, `setScreenSize` stores. Resizing the logical canvas from
   the sim `update` is race-free and rebuilds the explorer pool next commit. Verified:
   `hello_threaded_dense_land` toggles `setScreenSize` every ~2 s — TSan 0 conflicts in our code
   (no `mScreenSize` race), ASan/UBSan clean. Remaining cosmetic nuance: a 1-frame letterbox
   transient on resize (render uses live size while drawing the previous snapshot's pool); fully
   removing it = carry `screenSize` in `RenderSnapshot` (optional, see C).

## B. ImGui parity on the threaded path — ✅ DONE

5. **Cursor-shape feedback** — ✅ DONE (`0fabc0f`). Sim `ImGui::GetMouseCursor()` → `mThreadedCursor`
   (atomic) → render `ImGui::SetMouseCursor` before the main-context `NewFrame`, so the platform
   backend's existing `UpdateMouseCursor` applies it. No backend changes; render-backend-agnostic.
6. **ImGui gamepad nav** — ✅ DONE (`5ad2d95`). `NavEnableGamepad` on the main context (so the
   platform backend populates the `ImGuiKey_Gamepad*` keys already harvested+replayed) + on the
   sim-UI context (+`HasGamepad`). Threaded-only. Digital D-pad/face-button nav; **analog stick nav
   not yet marshalled** (bool-only harvest) — follow-up needs a float side-channel.
7. **OS clipboard across the thread boundary** — ✅ DONE (`43879a7`). New `WindowBackend`
   `get/setClipboardText` (GLFW/SDL3/headless); sim-UI clipboard callbacks read/write a mutex-guarded
   marshal that the render thread syncs with the OS (writes immediate, OS reads throttled ~0.25 s).

## C. Polish / stretch — 🔧 C8/C10/C11 DONE; C9 deferred

8. **Stats overlay when a sim-UI clone IS present** — ✅ DONE. The stats window is now drawn into the
   sim-UI frame (in `produce(Threaded)`, after the user `uiCallback`, before `Render`) instead of the
   render-thread main context, so it ends up in the cloned draw data that gets presented even when
   the app commits its own ImGui. The render cadence is marshalled to the sim via `mRenderFps`/
   `mRenderMs` atomics; a sim-side `PerformanceMonitor` (fed by an accumulated commit clock) gives
   the sim rate. Overlay shows render fps/ms next to the sim fps. Demo: `hello_threaded_imgui` now
   sets `canvas.stats() = true` (a `uiCallback` app — the case that previously hid stats).
9. **Triple-buffer interpolation** — ⏸ DEFERRED. Render could interpolate between the two most recent
   snapshots by stable id for extra smoothness. Larger change: needs per-drawable stable IDs in
   `DrawItem` (today only `TextureId`/`MeshId`), previous-snapshot retention (consume reads only the
   latest `readSlot()`), ID-matching, and a render-time alpha. C10's per-snapshot `screenSize` is a
   prerequisite it can build on.
10. **Carry `screenSize` in `RenderSnapshot`** — ✅ DONE. `RenderSnapshot` gained a `ScreenSize
    screenSize` field, stamped in both `produce` arms; `consume(Threaded)`/`renderSnapshotContents`
    use it for the viewport / world transform / `endFrame`, so the render draws each snapshot's pool
    in the size it was built for — no 1-frame letterbox transient on a threaded `setScreenSize`.
    Priming/un-stamped slots ({0,0}) fall back to the live atomic to avoid a 0-aspect divide
    (a real UBSan bug caught in verification). Single mode always stamps a valid size, so the
    fallback is a no-op there.
11. **Analog gamepad nav on the threaded path** — ✅ DONE. `ThreadedImguiInput` gained a
    `float keyAnalog[]` side-channel alongside `keyDown`; `harvestImguiInput` captures
    `io.KeysData[index].AnalogValue` (the public per-key array), and the replay routes the analog
    gamepad keys (`GamepadL2`/`R2`, `GamepadL/RStick*` — via the `isAnalogNavKey` helper) through
    `AddKeyAnalogEvent` instead of `AddKeyEvent`. ImGui's smooth gamepad nav (continuous
    scroll/tween) now survives the marshal, matching single-threaded. Render-backend-independent,
    works on GLFW and SDL3.

## D. Related — tracked separately (not part of "completing threaded mode")

- **Vulkan present-mode perf** (windowed FIFO → ~45 fps on a compositor) — ✅ DONE, landed in `main`
  and merged here. A `PresentMode` enum ([include/present_mode.h](include/present_mode.h),
  default `Mailbox`) is a `Canvas` ctor arg; it selects the Vulkan present mode and the OpenGL swap
  interval (`presentModeToSwapInterval`). Mailbox is vsync'd + triple-buffered (no tearing,
  compositor-friendly) and avoids the ~45 fps FIFO pacing. (The old `VULKAN_PRESENT_MODE_FIX.md`
  planning doc is superseded and was removed.)
- **Full Option A driver collapse** (`run`/`tick` → only `commit`/`renderFrame`) — deferred by
  choice; would force the ~15 ImGui-in-`update` single-threaded examples onto the threaded ImGui model.
- **markTextureAsDirty / setTextureMin|MagFilter threaded-safety** — ✅ DONE (`ccc11a3`). The three
  Canvas mutators no longer do immediate GPU work: two pure-CPU `TexturePack` flags (`mContentDirty`
  for re-upload, `mFilterDirty` for min/mag filters) are consumed by `syncToGpu` on the render thread;
  `AssetRegistry` setters became pure-CPU and new `FrameRunner` mode-aware wrappers take the asset
  lock when threaded (same pattern as `setTexture`). Single-threaded behavior preserved. Demo:
  `hello_threaded` flips a Direct texture's filter + marks it dirty from the sim update.
- **alice_engine / PocketPy integration** — wiring the engine's Python loop onto `commit`/`renderFrame`;
  out of nothofagus scope by design.

## Verification recipe (every change)

- Cross-backend builds: `linux-release-glfw-opengl-examples`, `-glfw-vulkan-examples`, `-sdl3-vulkan-examples`.
- SwiftShader goldens unchanged: `linux-release-headless-vulkan-tests` +
  `NOTHOFAGUS_RENDER_BACKEND=swiftshader .../rendering_tests`.
- TSan + ASan on the threaded demos (`hello_threaded`, `_imgui`, `_gamepad`, `_dense_land`) under
  `xvfb-run` — expect only Mesa/glib environmental noise, 0 conflicts in our code, 0 ASan/UBSan errors.
- Any new shared state must be mutex-guarded like `mThreadedGamepadState` / `mThreadedGameInputState`.
