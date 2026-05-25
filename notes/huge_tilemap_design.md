# Huge Tilemap Design — `Tilemap` + `TilemapView` (pooled, viewport-culled)

## Context

Today, an `IndirectTexture` in tile-map mode (`setMap(mapSize)`) is rendered as a single bellota with one mesh sized `mapSize * tileSize` and one GPU `map` texture covering the entire world. For large maps (open worlds, side-scrolling levels) two costs become severe:

1. **All-or-nothing map upload** ([source/canvas_impl.cpp:678-689](../source/canvas_impl.cpp#L678-L689)): any `setCell` re-uploads the whole world's cell texture next frame.
2. **No spatial culling and no concept of world data vs. rendered window**: the bellota always draws; the map texture is world-sized; there is no path to streaming or to bounded VRAM regardless of world size.

This design introduces two narrowly-scoped tilemap types:

- **`Tilemap`** — a pure data container holding the world: atlas (one copy), palette (one copy), full cell grid (`mapSize.x * mapSize.y` bytes), tile size, chunk size. Not rendered. Mutated via `setCell` at world-cell granularity.
- **`TilemapView`** — a renderer that owns a small **pool of IndirectTexture + Bellota slots** (sized to viewport + margin). Each slot has a stable IndirectTexture whose atlas + palette are copied once from `Tilemap`; only its *map data* (cell grid) and its bellota's *position* change as the camera moves. Slots stay anchored to their pool indices; the world chunks they display rotate through the pool as the camera scrolls.

Effect: memory and per-frame work are **O(viewport) + O(world data)**, not O(world chunks). A 1000×1000 cell world (~1000 chunks) needs ~50 IndirectTextures and ~50 bellotas instead of ~1000 of each, and ~1 MB of duplicated atlas data instead of ~16 MB. The renderer pipeline learns nothing new — pool slots are ordinary `IndirectTexture` + `Bellota` instances going through the existing 3-binding tilemap path.

### Locked-in decisions

- **Pool model**: small fixed slot count, slots anchored to pool indices, world chunks rotate through slots. Border-only rebind on camera motion (memcpy chunk cells + reposition bellota; *no* `canvas.setTexture` calls — the bellota stays bound to its slot's IndirectTexture for life).
- **Data/view split**: `Tilemap` (data, one per world) + `TilemapView` (renderer, may have multiple views per `Tilemap` later — v1 focuses on one).
- **Engine changes are minimal**: no shaders, no backend code, no IndirectTexture API changes apart from `setMapBulk`. New types live alongside; canvas grows a few accessors.
- **Cull-only v1**: bellotas/textures stay GPU-resident for the pool; streaming (free/upload textures across a larger world buffer) is deferred and is a natural extension since pool eviction = streaming eviction.

## Design

### `Tilemap` — world data type

```cpp
// include/tilemap.h
class Tilemap
{
public:
    Tilemap(glm::ivec2 mapSize,      // world cells
            glm::ivec2 chunkSize,    // cells per pool slot
            glm::ivec2 tileSize,     // pixels per cell
            const ColorPallete& palette,
            std::span<const std::vector<std::uint8_t>> tileGraphics);  // one entry per atlas layer

    void          setCell(glm::ivec2 worldCell, std::uint8_t layer);
    std::uint8_t  cell   (glm::ivec2 worldCell) const;

    glm::ivec2 mapSize()       const;
    glm::ivec2 chunkSize()     const;
    glm::ivec2 tileSize()      const;
    glm::ivec2 chunkGridSize() const;

    const ColorPallete&                        palette()       const;
    std::span<const std::vector<std::uint8_t>> tileGraphics() const;
    std::vector<std::uint8_t> chunkData(glm::ivec2 chunkPos)   const;
    std::uint64_t             chunkGeneration(glm::ivec2 chunkPos) const;
};
```

Internals:
- Flat `std::vector<std::uint8_t>` of size `mapSize.x * mapSize.y` for the world cell grid.
- `setCell` bumps a per-chunk generation counter (`std::vector<std::uint64_t>`). `TilemapView` polls these to know when to re-sync slot data — works cleanly for multiple views over one `Tilemap`.
- Edge chunks (when `mapSize` isn't divisible by `chunkSize`) report chunk data with unused cells set to 0.

### `TilemapView` — renderer with pool

```cpp
// include/tilemap_view.h
class TilemapView
{
public:
    explicit TilemapView(TilemapId tilemapId);  // pool sized internally based on canvas viewport

    void      setCamera(glm::vec2 worldOffset);
    glm::vec2 camera() const;

    void          setDepthOffset(std::int8_t offset);
    std::int8_t   depthOffset() const;

    TilemapId tilemap() const;
};
```

Pool layout lives in `canvas_impl`:

```cpp
struct PoolSlot
{
    TextureId     textureId;
    BellotaId     bellotaId;
    glm::ivec2    currentWorldChunk{-1, -1};  // {-1,-1} = unassigned
    std::uint64_t syncedGeneration{0};
};

struct TilemapViewPack
{
    TilemapView             view;
    glm::ivec2              poolGridSize;
    std::vector<PoolSlot>   slots;
};
```

- **Pool sizing** at registration: `poolGridSize = ceil(screenSize / (chunkSize * tileSize)) + {2, 2}` (1-chunk margin per side).
- **Pool init**: allocate slots. For each: build IndirectTexture with the Tilemap's atlas + palette + `setMap(chunkSize)`, register it via canvas (tagged view-managed), build a bellota bound to it (tagged view-managed), store both ids.

### Per-frame logic (in `canvas_impl::runOneFrame`, between user update and texture upload)

For each `TilemapViewPack`:
1. Compute the visible-world AABB. With v1's "view fills canvas centered" convention: `worldBottomLeft = view.camera() - canvasSize/2`, `worldTopRight = view.camera() + canvasSize/2`.
2. Compute `slotOriginChunk = floor(worldBottomLeft / chunkPixelSize) - {1, 1}` (margin).
3. For each slot `(px, py)`:
   - `desiredWorldChunk = slotOriginChunk + {px, py}`.
   - **Out of world** (any axis negative or >= chunkGridSize): hide the slot (`bellota.visible(false)`), skip data updates.
   - **In world**:
     - If `desiredWorldChunk != slot.currentWorldChunk` *or* generation differs: write chunk data via `IndirectTexture::setMapBulk(tilemap.chunkData(desiredWorldChunk))`, update `slot.currentWorldChunk` and `slot.syncedGeneration`.
     - Compute bellota location: `canvasCenter + chunkCenterWorld - view.camera()` where `chunkCenterWorld = desiredWorldChunk * chunkPixelSize + chunkPixelSize/2`.
     - Make visible.

Texture upload pass at line 620 picks up the dirty slot map textures and uploads them through the existing path. Depth-sort + main draw run unchanged.

### Lifecycle

- `canvas.addTilemap(Tilemap&&) → TilemapId` — stores world data; no GPU allocations.
- `canvas.addTilemapView(TilemapView&&) → TilemapViewId` — allocates the pool (K IndirectTextures + K bellotas tagged view-managed).
- `canvas.removeTilemapView(id)` — removes pool bellotas and textures; drops the pack.
- `canvas.removeTilemap(id)` — `debugCheck`s no view still references it.
- View-managed bellotas/textures: public `removeBellota`/`removeTexture` `debugCheck`s reject ids in the view-managed sets.

### `IndirectTexture::setMapBulk(std::span<const std::uint8_t>)`

Only `IndirectTexture` API addition. Atomically overwrites the cell grid; asserts size matches `mapSize.x * mapSize.y`. Sets `mMapDirty`.

### Convenience builder

```cpp
struct TilemapHandles
{
    TilemapId     tilemapId;
    TilemapViewId viewId;
};

TilemapHandles createTilemap(Canvas& canvas,
                              glm::ivec2 mapSize, glm::ivec2 chunkSize, glm::ivec2 tileSize,
                              const ColorPallete& palette,
                              std::span<const std::vector<std::uint8_t>> tileGraphics);
```

World-cell editing goes through `canvas.tilemap(handles.tilemapId).setCell(...)`.

## Files added

- `include/tilemap_id.h`, `include/tilemap_view_id.h`
- `include/tilemap.h`, `source/tilemap.cpp`
- `include/tilemap_view.h`, `source/tilemap_view.cpp`
- `include/tilemap_builder.h`, `source/tilemap_builder.cpp`
- `examples/hello_tilemap_huge.cpp`

## Files modified

- `include/canvas.h`, `source/canvas.cpp` — forwarders.
- `source/canvas_impl.h`, `source/canvas_impl.cpp` — `mTilemaps`, `mTilemapViews`, `TilemapViewPack`, per-frame pre-pass, view-managed tagging in `removeBellota` / `removeTexture`.
- `include/texture.h`, `source/texture.cpp` — `setMapBulk`.
- `include/nothofagus.h` — include new headers.
- `CMakeLists.txt` (root) — add new source files.
- `examples/CMakeLists.txt` — register `hello_tilemap_huge`.
- `CLAUDE.md` — new "Huge tilemaps via `Tilemap` + `TilemapView`" section.

## Out of scope (deferred)

- **Streaming** beyond the pool (RAM-side world cell grid eviction to disk).
- **Multiple views per `Tilemap`** (the design supports it; v1 exercises one).
- **RTT-targeted tilemap rendering**.
- **Shader-scrolled single-draw fast path**.
- **Pool shrink on viewport reduction**.
