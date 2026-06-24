# Nothofagus: make the renderer thread-aware

> Implementation plan + status for the nothofagus sim/render thread split (M1–M4).
> Companion to [multithreading_design_discussion.md](multithreading_design_discussion.md)
> (the exploratory design). This file tracks the concrete, as-built work.

## Context

Alice Engine is 100% single-threaded today. The long-term goal is a sim/render
split: the simulation thread mutates the scene and **commits** a frame snapshot;
the render thread draws the **previous** frame's snapshot; the GPU is the implicit
third stage. This decouples render of frame N from sim of frame N+1, and requires
deferred GPU-resource removal so a resource dropped by sim this tick isn't freed
while an in-flight snapshot still references it.

Scoped to **nothofagus only** (no `alice_engine` / PocketPy integration yet),
following [multithreading_design_discussion.md](multithreading_design_discussion.md)
**option (I)** (the double buffer lives inside nothofagus at the scene/draw-data
level), staged as: *seam first → thread flip → committed ImGui → full ImGui input
→ game input on the sim thread*.

## Status

- **M1 — single-threaded seam: DONE & verified** (details + verification below).
- **M2 — flip to two threads: DONE & verified.** Phase A (snapshot hand-off,
  triple buffer, value-only runtime) + Phase B (runtime spawn/despawn via a
  scoped asset mutex + deferred free). New `hello_threaded` demo. Validated:
  TSan/ASan clean in our code, SwiftShader goldens unchanged. No ImGui on the
  threaded path yet.
- **M3 — interactive committed ImGui on the threaded path: DONE & verified.**
  Sim-UI ImGui context + `ImDrawData` clone in the snapshot + thread-local
  `GImGui` + render→sim **mouse** marshalling. The shared-atlas race (ImGui 1.92's
  per-frame dynamic atlas) is serialized by `mImguiMutex` (short ImGui sections
  only; game-sim + sprite render still overlap). `commit(dt, update, uiCallback)`
  splits lock-free game logic from the locked UI frame. New `hello_threaded_imgui`
  demo. Validated: TSan (atlas races gone; only Mesa driver noise), ASan clean,
  goldens unchanged, alice_engine builds.
- **M4 — full keyboard / text / focus input for threaded ImGui: DONE & verified.**
  Render→sim marshalling of the full keyboard state + modifiers + typed
  characters + focus (reusing ImGui_ImplGlfw's keymap on the main context),
  replayed on the sim-UI context; `WantCaptureMouse/Keyboard` back-marshalled
  (`Canvas::imguiWantsMouse()/imguiWantsKeyboard()`); in-process clipboard for
  `InputText` copy/paste; `NavEnableKeyboard`. Demo extended (InputText + Space
  shortcut + capture gating). Validated: TSan 0 races in our code, ASan clean,
  goldens 49/49, alice_engine builds.
- **M5 — gamepad input on the sim thread (game input, not ImGui): NEXT** (plan
  below). Marshal the render `Controller`'s normalized gamepad state → feed a
  sim-side `Controller` the game `update` polls / gets callbacks from. Two-
  controller model (render controller for window/`close`; sim controller for game
  input). nothofagus-only, no ImGui.

## Implemented (on disk, not committed to git) vs Pending

**Implemented & validated — M1–M4:** the full nothofagus-only sim/render thread
split. Single-threaded `run()`/`tick()` are unchanged; the threaded path is an
additive, opt-in API (`beginThreadedSession` / `commit(dt, update[, uiCallback])`
/ `renderFrame` / `isThreadedRunning` / `imguiWantsMouse|Keyboard`; runtime add/
remove via the unified `addBellota` / `removeBellota`, which detect a live threaded
session) exercised by `hello_threaded` (no-ImGui baseline) and
`hello_threaded_imgui` (interactive). Snapshot triple-buffer, deferred GPU free
across the one-frame lag, runtime spawn/despawn, and interactive sim-thread ImGui
(mouse + keyboard + text + clipboard) all work and are TSan/ASan-clean (only
environmental Mesa-driver noise) with goldens unchanged.

**Pending / deferred (none required for the threaded path to work):**
- **Game input on the sim thread** — M1–M4 only marshalled input to the *ImGui*
  context, not to the game's `Controller` on the sim side. **M5 (below) closes
  this for gamepad.** Keyboard/mouse game-input on the sim thread (same
  marshal→sim-`Controller` pattern) and **ImGui gamepad nav** (`NavEnableGamepad`
  + gamepad-key marshalling) remain after M5.
- **Cursor-shape feedback** — sim UI's `GetMouseCursor()` marshalled sim→render +
  `glfwSetCursor` (I-beam over text, resize handles). Output, not input.
- **OS clipboard** across the thread boundary (today: in-process app-local only).
- **Explorers (Dense/Sparse land) on the threaded path** — they mutate pool
  textures; would route through the same asset-mutex/command path. Out of scope so
  far; works single-threaded.
- **Triple-buffer interpolation** — render could interpolate between the two most
  recent snapshots by stable id for extra smoothness (the doc's stretch goal).
- **alice_engine / PocketPy integration** — wiring the engine's Python loop onto
  `commit`/`renderFrame` (Python `update` on the sim thread, etc.). Entire effort
  so far is nothofagus-only by design.
- **Git** — nothing committed; M1–M4 (and `M1_PR.md`) sit in the working tree.

## Architecture invariants (settled, hold across all milestones)

- **Nothofagus stays driven, not driving.** The library spawns no threads. The
  app drives one or two threads and calls into the canvas.
- **One mutable scene + one POD projection.** No second `BellotaContainer`. The
  live `Canvas::bellota(id) -> Bellota&` mutation path is untouched; the render
  side consumes only the POD `RenderSnapshot` and never touches a `Bellota`.
- **Snapshot references resources by id, not GPU handle.** `DrawItem` carries
  `TextureId`/`MeshId`; the render side resolves id → `DTexture`/`DMesh` *after*
  GPU upload (a freshly created texture has no handle at commit time).
- **The seam sits above `ActiveBackend`** — GL and Vulkan inherit the split for
  free; the one invariant is *all GPU create/destroy/draw on the single render
  thread*.
- **RTT passes ride in the same snapshot, no extra-frame deferral.** Within a
  render frame the render side already runs RTT → main → present in order, so RTT
  inherits the same uniform one-frame sim→render latency as sprites. Deferring it
  would make RTT lag its own sprites.

## M1 — single-threaded seam (DONE)

Inverted `FrameRunner::runOneFrame` into **`buildSnapshot()` (produce POD) →
`renderSnapshot()` (consume POD)** on the same thread, behavior-preserving, with
the deferred-free plumbing wired (draining immediately at depth-0).

**What was built:**
- [source/render_snapshot.h](../third_party/nothofagus/source/render_snapshot.h) —
  new POD types `DrawItem` (resolved transform + `TextureId`/`MeshId` + tint /
  opacity / layer / depth), `RttPass` (target + sorted draws), `RenderSnapshot`
  (commitSeq + main draws + RTT passes + clearColor).
- [source/frame_runner.cpp](../third_party/nothofagus/source/frame_runner.cpp) /
  `.h` — `runOneFrame` = `buildSnapshot(...)` → `renderSnapshot(...)`.
  `buildSnapshot` runs input/ImGui-NewFrame/`update`/explorers, detects unused
  resources (enqueues for deferred free), and projects the scene into the reused
  `mSnapshot`. `renderSnapshot` drains frees, uploads, draws the snapshot's RTT +
  main passes, ImGui render, presents. The old `drawBellotaPacks` /
  `makeSpriteDrawParams` / `sortByDepthOffset` were replaced by `drawItems` /
  `makeDrawItem` / `buildMainDraws` over POD. New members: `mSnapshot`,
  `mCommitSeq`, `mLastRenderedSeq`, `mPendingTextureFrees` / `mPendingMeshFrees`,
  `mFramebufferWidth/Height`; `mSortedBellotaPacks` removed.
- [source/asset_registry.cpp](../third_party/nothofagus/source/asset_registry.cpp) /
  `.h` — `collectUnusedTextures` / `collectUnusedMeshes` (detect + monitor-clear,
  no free) and `freeRetiredTexture` / `freeRetiredMesh` (free GPU + container
  erase, no monitor touch). Deferred-free gates on `retireSeq <= lastRenderedSeq`;
  at depth-0 `lastRenderedSeq == commitSeq` so timing matches the old
  `clearUnused*` exactly.

**Verification (passed):**
- nothofagus `linux-debug-glfw-opengl-examples` — clean build.
- nothofagus SwiftShader visual + nonvisual suite — **0 allowed diff pixels**
  (the deterministic proof the seam is behavior-identical: animation, RTT,
  nested-RTT, tilemap all byte-identical).
- parent `alice_engine` `linux-release` — clean build through the changed library.
- engine golden demos — 5/6 byte-exact on GPU; `anim_text` shows a stable
  2/10240-pixel diff = real-GPU-vs-SwiftShader rasterization variance (confirmed
  acceptable), not a regression.

## M2 — flip to two threads (DONE)

> **As built:** Phase A landed as planned; **Phase B used a scoped
> `mThreadedAssetMutex` for runtime spawn/despawn + deferred free, not the
> lock-free resource command queue** the section below describes (the mutex reuses
> AssetRegistry/`syncToGpu` with far less churn; the command queue remains a
> possible future optimization). The deliverables/verification below are the
> original plan, kept as the record.

Goal: a **new** nothofagus-only example, `examples/hello_threaded.cpp`, driven by
two `std::thread`s — a sim thread commits frame N+1 while the main/render thread
draws frame N. **Existing examples are not modified**; they stay on `run()`/
`tick()` as the single-threaded regression baseline.

**Shared invariants for the threaded path:**
- The GL/window context lives on the **main thread**; `renderFrame()` (all GPU
  work + present) runs there. The sim thread issues **zero** GPU calls.
- `run()`/`tick()` are untouched (additive change), so the whole existing example
  + golden suite keeps passing.

### Phase A — snapshot handoff, value-only runtime

At runtime (while both threads run) the sim may only **mutate existing bellota
values** — transform, tint, opacity, layer/animation. All textures, meshes, and
bellotas are created up front, single-threaded, before the threads start ⇒
`mTextures`/`mMeshes` are render-exclusive and `mBellotas` is sim-exclusive at
runtime, so **no container locks** are needed. This proves the handoff + the
real sim/render overlap.

Built:
1. **Triple-buffered snapshot store** —
   [source/snapshot_buffers.h](../third_party/nothofagus/source/snapshot_buffers.h):
   3 `RenderSnapshot` slots + a lock-free mailbox protocol (atomic published
   index/seq). Sim never blocks; render always gets the freshest and drops stale
   intermediates. Each slot reuses its `draws`/`rttPasses` vectors.
2. **Public Canvas API (additive)**: `commit(dt, update)` (sim — projection,
   published into a slot) and `renderFrame(controller)` (main — acquire, upload,
   RTT + main passes, present, drain frees, poll). `beginThreadedSession` /
   `isThreadedRunning` / `close` drive the loops. `run()`/`tick()` unchanged.
3. **No-ImGui present path** in `renderFrame` (empty draw data) — replaced in M3.
4. **`hello_threaded` v1**: a field of sprites animated by the sim thread, drawn
   one frame behind by the main thread.

### Phase B — runtime container mutation (scoped mutex, as built)

> The plan proposed a lock-free **resource command queue + AssetRegistry
> ownership split**; the as-built solution is a narrower **`mThreadedAssetMutex`**
> held only for the short container section (deferred frees + GPU upload + id→
> handle resolve + draw submission), released **before** the vsync swap, while the
> game-sim update and bellota value mutation stay lock-free. Reuses all existing
> AssetRegistry/`syncToGpu`/deferred-free machinery.

Built (originally `Canvas::spawnBellota`/`despawnBellota`; later unified — see the
Step-2 note below — into the regular `addBellota`/`removeBellota`, which detect a
live threaded session):
- runtime add/remove guarded by
  `mThreadedAssetMutex`; removal detects orphaned resources and queues them for
  deferred GPU free (tagged with the commit seq), freed render-side once
  `retireSeq <= lastRenderedSeq` (no use-after-free across the one-frame lag).
- `commitFrame` stamps the commit seq before `update` so removal tags resources
  correctly; `renderFrameThreaded` runs `renderSnapshotContents` under the asset
  mutex, releasing it before the swap.
- **`hello_threaded` escalated**: continuous spawn/despawn burst (sprites + their
  auto-quad meshes) while render lags one frame.

**Validation (A + B):** built `*-glfw-opengl-examples`; ran `hello_threaded`
(smooth, no flicker) under **TSan** (no races in our code — only environmental
Mesa-driver noise; sim thread never a racing party) and **ASan** (sustained
create/destroy churn, no use-after-free); SwiftShader goldens unchanged; parent
`alice_engine` builds.

## M3 — interactive committed ImGui on the threaded path (DONE)

> **As built:** matches the plan below, with one discovery — ImGui 1.92's dynamic
> font atlas is mutated every `NewFrame`, so thread-local `GImGui` alone wasn't
> enough; the two threads' ImGui sections are serialized by `mImguiMutex` (short
> sections; game-sim + sprite render still overlap). The clone uses manual
> `CmdLists` fill (`CloneOutput` returns sealed lists).

Goal: user ImGui widget code runs on the **sim** thread (inside the UI callback,
where the game state it reads/writes lives) and is **interactive** — mouse drives
sliders/buttons/checkboxes — while the **render** thread only issues
`RenderDrawData` from a **deep-cloned `ImDrawData`** carried in the snapshot. All
ImGui logic stays single-threaded (ImGui is not thread-safe; for the future
alice_engine the callbacks are Python on one VM). ImGui is **1.92.8**;
`ImDrawList::CloneOutput()` is the core primitive.

### Thread/context model
- **Sim-UI context** (new): a dedicated `ImGuiContext` sharing the main font atlas
  (manual IO, no platform/renderer `NewFrame`). The sim thread runs `NewFrame` →
  widgets → `Render` on it.
- **Render/backend context**: the original ctor context (renderer bound to it)
  stays on the render thread, used only for `RenderDrawData` of the clone.
- **Thread-local `GImGui`** ([imconfig.h](../third_party/nothofagus/third_party/imgui/imconfig.h),
  definition in `imgui_draw_clone.cpp`) so each thread refers to its own context.
- **`mImguiMutex`** serializes the shared font-atlas access between the two
  threads' ImGui sections (the 1.92 dynamic atlas is mutated every `NewFrame`).

### What was built
1. **`ImDrawData` deep-clone util** —
   [source/imgui_draw_clone.h/.cpp](../third_party/nothofagus/source/imgui_draw_clone.cpp):
   `ClonedImDrawData` owns `ImDrawList*` via `CloneOutput()` and rebuilds an
   `ImDrawData` (manual `CmdLists` fill + count sums). Reused per slot; lists are
   allocated/freed on the sim thread only.
2. **Clone in the snapshot** —
   [render_snapshot.h](../third_party/nothofagus/source/render_snapshot.h) gains a
   forward-declared `std::unique_ptr<ClonedImDrawData> mainUi` (out-of-line dtor in
   `render_snapshot.cpp` keeps `imgui.h` out of the header). Dropped snapshots skip
   a UI frame; nothing leaks.
3. **Mouse marshalling render→sim** — `harvestImguiInput` snapshots the main
   context's processed mouse pos/buttons/wheel + DisplaySize/scale; the sim replays
   via the event API before `NewFrame`.
4. **`commit(dt, update, uiCallback)`** — game `update` runs lock-free (overlaps
   render); the UI callback runs in a `mImguiMutex`-locked section that feeds IO,
   `NewFrame`, runs widgets, `Render`, and clones the draw data.
5. **Render the clone** — `renderFrameThreaded` runs the main empty ImGui frame
   (harvest + atlas upload) and `RenderDrawData` of the clone, both under
   `mImguiMutex`; the sprite render stays under `mThreadedAssetMutex`; the swap is
   outside both locks.

### Constraints (still hold)
- **Fonts baked up-front / render-side.** Runtime font baking from the sim thread
  is out of scope (would need atlas-mutation marshalling).

### Demo
[examples/hello_threaded_imgui.cpp](../third_party/nothofagus/examples/hello_threaded_imgui.cpp)
— interactive panel (sliders / checkbox / button) on the sim thread driving the
simulation; `hello_threaded` stays the no-ImGui baseline.

**Validation:** ran the demo (no asserts); **TSan** — the real atlas races are
gone (only environmental Mesa-driver noise; sim ImGui never a racing party);
**ASan** clean; SwiftShader goldens unchanged; `alice_engine` builds.

## M4 — full input for threaded ImGui (keyboard / text / focus) (DONE)

> **As built:** matches the plan below.

Goal: complete the M3 input path so sim-thread ImGui widgets get **everything** —
keyboard keys, modifiers, text entry, focus — so `InputText`, shortcuts, nav, and
keyboard-driven widgets work on the threaded path.

### Key enabler
The render thread's main ImGui context **already receives all input** every frame:
nothofagus forwards keys via `glfwKeyCallback → ImGui_ImplGlfw_KeyCallback`
([glfw_backend.cpp](../third_party/nothofagus/source/backends/glfw_backend.cpp)),
and `ImGui_ImplGlfw_InitForOpenGL(window, true)` installs the chaining **char**
callback (which `beginSession` does not override). So after the render thread's
`ImGui::NewFrame()` the main context's `io` holds the full keyboard state + typed
characters. M4 **harvests and replays** it on the sim-UI context — reusing
ImGui_ImplGlfw's GLFW→`ImGuiKey` mapping, no new GLFW callbacks. It extends the
existing M3 `harvestImguiInput` → `mThreadedImguiInput` → replay-in-`commitFrame`
channel.

### What was built
1. **Extended `ThreadedImguiInput`** (frame_runner.h): full named-key state array,
   modifier flags, a typed-char buffer, focus.
2. **Harvest (render)**: keyboard named-key range (excluding the mouse-button
   sub-range), modifiers, `io.InputQueueCharacters`, `io.AppFocusLost`.
3. **Replay (sim)**: `AddKeyEvent` per key + `ImGuiMod_*`, `AddInputCharacter`
   (consumed once under the input mutex), `AddFocusEvent`.
4. **`WantCaptureMouse/Keyboard` back-marshal** → `Canvas::imguiWantsMouse()` /
   `imguiWantsKeyboard()`, so the host game `update` can skip world interaction
   while a widget has focus (one frame stale by construction).
5. **In-process clipboard** wired onto the sim-UI context
   (`Platform_Get/SetClipboardTextFn`) for `InputText` copy/paste (GLFW clipboard
   is main-thread-only); `ImGuiConfigFlags_NavEnableKeyboard` enabled.
6. **Demo**: `InputText`, Space shortcut (suppressed while typing), capture-state
   readout, auto-spawn gated on `!imguiWantsMouse()`.

### Known follow-ups (deferred)
- **Cursor shape** (I-beam / resize) — sim→render marshal + `glfwSetCursor`.
- **OS clipboard** across the thread boundary (vs the in-process one).
- **Gamepad nav** — harvest gamepad keys + `NavEnableGamepad`.

**Validation:** ran the demo; **TSan** 0 races in our code (new state lives in the
existing mutex-guarded sections; only Mesa-driver noise); **ASan** clean;
SwiftShader goldens **49/49**; `alice_engine` builds.

> Caveat: automated runs are headless (no synthetic keystrokes), so the harvest+
> replay code executes every frame and `InputText` renders, but real typing /
> shortcut / copy-paste interactivity is best confirmed by running the demo
> interactively.

## M5 — gamepad input on the sim thread (nothofagus, no ImGui) (NEXT)

Goal: a game whose logic runs on the **sim** thread can use gamepad input the
normal nothofagus way — both **polling** (`getGamepadAxis`/`getGamepadButton`/
`isGamepadConnected`/`getConnectedGamepadIds`) and **callbacks**
(`registerGamepadAction`/`registerGamepadAxis`/`registerGamepadConnected`/
`Disconnected`) — from inside `commit`'s `update`. Today gamepad is polled and
dispatched on the **render** thread only (`renderFrameThreaded` →
`mWindow->endFrame(controller, …)` + `processInputs`); the sim `update` never sees
it. No ImGui involvement.

### Why a second Controller

`Controller` is not thread-safe and the window backend polls it on the render
thread (and `close()` must stay render-thread — GLFW is main-thread-only). So the
threaded path uses **two controllers**:
- **render controller** (already passed to `beginThreadedSession`/`renderFrame`):
  the raw window-input sink polled by `endFrame`; keep render-thread handling here
  (e.g. `Escape → close()`).
- **sim controller** (new): the game's controller, consumed on the sim thread —
  the game registers gamepad callbacks on it and/or polls it inside `update`.

### Mechanism — marshal render → sim, then replay (reuses all of `Controller`)

The render controller already holds the backend-**normalized** gamepad state
(deadzone / Y-invert / trigger remap done in
[GlfwBackend::endFrame](../third_party/nothofagus/source/backends/glfw_backend.cpp)
before `updateGamepadAxis`), queryable via its getters; and `Controller`'s
`activateGamepadButton` (sets state + queues the edge) / `updateGamepadAxis` (sets
value + fires axis cb on change) / `gamepadConnected`/`Disconnected` are the exact
feed primitives the backend uses. So:

1. **Harvest (render, after `endFrame`)** — new `harvestGamepadInput()`: read the
   render controller into a POD `GamepadSnapshot` (per id `0..GLFW_JOYSTICK_LAST`:
   `connected` + `bool buttons[15]` + `float axes[6]`, iterating the
   `GamepadButton`/`GamepadAxis` enums), stored under a new `mThreadedGamepadMutex`.
2. **Feed (sim, in `commitFrame` before `update`)** — copy the snapshot under the
   mutex, then for each id diff it against the **sim controller's** current state
   and replay: changed button → `activateGamepadButton({id,btn,Press|Release})`;
   each axis → `updateGamepadAxis(id,axis,value)`; connect/disconnect by comparing
   the connected sets → `gamepadConnected`/`Disconnected`; finally
   `simController.processInputs()` to dispatch the queued button edges. The game's
   `update` then polls / has received callbacks on the sim controller.

Backend-agnostic ~30-line replay; no GLFW on the sim thread, no new gamepad logic,
no change to `Controller`.

### API

- **`Canvas::commit(dt, update, Controller& simController)`** (new overload,
  symmetric with `renderFrame(controller)`): `FrameRunner::commitFrame` feeds the
  given sim controller from the latest gamepad snapshot *before* invoking `update`.
  Existing `commit(dt, update)` and `commit(dt, update, uiCallback)` stay; a
  `commit(dt, update, controller, uiCallback)` 4-arg form can be added for the
  combined gamepad+ImGui case (not needed for this milestone).
- No new public types — the game uses the existing `Controller` gamepad API.

### Threading correctness

`mThreadedGamepadState` is the only shared state (mutex-guarded; tiny critical
sections). The render controller stays render-thread-exclusive; the sim controller
stays sim-thread-exclusive (fed + polled only there). No new overlap cost — the
feed is part of the existing lock-free `commit` game phase.

### Demo / Verification

New `examples/hello_threaded_gamepad.cpp`: render controller with `Escape →
close`; a sim controller whose left-stick axes + face buttons move/color a sprite,
polled inside `update` (mirrors `examples/test_gamepad.cpp` on the threaded path).
Build + run (responds with a pad connected; clean no-op without one — harvest/feed
run every frame regardless); **TSan/ASan** clean (only `mThreadedGamepadState`
shared + mutex-guarded; both controllers single-thread-exclusive); SwiftShader
goldens unchanged; parent `alice_engine` builds.

> Note: real gamepad interaction needs hardware (same as `test_gamepad`); the
> automated checks confirm the marshal/feed machinery runs clean and race-free,
> not actual stick input.

## Critical files (as built)

- `third_party/nothofagus/source/render_snapshot.h` / `.cpp` — POD draw list +
  `mainUi` clone handle (out-of-line dtor).
- `third_party/nothofagus/source/snapshot_buffers.h` — triple buffer + mailbox.
- `third_party/nothofagus/source/imgui_draw_clone.h` / `.cpp` — `ClonedImDrawData`
  + thread-local `GImGui` definition.
- `third_party/nothofagus/third_party/imgui/imconfig.h` — thread-local `GImGui`.
- `third_party/nothofagus/source/frame_runner.h` / `.cpp` — `buildSnapshot` /
  `renderSnapshot[Contents]`, threaded `commitFrame` / `renderFrameThreaded`,
  sim-UI context, input marshalling (mouse + keyboard/text/focus), the two mutexes
  (`mThreadedAssetMutex`, `mImguiMutex`), deferred-free, `WantCapture` back-marshal.
- `third_party/nothofagus/include/canvas.h` + `source/canvas.cpp` — threaded API
  (`beginThreadedSession`, `commit[+uiCallback]`, `renderFrame`, `isThreadedRunning`,
  `imguiWantsMouse`/`imguiWantsKeyboard`); runtime add/remove via the unified
  `addBellota`/`removeBellota`.
- `third_party/nothofagus/source/asset_registry.*` — `collectUnused*` /
  `freeRetired*` (deferred free).
- `third_party/nothofagus/examples/hello_threaded.cpp` — no-ImGui threaded demo.
- `third_party/nothofagus/examples/hello_threaded_imgui.cpp` — interactive demo.
- `third_party/nothofagus/examples/CMakeLists.txt`, `CMakeLists.txt` — build entries.

### M5 (planned, not yet implemented)

- `third_party/nothofagus/source/frame_runner.h` / `.cpp` — `GamepadSnapshot` +
  `mThreadedGamepadState` / `mThreadedGamepadMutex`, `harvestGamepadInput()`
  (render), and the sim-controller feed in `commitFrame`. No change to `Controller`
  or the window backends.
- `third_party/nothofagus/include/canvas.h` + `source/canvas.cpp` — new
  `commit(dt, update, Controller& simController)` overload.
- `third_party/nothofagus/examples/hello_threaded_gamepad.cpp` +
  `examples/CMakeLists.txt` — new threaded gamepad demo.
