# Threaded mode — the diegetic-ImGui / registered-image gap

> Companion to [THREADED_MODE.md](THREADED_MODE.md). That doc covers the *main-canvas* ImGui
> path, which is fully threaded (sim-UI context + `ImDrawData` deep-clone in the snapshot).
> **This doc scopes the two ImGui features that are NOT yet threaded**, so their support can be
> designed and implemented separately. Until then, examples using them stay on the
> single-threaded `run(update, Controller&)` path.

## TL;DR — threaded status

| Feature | Public API | Threaded status | Examples |
|---|---|---|---|
| **Diegetic ImGui in an RTT** | `Canvas::renderImguiTo(rtId, fontId, callback)` | **Unsupported** | `hello_imgui_rtt`, `hello_dpi_scaling` |
| **Registered ImGui images** | `Canvas::registerImguiImage` / `imguiImage` / `updateImguiImage` | **Supported** (Phase 1) | `hello_imgui_visual`, `hello_imgui_image_registry`, `hello_markdown` (spinner) |

> **Registered images are done.** `ImguiImageManager` is now threaded through
> `runThreaded`/`commitFrame`/`renderFrameThreaded`; the sim-side `beginFrame` +
> `appendInternalPasses` run in the `FrameMode::Threaded` produce arm, the registry is
> serialized under the asset mutex (the same one `resolveImages` holds render-side), and GPU
> handle/RTT frees are deferred to the render thread via a two-phase retire queue. The three
> examples above run on `run(update, ui)`. What remains below is the **diegetic `renderImguiTo`**
> path.

Everything else the examples need is already threaded: main-canvas ImGui (interactive, input-marshalled),
`renderTo` sprite RTT passes (ride the snapshot's `rttPasses`), screenshots, explorers, runtime asset
mutation, `setScreenSize`. See THREADED_MODE.md § Done.

> Note: THREADED_MODE.md's "Per-demo caveats" currently lists only the `imguiImages` examples.
> The diegetic `renderImguiTo` demos (`hello_imgui_rtt`, `hello_dpi_scaling`) and
> `hello_markdown` belong in the same deferred bucket — this doc is the authoritative list.

## Why it's blocked — the mechanics

Both features are driven **exclusively from the `FrameMode::Single` arm** of
`FrameRunner::produce` (`source/frame_runner.cpp`). The `FrameMode::Threaded` arm (the sim-thread
commit, ends ~line 592) never touches them, and `commitFrame` passes both managers as `nullptr`.

### 1. Registered ImGui images (`ImguiImageManager`)

Single-arm-only calls, absent in the Threaded arm:
- `imguiImages->beginFrame()` — advances the manager's frame clock (frame_runner.cpp ~line 606).
- `imguiImages->appendInternalPasses(snapshot.rttPasses)` — pushes the internal RTT passes that
  rasterize each registered Visual into its off-screen target (frame_runner.cpp ~line 671).

Consequence: in threaded mode no internal pass is ever appended, so the image's off-screen target is
never rendered and its `ImTextureID` handle stays `0` (`isReady` false forever). `imguiImage(id)` in a
sim-side `ui` would bake a null/empty handle.

Additional wiring gaps:
- `commitFrame` (frame_runner.cpp ~line 1191+) calls `produce(FrameMode::Threaded, …, imguiImages = nullptr, …)`.
  `runThreaded`/`commitFrame` must thread the real `ImguiImageManager&` through to the sim side.
- `ImguiImageManager::resolveImages()` (handle creation) must run render-side in `consume`, and its
  `Entry` map is mutated from registration (potentially the sim thread) while read render-side — needs
  the same id-referenced / deferred-free discipline the asset registry already uses.

### 2. Diegetic ImGui (`ImguiRttManager::flushPending`)

`Canvas::renderImguiTo(rtId, fontId, cb)` → `ImguiRttManager::enqueue(rtId, cb)` pushes onto
`mPendingPasses`. The queue is drained by `ImguiRttManager::flushPending(dt, sharedFonts)`
(`source/imgui_rtt_manager.cpp`), called from `renderSnapshotContents` (~line 820) — which runs on the
**render/main thread in both modes**. For each pass `flushPending`:
```
SetCurrentContext(secondary rttCtx)
imguiNewFrameForRenderTarget(...) ; ImGui::NewFrame()
imguiDrawCallback()          // <-- the USER's ImGui code
ImGui::Render()
beginRttPass(...) + render draw data
SetCurrentContext(mainCtx)
```

Why that's wrong under threading:
- **User callback runs on the render thread.** In the threaded model every user callback (`update`, `ui`,
  input actions) runs on the **sim** thread; `renderImguiTo`'s callback captures app state by reference and
  would execute on the **render** thread — the exact cross-thread aliasing the sim/render split exists to
  prevent. Widgets mutating shared game state would race `update`.
- **Unsynchronized enqueue.** `enqueue` mutates `mPendingPasses` from the sim thread (inside `ui`/`update`)
  while the render thread iterates it in `flushPending` — no mutex, no snapshot handoff.
- **No draw-data clone.** Unlike the main sim-UI context (whose `ImDrawData` is deep-cloned into the
  snapshot via `imgui_draw_clone.*`), each secondary RTT context does a synchronous NewFrame→Render on the
  render thread. There is no per-RTT clone riding the snapshot.
- **Shared font atlas.** The `fontId` auto-push/pop and secondary-context glyphs read the shared 1.92
  dynamic atlas, which is only mutex-guarded on the main sim-UI path today.

## The reuse template (how the main path already solved the analogous problem)

The main-canvas ImGui path is the working blueprint — the fix is to generalize it from one context to N:

- **Sim-side render of the frame's ImGui**, on a per-context basis (main today; +N RTT contexts).
- **`ImDrawData` deep-clone into the `RenderSnapshot`** (`source/imgui_draw_clone.h/.cpp`,
  `RenderSnapshot::mainUi`) — add an analogous per-RTT clone list to the snapshot.
- **Render thread replays cloned draw data only** — never runs a user callback, never a secondary NewFrame.
- **`mImguiMutex`** already serializes the shared atlas around the short sim-UI section.
- **Input marshalling** render→sim already exists for the main context; diegetic panels currently take no
  input (v1 limitation noted in CLAUDE.md), so parity, not new input, is the near-term bar.

## Rough implementation sketch (for the follow-up agent to expand)

Not a committed design — a starting point.

**Registered images (smaller):**
1. Thread `ImguiImageManager&` through `runThreaded` → `commitFrame` → `produce(FrameMode::Threaded)`.
2. In the Threaded arm, call `imguiImages->beginFrame()` before `uiCallback`, and
   `imguiImages->appendInternalPasses(snapshot.rttPasses)` during the RTT gather (mirror lines 606/671).
3. Keep `resolveImages()` render-side in `consume`; audit `Entry`-map access for sim/render sharing and
   apply the deferred-free / id-reference discipline used by `AssetRegistry`.
4. Verify `imguiImage(id)` draw baked in the sim-UI `ui` resolves the render-created handle across the
   one-frame lag (handle stability is already a design goal of the registry).

**Diegetic ImGui (larger):**
1. Move secondary-context NewFrame → `renderImguiTo` callback → Render onto the **sim thread**, against
   per-RTT secondary contexts, under `mImguiMutex`.
2. Deep-clone each RTT's `ImDrawData` into a new per-RTT field of the snapshot (extend `imgui_draw_clone`).
3. `enqueue` becomes sim-thread-local (drained within the same commit); the render thread only executes
   `beginRttPass` + replays the cloned draw data — no user callback, no NewFrame render-side.
4. Reconcile secondary-context lifecycle (lazy create / `releaseContext` / `drainPendingFontOps`) with the
   sim/render split and deferred frees.

## Affected files

- `source/frame_runner.cpp` / `.h` — `produce(FrameMode::Threaded)` arm, `commitFrame` (drop the
  `nullptr` managers), `renderFrameThreaded`, `consume`.
- `source/imgui_rtt_manager.h` / `.cpp` — `enqueue` / `flushPending` split into sim-produce vs
  render-replay.
- `source/imgui_image_manager.h` / `.cpp` — sim-side `beginFrame`/`appendInternalPasses`, render-side
  `resolveImages`, thread-safe `Entry` map.
- `source/imgui_draw_clone.h` / `.cpp` + `source/render_snapshot.h` — per-RTT cloned draw data in the
  snapshot.
- `include/canvas.h` / `source/canvas.cpp` — `renderImguiTo` / `registerImguiImage` thread-affinity docs.

## Verification (when implemented)

- Migrate `hello_imgui_rtt`, `hello_dpi_scaling`, `hello_imgui_visual`, `hello_imgui_image_registry`,
  `hello_markdown` to `run(update, ui[, sim, render])`; each renders identically to its single-thread form.
- TSan under `xvfb-run` on all five: 0 races in our code (proves the RTT-context and image-Entry sharing is
  guarded). ASan/UBSan clean.
- SwiftShader goldens unchanged.
- Then drop these five from the deferred bucket in the migration plan and this doc's TL;DR table.
