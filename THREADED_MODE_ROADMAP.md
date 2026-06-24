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

## B. ImGui parity on the threaded path — 🔧 OPEN

5. **Cursor-shape feedback** — marshal sim `ImGui::GetMouseCursor()` → render `glfwSetCursor`/SDL
   equivalent (I-beam over text, resize handles). Output, sim→render (reverse of the input harvest).
6. **ImGui gamepad nav** — only `NavEnableKeyboard` is set on the sim-UI context; add
   `NavEnableGamepad` + marshal the gamepad ImGui-keys into it.
7. **OS clipboard across the thread boundary** — today the sim-UI context uses an in-process
   clipboard; bridge to the real OS clipboard (GLFW/SDL, main-thread-only) across the boundary.

## C. Polish / stretch — 🔧 OPEN

8. **Stats overlay when a sim-UI clone IS present** — stats render on the main context, which is only
   shown when the app commits no `uiCallback` (e.g. the gamepad demo). Apps with sim ImGui need
   stats injected into the clone (sim side).
9. **Triple-buffer interpolation** — render could interpolate between the two most recent snapshots
   by stable id for extra smoothness.
10. **Carry `screenSize` in `RenderSnapshot`** — removes the 1-frame letterbox transient on a
    threaded `setScreenSize` (the render would use the snapshot's size, matching its pool, instead of
    the live size). Stamp it in `produce`; `consume` uses `snapshot.screenSize` for the viewport /
    world transform / `endFrame`. A4 is already race-safe without this.

## D. Related — tracked separately (not part of "completing threaded mode")

- **Vulkan present-mode perf** (windowed FIFO → ~45 fps on a compositor) — ✅ DONE, landed in `main`
  and merged here. A `PresentMode` enum ([include/present_mode.h](include/present_mode.h),
  default `Mailbox`) is a `Canvas` ctor arg; it selects the Vulkan present mode and the OpenGL swap
  interval (`presentModeToSwapInterval`). Mailbox is vsync'd + triple-buffered (no tearing,
  compositor-friendly) and avoids the ~45 fps FIFO pacing. (The old `VULKAN_PRESENT_MODE_FIX.md`
  planning doc is superseded and was removed.)
- **Full Option A driver collapse** (`run`/`tick` → only `commit`/`renderFrame`) — deferred by
  choice; would force the ~15 ImGui-in-`update` single-threaded examples onto the threaded ImGui model.
- **markTextureAsDirty / setTextureMin|MagFilter threaded-safety** — currently single/main-thread-only
  (immediate GPU work); deferred-GPU versions are a follow-up if needed.
- **alice_engine / PocketPy integration** — wiring the engine's Python loop onto `commit`/`renderFrame`;
  out of nothofagus scope by design.

## Verification recipe (every change)

- Cross-backend builds: `linux-release-glfw-opengl-examples`, `-glfw-vulkan-examples`, `-sdl3-vulkan-examples`.
- SwiftShader goldens unchanged: `linux-release-headless-vulkan-tests` +
  `NOTHOFAGUS_RENDER_BACKEND=swiftshader .../rendering_tests`.
- TSan + ASan on the threaded demos (`hello_threaded`, `_imgui`, `_gamepad`, `_dense_land`) under
  `xvfb-run` — expect only Mesa/glib environmental noise, 0 conflicts in our code, 0 ASan/UBSan errors.
- Any new shared state must be mutex-guarded like `mThreadedGamepadState` / `mThreadedGameInputState`.
