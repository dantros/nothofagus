# CanvasImpl modularization — pending ideas

Tracking the slices of the CanvasImpl decomposition that haven't landed yet.
The numbering matches the original brainstorm chat; ideas #2 (extract
`drawBellotaPacks`), #6 (`AssetRegistry`), and #8 (`Bellota::withTexture` /
`Bellota::withMesh` builders) are already on `main`. Ideas #1 + #3 + #4
are the natural next bundle — see the bottom of this file for a combined plan.

---

## 1. Carve `runOneFrame` into a frame pipeline

Today `runOneFrame` in [source/canvas_impl.cpp](source/canvas_impl.cpp)
interleaves: input → ImGui setup → user update → tilemap explorers →
asset GC → texture upload nest → render-target lazy init → mesh upload →
depth sort → RTT passes → main draw → stats overlay → swap. The
texture-upload section alone is ~80 lines.

Goal: extract free helpers in the same TU (or moved into AssetRegistry as
methods) so `runOneFrame` becomes ~30 lines of phase orchestration:

```cpp
static void uploadDirtyTextures     (TextureContainer&,      ActiveBackend&);
static void uploadDirtyRenderTargets(RenderTargetContainer&, TextureContainer&, ActiveBackend&);
static void uploadDirtyMeshes       (MeshContainer&,         ActiveBackend&);
static void drawScene(std::span<const BellotaPack* const>,
                      const TextureContainer&, const MeshContainer&,
                      const glm::mat3& worldTransform, ActiveBackend&);
```

Each helper is independently readable and unit-testable. No semantic
change. Easiest after #3 lands (which moves the upload body onto the
packs themselves) — the helpers then become single-line iterations.

## 3. Move texture-upload logic onto `TexturePack`

The dirty-flag branch matrix in `runOneFrame`'s upload nest (mode ×
isDirty × isAtlasDirty × isMapDirty × isPaletteDirty) is the densest
piece of the file. The dirty-flag knowledge already lives next to the
data in `TexturePack`; the upload calls should too.

Give each pack a `syncToGpu(ActiveBackend&)` method:

```cpp
struct TexturePack {
    /// ... existing fields ...
    void syncToGpu(ActiveBackend&);
};

struct MeshPack {
    /// ... existing fields ...
    void syncToGpu(ActiveBackend&);
};

struct RenderTargetPack {
    /// ... existing fields ...
    void syncToGpu(ActiveBackend&, TextureContainer&);   // touches the proxy TexturePack
};
```

`runOneFrame` then iterates each container once and calls `syncToGpu`.
The branch matrix moves into `TexturePack::syncToGpu`'s body where it
belongs.

Decision point: pass the backend reference per-call or hand each
`AssetRegistry` an `ActiveBackend&` (already there) and have the
container offer a `syncAll(backend)` method? The free-function pattern
matches existing usage; keep it.

## 4. Stop duplicating teardown logic

Today the same GPU-free sequence appears twice for textures:
- `AssetRegistry::removeTexture` ([source/asset_registry.cpp](source/asset_registry.cpp), lines ~95-110): frees palette/map/dtexture handles
- `AssetRegistry::freeAllGpuResources` ([source/asset_registry.cpp](source/asset_registry.cpp), lines ~370-405): iterates and does the same thing

Same duplication exists for `removeMesh` vs the meshes loop, and
`removeRenderTarget` vs the render-targets loop. Plus
`markTextureAsDirty` is a third copy of the texture free sequence
([source/asset_registry.cpp](source/asset_registry.cpp)).

Goal: give each pack a `freeGpuResources(ActiveBackend&)` method:

```cpp
struct TexturePack {
    /// ... existing fields ...
    void freeGpuResources(ActiveBackend&);
};

struct MeshPack {
    /// ... existing fields ...
    void freeGpuResources(ActiveBackend&);
};

struct RenderTargetPack {
    /// ... existing fields ...
    void freeGpuResources(ActiveBackend&, TexturePack& proxy);
};
```

`removeX` becomes `pack.freeGpuResources(backend); pack.clear(); container.remove(id);`.
`freeAllGpuResources` becomes a one-line loop per container.
`markTextureAsDirty` becomes `pack.freeGpuResources(backend); pack.clear();`.

Tightly coupled with #3 — both add methods to the same pack types — so
worth doing them together.

---

## 5. Decouple `TilemapManager` from `Canvas`

[source/tilemap_manager.cpp](source/tilemap_manager.cpp) takes
`Canvas&` purely to call `addTexture`, `removeTexture`, `addBellota`,
`removeBellota` for its pool slots. That's a circular dep
(`Canvas` → `CanvasImpl` → `TilemapManager` → `Canvas&`).

Goal: define a slim interface in [source/](source/):

```cpp
class ResourceContext {
public:
    virtual ~ResourceContext() = default;
    virtual BellotaId addBellota(const Bellota&) = 0;
    virtual void      removeBellota(BellotaId) = 0;
    virtual TextureId addTexture(const Texture&) = 0;
    virtual void      removeTexture(TextureId) = 0;
};
```

`AssetRegistry` already implements exactly these four methods, so make
it derive from `ResourceContext` (or implement a thin adapter).
`TilemapManager::addTilemapExplorer` /
`removeTilemapExplorer` / `updateExplorers` take
`ResourceContext&` instead of `Canvas&`. `CanvasImpl` passes
`mAssets`.

Side benefits:
- `tilemap_manager.h` stops including `canvas.h` — kills the cyclic
  include chain.
- `TilemapManager` is unit-testable with a mock `ResourceContext`
  (no window, no GPU).

Best done after #1 + #3 + #4 because by then `AssetRegistry`'s
interface is the natural fit and we won't be retrofitting through
intermediate states.

## 7. Eject the window-mode bookkeeping

`mLastWindowedAABox` lives on `CanvasImpl`
([source/canvas_impl.h](source/canvas_impl.h))
purely so `setFullScreenOnMonitor` can remember the windowed rect to
restore later. The behavior belongs inside `Window` (or a small
`WindowModeController` wrapping it):

```cpp
class WindowModeController {
public:
    void setFullScreenOnMonitor(std::size_t);
    void setWindowed();
    bool isFullscreen() const;
    // ...
private:
    Window& mWindow;
    AABox   mLastWindowedAABox;
};
```

Three methods on `CanvasImpl` become one-line forwarders or move out
entirely; one piece of state leaves the god object.

## 9. Stats overlay shouldn't be hard-coded into the frame loop

[source/canvas_impl.cpp](source/canvas_impl.cpp) `runOneFrame` ends
with an ImGui `Begin/End` block that draws fps/ms when `mStats` is
true. It's the only ImGui draw call hard-coded into the engine.

Extract into a free function `drawStatsOverlay(float deltaTimeMS)` (or a
tiny `OverlayRenderer` struct if it grows beyond fps/ms). Frame loop
holds zero ImGui draw calls of its own.

Tiny change; bundle into #1's frame-pipeline carve.

---

## Combined plan: #1 + #3 + #4

See the chat transcript for a step-by-step. Summary:

1. **#4 first** — add `freeGpuResources(backend)` to `TexturePack`,
   `MeshPack`, `RenderTargetPack`. Rewrite the per-id removes and
   `freeAllGpuResources` to use them. Smallest, lowest-risk; makes
   teardown symmetric on both axes.
2. **#3 second** — add `syncToGpu(backend)` to the same three pack
   types. Move the upload nest body from `runOneFrame` into
   `TexturePack::syncToGpu`. The lazy render-target init loop becomes
   `RenderTargetPack::syncToGpu`. The mesh upload loop becomes
   `MeshPack::syncToGpu`.
3. **#1 last** — by now the body of `runOneFrame` is dominated by
   one-line iterations + control-flow plumbing. Extract phase helpers
   (`uploadDirtyTextures`, `uploadDirtyRenderTargets`,
   `uploadDirtyMeshes`, `drawScene`) and the stats overlay (#9 freebie
   if we're already there). Final `runOneFrame` is ~30 lines.

Verify after each step on linux glfw+opengl + glfw+vulkan, plus
`hello_headless` end-to-end. Nonvisual tests should pass throughout.
Visual golden-image tests if/when a display server is available.

## Deferred (mentioned for completeness, lower priority)

- `textureArray()` declarations in [source/canvas_impl.h](source/canvas_impl.h)
  are declared but never defined — calling either is a link error.
  Dead code; flag for a follow-up cleanup.
- Verbose Doxygen blocks in [include/canvas.h](include/canvas.h) and
  former blocks in [source/canvas_impl.h](source/canvas_impl.h) sometimes
  duplicated each other. Worth a single-purpose pass to keep docs only
  on the public API.
