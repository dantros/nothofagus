# Threaded mode — sim/render split (status)

> As-built status of nothofagus's optional two-thread sim/render path. `run()`/`tick()`
> remain the unchanged single-threaded default; the threaded path is additive and opt-in.
> Branch: `thread_aware_phase2_unification` (first commit `71805e8`).

## What it is

A sim thread mutates the scene and **commits** a POD frame snapshot; the render thread
draws the **previous** snapshot; the GPU is the implicit third stage. Render of frame N
overlaps sim of frame N+1. GPU-resource frees are **deferred** across the one-frame lag so
a resource dropped by sim this tick isn't freed while an in-flight snapshot still uses it.

Opt-in API (nothofagus spawns no threads unless you use a `run(...)` convenience overload):
`beginThreadedSession` / `commit(dt, update[, uiCallback][, simController])` / `renderFrame`
/ `isThreadedRunning`, plus the `run(update, ui[, simController, renderController])`
convenience overloads that own the sim thread for you.

## Architecture invariants (hold across the whole path)

- **Nothofagus stays driven, not driving** — the library doesn't spawn threads except in
  the `run(...)` convenience; the app can drive both loops itself.
- **One mutable scene + one POD projection** — no second bellota container. The live
  `bellota(id) -> Bellota&` mutation path is untouched; the render side consumes only the
  POD `RenderSnapshot` and never touches a `Bellota`.
- **Snapshot references resources by id, not GPU handle** — `DrawItem` carries
  `TextureId`/`MeshId`; the render side resolves id → GPU handle *after* upload.
- **The seam sits above `ActiveBackend`** — GL and Vulkan inherit the split; the invariant
  is *all GPU create/destroy/draw on the single render thread*.
- **RTT passes ride in the same snapshot** — no extra-frame deferral; uniform one-frame lag.

## Done (verified in source)

### Foundation
- **Unified frame path** — one `produce(FrameMode)` + one `consume(FrameMode)`
  (`Single` = run/tick, `Threaded` = commit/renderFrame). Behavior-preserving.
- **Triple-buffered snapshot store** (`snapshot_buffers.h`) — 3 slots + lock-free mailbox;
  sim never blocks, render always gets the freshest, stale intermediates dropped.
- **Deferred GPU free** — `collectUnused*` / `freeRetired*` in `asset_registry.*`, gated on
  `retireSeq <= lastRenderedSeq`; render-target deferred-free queue drains + releases the
  per-RTT ImGui context.
- **Unified `addBellota`/`removeBellota`** — one mode-aware add/remove safe in both
  single-threaded and live-threaded contexts (old `spawnBellota`/`despawnBellota` deleted).

### Game input on the sim thread (two-controller model)
- **Gamepad** — `commit(dt, update, Controller& simController)`; render-side `GamepadSnapshot`
  harvest → POD → sim-side feed/replay against the sim controller.
- **Keyboard + mouse** — `Controller` gained `isKeyDown`/`isMouseButtonDown` + a scroll
  accumulator; `GameInputSnapshot` harvest/feed. The 4-arg `commit` feeds gamepad + keyboard
  + mouse before `update`.
- **render controller vs sim controller** — render controller owns window input + `close`
  (GLFW is main-thread-only); sim controller is the game's, consumed only on the sim thread.

### Interactive ImGui on the sim thread
- **Sim-UI context + `ImDrawData` deep-clone** carried in the snapshot; thread-local `GImGui`;
  the shared 1.92 dynamic font atlas is serialized by `mImguiMutex` (short sections; game-sim
  + sprite render still overlap). `commit(dt, update, uiCallback)` splits lock-free game logic
  from the locked UI frame.
- **Full input marshalling** render→sim — mouse, full keyboard state + modifiers + typed
  characters + focus; `WantCaptureMouse/Keyboard` back-marshalled to
  `imguiWantsMouse()`/`imguiWantsKeyboard()`.
- **Cursor-shape feedback** — sim `GetMouseCursor()` → atomic → render `SetMouseCursor`.
- **ImGui gamepad nav** — `NavEnableGamepad` on both contexts; digital D-pad/face-button plus
  **analog** stick nav (`keyAnalog[]` side-channel + `AddKeyAnalogEvent`).
- **OS clipboard** across the thread boundary — `WindowBackend get/setClipboardText`
  (GLFW/SDL3/headless); mutex-guarded marshal, writes immediate, OS reads throttled ~0.25 s.
- **Stats overlay** drawn into the sim-UI clone (so it survives even when the app commits its
  own ImGui); render fps/ms marshalled to sim via atomics.

### Runtime asset mutation from the sim thread
- **Explorers (Dense/Sparse land)** run in `produce(Threaded)` under the asset mutex.
- **Create/destroy of textures / meshes / render targets** from `commit`'s update — mode-aware
  asset wrappers (lazy adds + deferred removes), tolerant retire, RT deferred-free queue.
- **Threaded-safe `setScreenSize`** — `mScreenSize` is `std::atomic<ScreenSize>`; resizing the
  logical canvas from the sim update is race-free and rebuilds the explorer pool next commit.
- **`screenSize` carried in `RenderSnapshot`** — each snapshot renders in the size it was built
  for; no 1-frame letterbox transient on a threaded `setScreenSize`.
- **Threaded-safe `markTextureAsDirty` / `setTextureMin|MagFilter`** — now pure-CPU dirty flags
  (`mContentDirty` / `mFilterDirty`) consumed by `syncToGpu` on the render thread.

### Diegetic ImGui + registered ImGui images on the sim thread
- **Registered ImGui images** (`registerImguiImage`/`imguiImage`/`updateImguiImage`) — the
  `ImguiImageManager` is threaded through the produce/consume arms; sim-side registry access is
  serialized under the asset mutex and GPU frees deferred via a two-phase retire queue.
- **Diegetic `renderImguiTo`** — `ImguiRttManager` split into sim-side `produceClones` (run each
  callback on its secondary context, deep-clone into `RenderSnapshot::rttUi`) + render-side
  `replayClones` (backend init + `RenderDrawData`, no user code). Render consume takes the ImGui
  mutex outer of the asset mutex so the RTT-clone atlas access can't deadlock with the sim.
  See [THREADED_DIEGETIC_IMGUI.md](THREADED_DIEGETIC_IMGUI.md) for the full as-built.

## Pending — port all demos to the threaded path (main remaining work)

The threaded core is complete; the outstanding effort is migrating the example demos so the
suite exercises the threaded path as the default. The `run(update, ui[, simController,
renderController])` convenience makes each migration mechanical: split the single `run` lambda
into a game-logic `update` + an ImGui `ui`, and split input into `simController` (game) vs
`renderController` (window/`close`).

**Ported (6 / 29):** `hello_threaded`, `hello_threaded_imgui`, `hello_threaded_gamepad`,
`hello_threaded_dense_land` (dedicated); `hello_nothofagus`, `test_keyboard` (migrated in place).

**Remaining (23):** hello_animation, hello_animation_state_machine, hello_custom_font,
hello_dense_land, hello_direct_texture, hello_dpi_scaling, hello_headless, hello_imgui_image_registry,
hello_imgui_overlay, hello_imgui_rtt, hello_imgui_visual, hello_layers, hello_markdown, hello_mesh,
hello_nested_render_targets, hello_render_to_texture, hello_screenshot, hello_sparse_land,
hello_text, hello_tilemap, hello_tint, test_create_destroy, test_gamepad.

Per-demo caveats:
- **hello_headless** uses `tick()` (deliberate single-step/headless harness) — a threaded port
  may not be meaningful; keep as the single-threaded/manual-tick reference.
- **hello_screenshot** stays on the deprecated single-thread `run(update, Controller&)`: the
  scheduled screenshot request/result handoff crosses the sim/render boundary unsynchronized
  (`requestScreenshot` arms sim-side, `finishScreenshot` writes render-side). This is a *separate*
  gap from the ImGui features and is the only remaining deferred demo.
- **hello_imgui_rtt / hello_dpi_scaling** (diegetic `renderImguiTo`) and **hello_imgui_visual /
  hello_imgui_image_registry / hello_markdown** (registered ImGui images) are **now threaded** —
  all five run on `run(update, ui)` (see [THREADED_DIEGETIC_IMGUI.md](THREADED_DIEGETIC_IMGUI.md)).

## Out of scope

- **Triple-buffer interpolation** — render interpolating between the two most recent snapshots
  by stable id for extra smoothness. Deferred: needs per-drawable stable IDs in `DrawItem`,
  previous-snapshot retention, ID-matching, and a render-time alpha.
- **Integrating a nothofagus consumer's game/scripting loop onto `commit`/`renderFrame`** —
  out of nothofagus scope by design; not tracked here.
- **Full driver collapse** (`run`/`tick` → only `commit`/`renderFrame`) — deferred by choice;
  it would force every ImGui-in-`update` demo onto the threaded ImGui model. The in-place demo
  migration above is the incremental path toward this, without forcing it.

## Critical files

- `source/render_snapshot.h`/`.cpp` — POD draw list + `screenSize` + `mainUi` clone handle.
- `source/snapshot_buffers.h` — triple buffer + lock-free mailbox.
- `source/frame_runner.h`/`.cpp` — `produce`/`consume`, threaded `commitFrame`/`renderFrameThreaded`,
  `runThreaded`, sim-UI context, input marshalling, the asset + ImGui mutexes, deferred frees.
- `source/imgui_draw_clone.h`/`.cpp` — `ClonedImDrawData` + thread-local `GImGui`.
- `source/asset_registry.*` — `collectUnused*` / `freeRetired*`.
- `include/canvas.h` + `source/canvas.cpp` — threaded API + `run(...)` convenience overloads.
- `examples/hello_threaded*.cpp` — threaded demos.

## Verification recipe (every change)

- Cross-backend builds: `linux-release-glfw-opengl-examples`, `-glfw-vulkan-examples`, `-sdl3-vulkan-examples`.
- SwiftShader goldens unchanged: `linux-release-headless-vulkan-tests` +
  `NOTHOFAGUS_RENDER_BACKEND=swiftshader .../rendering_tests`.
- TSan + ASan on the threaded demos under `xvfb-run` — expect only environmental Mesa/glib noise,
  0 conflicts in our code, 0 ASan/UBSan errors.
- Any new shared state must be mutex- or atomic-guarded like the existing marshal channels.
