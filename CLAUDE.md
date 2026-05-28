# Nothofagus - Claude Code Guide

## Project Overview

Pixel art real-time renderer with OpenGL 3.3 and Vulkan backends, written in C++20. Outputs a static library (`nothofagus`) consumed by user projects.

## Key Terminology

- **Bellota** ("acorn") — a drawable sprite/element on screen
- **IndirectTexture** — paletted texture: pixels hold color indices into a `ColorPallete`. Optional multi-layer atlas for sprite animation; optional cell grid (`setMap`) opts the texture into tile-map rendering, where each cell selects a layer to draw.
- **DirectTexture** — raw RGBA texture
- **Transform** — position (`mLocation`), scale (`mScale`), rotation (`mAngle` in degrees)
- **Controller** — keyboard, mouse, and gamepad input handler; maps `KeyboardTrigger`/`MouseButtonTrigger`/`GamepadButtonTrigger` → `Action` callbacks and tracks mouse position as `glm::vec2`
- **MouseButton** — enum with values `Left`, `Middle`, `Right`
- **GamepadButton** — enum with values `A`, `B`, `X`, `Y`, `LeftBumper`, `RightBumper`, `Back`, `Start`, `Guide`, `LeftThumb`, `RightThumb`, `DpadUp`, `DpadRight`, `DpadDown`, `DpadLeft`
- **GamepadAxis** — enum with values `LeftX`, `LeftY`, `RightX`, `RightY`, `LeftTrigger`, `RightTrigger`
- **AnimationState** — frame sequence with per-frame durations, loops automatically
- **AnimationStateMachine** — manages multiple states with named event-based transitions

## Architecture

```
Canvas (public API)
└── CanvasImpl (Pimpl, hidden windowing/rendering details)
    ├── Window : SelectedWindowBackend   → GlfwBackend, Sdl3Backend, or HeadlessBackend (compile-time)
    ├── ActiveBackend (RenderBackend)    → OpenGLBackend or VulkanBackend (compile-time)
    │   └── VulkanBackend
    │       └── ActiveVulkanPresentation → WindowedVulkanPresentation or HeadlessVulkanPresentation (compile-time)
    ├── IndexedContainer<BellotaPack>    → Bellota + Mesh + DMesh + Tint
    └── IndexedContainer<TexturePack>   → Texture + DTexture
```

- `include/` — public API headers
- `source/` — implementation + internal headers (never expose to users)
- `source/backends/` — window/input backend implementations (`glfw_backend`, `sdl3_backend`, `headless_backend`, per-backend keyboard/mouse/gamepad mappers), render backends (`opengl_backend`, `vulkan_backend`), and Vulkan presentation policies (`vulkan_presentation`)
- `examples/` — standalone demo executables
- `third_party/` — git submodules (glfw, glad, glm, imgui, spdlog, font8x8, SDL, vk-bootstrap, VulkanMemoryAllocator)

## Build System

**Presets (CMakePresets.json):**

Preset naming: `{platform}-{buildtype}-{window}-{graphics}[-examples]`. All use Ninja.

| Preset pattern | Platform | Compiler | Window | Graphics |
|----------------|----------|----------|--------|----------|
| `windows-{debug,release}-glfw-opengl[-examples]` | Windows | clang-cl | GLFW | OpenGL |
| `windows-{debug,release}-glfw-vulkan[-examples]` | Windows | clang-cl | GLFW | Vulkan |
| `windows-{debug,release}-sdl3-opengl[-examples]` | Windows | clang-cl | SDL3 | OpenGL |
| `windows-{debug,release}-sdl3-vulkan[-examples]` | Windows | clang-cl | SDL3 | Vulkan |
| `windows-{debug,release}-headless-vulkan[-examples]` | Windows | clang-cl | None | Vulkan (offscreen) |
| `linux-{debug,release}-glfw-opengl[-examples]` | Linux | clang++ | GLFW | OpenGL |
| `linux-{debug,release}-glfw-vulkan[-examples]` | Linux | clang++ | GLFW | Vulkan |
| `linux-{debug,release}-sdl3-opengl[-examples]` | Linux | clang++ | SDL3 | OpenGL |
| `linux-{debug,release}-sdl3-vulkan[-examples]` | Linux | clang++ | SDL3 | Vulkan |
| `linux-{debug,release}-headless-vulkan[-examples]` | Linux | clang++ | None | Vulkan (offscreen) |

**Build and install (examples):**
```bash
cmake --preset windows-debug-glfw-opengl-examples
cmake --build build/windows-debug-glfw-opengl-examples
cmake --install build/windows-debug-glfw-opengl-examples
# Artifacts land in install/windows-debug-glfw-opengl-examples/
```

**CMake options:**
- `NOTHOFAGUS_BUILD_EXAMPLES` — build demo apps (default OFF, enabled by `-examples` presets)
- `NOTHOFAGUS_INSTALL` — install artifacts (default OFF, presets set ON)
- `NOTHOFAGUS_BUILD_DOCS` — generate Doxygen docs (default OFF)
- `NOTHOFAGUS_WINDOW_BACKEND` — `"GLFW"` (default) or `"SDL3"`; selects the window/input backend at configure time
- `NOTHOFAGUS_BACKEND_VULKAN` — use the Vulkan render backend instead of OpenGL (default OFF)
- `NOTHOFAGUS_HEADLESS_VULKAN` — pure offscreen Vulkan rendering with no window or display server (default OFF; requires `NOTHOFAGUS_BACKEND_VULKAN=ON`). Replaces the window backend with `HeadlessBackend` and the Vulkan presentation policy with `HeadlessVulkanPresentation`. Intended for CI/CD rendering tests.

## Window Backend Abstraction

The windowing and input layer is abstracted behind a **C++20 concept** (`WindowBackend`) so the rest of the engine is completely decoupled from both GLFW and SDL3.

**Selection is compile-time** — `NOTHOFAGUS_WINDOW_BACKEND=SDL3` sets the `NOTHOFAGUS_BACKEND_SDL3` preprocessor define, which swaps in `Sdl3Backend`. `NOTHOFAGUS_HEADLESS_VULKAN` swaps in `HeadlessBackend`. Without either, `GlfwBackend` is used. A `static_assert` verifies the chosen class satisfies the concept at build time.

```
source/backends/
├── window_backend.h       — WindowBackend concept + SelectedWindowBackend type alias
├── glfw_backend.h/.cpp    — GLFW implementation
├── glfw_keyboard.h/.cpp   — GLFW key-code ↔ Key mapping
├── glfw_mouse.h/.cpp      — GLFW button ↔ MouseButton mapping
├── glfw_gamepad.h/.cpp    — GLFW button/axis ↔ GamepadButton/GamepadAxis mapping
├── sdl3_backend.h/.cpp    — SDL3 implementation
├── sdl3_keyboard.h/.cpp   — SDL3 key-code ↔ Key mapping
├── sdl3_mouse.h/.cpp      — SDL3 button ↔ MouseButton mapping
├── sdl3_gamepad.h/.cpp    — SDL3 button/axis ↔ GamepadButton/GamepadAxis mapping
└── headless_backend.h/.cpp — No-op stub for headless Vulkan (no window, no display server)
```

`CanvasImpl` owns a `Window` that inherits from `SelectedWindowBackend` (PIMPL). The `window_backend.h` header is only included in `canvas_impl.cpp`, keeping backend headers entirely out of the public API.

**`WindowBackend` concept — required interface:**
- Session: `beginSession(Controller&)`, `isRunning()`
- Per-frame: `newImGuiFrame()`, `endFrame(Controller&, ViewportRect, ScreenSize)`, `getFramebufferSize()`, `getTime()`
- ImGui/DPI: `initImGui(fontSize, fontData, fontDataLen)`, `contentScale()`
- Window management: `getCurrentMonitor()`, `isFullscreen()`, `setFullscreenOnMonitor(index)`, `getWindowAABox()`, `setWindowed(AABox)`, `getWindowSize()`, `requestClose()`

Both windowed backends route all keyboard, mouse, scroll, and gamepad events into `Controller` using the same public API (`activate`, `activateMouseButton`, `updateMousePosition`, `scrolled`, `activateGamepadButton`, `updateGamepadAxis`). The `HeadlessBackend` provides no input — it returns no-op/defaults for all input and window management methods, sets `ImGuiIO::DisplaySize` manually, and uses `std::chrono::steady_clock` for timing.

## Vulkan Presentation Policy

When the Vulkan render backend is active, `VulkanBackend` delegates all surface/swapchain/present operations to a **presentation policy** — a compile-time selected struct that encapsulates the differences between windowed and headless rendering.

```
source/backends/
├── vulkan_presentation.h   — Policy structs + ActiveVulkanPresentation type alias
├── vulkan_presentation.cpp — Implementations (only the active policy is compiled via #ifdef)
├── vulkan_backend.h        — VulkanBackend class (holds ActiveVulkanPresentation mPresentation)
└── vulkan_backend.cpp      — Core rendering logic, delegates to mPresentation at 7 points
```

**Two policies:**

| Policy | Selected when | Surface | Swapchain | Present | Screenshot source |
|--------|---------------|---------|-----------|---------|-------------------|
| `WindowedVulkanPresentation` | `NOTHOFAGUS_HEADLESS_VULKAN` is not defined | VkSurfaceKHR via GLFW/SDL | VkSwapchainKHR | vkQueuePresentKHR with semaphores | Swapchain image (blit to intermediate for format conversion) |
| `HeadlessVulkanPresentation` | `NOTHOFAGUS_HEADLESS_VULKAN` is defined | None | None | Submit with fence only, no present | Offscreen VkImage (direct vkCmdCopyImageToBuffer, no format conversion) |

**Policy interface (duck-typed, not virtual):**

| Method | Called from | Purpose |
|--------|------------|---------|
| `createSurface()` | `initialize()` | Create VkSurfaceKHR or no-op |
| `configurePhysicalDeviceSelector()` | `initialize()` | Add `.set_surface().require_present()` or `.require_present(false).defer_surface_initialization()` |
| `retrieveQueues()` | `initialize()` | Get graphics + present queues, or graphics only |
| `createPresentationTarget()` | `initialize()` | Create swapchain (determines format/extent) or offscreen VkImage |
| `createPresentationFramebuffers()` | `initialize()` | Create framebuffers using the main render pass (called after render pass creation) |
| `colorFormat()` / `mainPassFinalLayout()` | `initialize()` | Provide format and final layout for main render pass creation |
| `acquireImage()` | `beginFrame()` | vkAcquireNextImageKHR or no-op (always succeeds) |
| `mainFramebuffer()` / `extent()` | `beginMainPass()` | Return the active framebuffer and render area |
| `submitAndPresent()` | `endFrame()` | Submit + present with semaphores, or submit with fence only |
| `takeScreenshot()` | `takeScreenshot()` | Copy pixels from swapchain or offscreen image to CPU |
| `shutdown()` | `shutdown()` | Destroy surface/swapchain or offscreen resources |

**Initialization order** (critical — render pass depends on format from the presentation target):
1. Instance → surface → physical device → logical device → VMA → command pool → fences
2. `createPresentationTarget()` — creates swapchain or offscreen image, determines color format
3. Main render pass — uses `colorFormat()` and `mainPassFinalLayout()` from the policy
4. `createPresentationFramebuffers()` — creates framebuffers using the render pass from step 3

The `#ifdef NOTHOFAGUS_HEADLESS_VULKAN` appears only in two places: the `ActiveVulkanPresentation` type alias (in `vulkan_presentation.h`) and the implementation guard (in `vulkan_presentation.cpp`). All other code is free of headless conditionals.

**Headless offscreen image:** created with `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT` in `VK_FORMAT_R8G8B8A8_UNORM`. The main render pass uses `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` as the final layout (not `PRESENT_SRC_KHR`). Screenshots transition to `TRANSFER_SRC_OPTIMAL`, copy via `vkCmdCopyImageToBuffer`, then restore to `COLOR_ATTACHMENT_OPTIMAL`.

## Public API Patterns

### Creating textures and bellotas
```cpp
// Paletted texture
Nothofagus::IndirectTexture tex({8, 8}, {0,0,0,0}); // size, background color
tex.setPallete(pallete).setPixels({ /* indices */ });
TextureId texId = canvas.addTexture(tex);
BellotaId id = canvas.addBellota({{{x, y}}, texId});

// Raw RGBA texture
Nothofagus::DirectTexture tex({w, h});
TextureId texId = canvas.addTexture(tex);

// Rebind a bellota to a different texture (old texture is auto-GC'd next frame)
canvas.setTexture(bellotaId, newTexId);
```

### Main loop
```cpp
canvas.run([&](float dt) {
    // ImGui calls go here
    canvas.bellota(id).transform().location() += ...;
});
```

### Headless mode and manual tick

Pass `headless = true` as the last constructor argument to create a canvas with a hidden window (no visible UI). Works with all backend combinations (GLFW/SDL3 + OpenGL/Vulkan). Use `tick()` to drive rendering one frame at a time with a caller-supplied delta time (in milliseconds) instead of the engine's internal loop.

`run()` and `tick()` are mutually exclusive on a given Canvas — do not mix them.

```cpp
// Headless canvas — no window appears
Nothofagus::Canvas canvas({15, 10}, "test", {0,0,0}, 1, 14, /*headless=*/true);

// Add textures and bellotas as usual...
auto texId = canvas.addTexture(tex);
auto id    = canvas.addBellota({{{x, y}}, texId});

// Drive the loop manually — dt in milliseconds
for (int i = 0; i < 10; ++i)
    canvas.tick(16.0f);

// tick() with update callback and controller (same overloads as run()):
canvas.tick(16.0f, [&](float dt) { /* update logic */ }, controller);
canvas.tick(16.0f, [&](float dt) { /* update logic */ });

// Screenshot works in headless mode:
Nothofagus::DirectTexture screenshot = canvas.takeScreenshot();
```

GPU resources are cleaned up automatically in the `Canvas` destructor — no need to call `run()` or any explicit shutdown.

**Two headless modes exist:**

| Mode | CMake flag | Window | Display server | Use case |
|------|-----------|--------|----------------|----------|
| Hidden window | `headless=true` at runtime | Created but invisible (GLFW/SDL) | Required (X11/xvfb on Linux) | Local testing with real GPU |
| Pure offscreen | `NOTHOFAGUS_HEADLESS_VULKAN=ON` at build time | None (`HeadlessBackend`) | Not required | CI/CD with software Vulkan (SwiftShader/lavapipe) |

Both modes use the same `Canvas` API — the difference is entirely at the build/link level. Code that works with `headless=true` works unchanged when built with `NOTHOFAGUS_HEADLESS_VULKAN=ON`.

### Keyboard input
```cpp
controller.registerAction({Key::W, DiscreteTrigger::Press}, [&]() { ... });
controller.deleteAction({Key::W, DiscreteTrigger::Press});
canvas.run(update, controller);
```

### Mouse input

Mouse position is delivered in **game canvas coordinates** (origin bottom-left, same space as bellota positions). Coordinate conversion from window coords through the letterbox viewport happens automatically inside `canvas.run()`.

```cpp
// Button callbacks — same DiscreteTrigger enum as keyboard
controller.registerMouseAction({MouseButton::Left, DiscreteTrigger::Press}, [&]() { ... });
controller.deleteMouseAction({MouseButton::Right, DiscreteTrigger::Release});

// Move callback — fires every time the cursor moves
controller.registerMouseMove([&](glm::vec2 position) { ... });

// Scroll callback — fires on scroll wheel events; offset.x = horizontal, offset.y = vertical
// offset.y > 0 = scroll up, offset.y < 0 = scroll down
controller.registerMouseScroll([&](glm::vec2 offset) { ... });

// Polling — valid at any point inside the canvas.run() callback
glm::vec2 pos = controller.getMousePosition();
```

Mouse button callbacks are dispatched via the same `processInputs()` queue as keyboard. The move and scroll callbacks fire immediately from the backend event source (outside the queue), so they can be called multiple times per frame.

### Gamepad input

Gamepads are polled each frame by the active backend. Button state diffs generate queued `Press`/`Release` events; axis updates fire callbacks immediately if the value changed. Connect/disconnect is detected per-frame by the backend.

**Axis normalisation applied by the backend before the controller receives values:**
- `LeftY`, `RightY`: inverted so positive = up (matches canvas convention)
- `LeftTrigger`, `RightTrigger`: remapped to `[0, 1]`
- All axes: 0.1 deadzone (values below threshold clamped to 0)

```cpp
// Registration
controller.registerGamepadAction({gamepadId, GamepadButton::A, DiscreteTrigger::Press}, [&]() { ... });
controller.deleteGamepadAction({gamepadId, GamepadButton::A, DiscreteTrigger::Press});
controller.registerGamepadAxis(gamepadId, GamepadAxis::LeftX, [&](float value) { ... });
controller.registerGamepadConnected([&](int id) { ... });
controller.registerGamepadDisconnected([&](int id) { ... });

// Polling — valid any time inside canvas.run() callback
float value = controller.getGamepadAxis(gamepadId, GamepadAxis::LeftX);
bool  held  = controller.getGamepadButton(gamepadId, GamepadButton::A);
bool  conn  = controller.isGamepadConnected(gamepadId);
std::vector<int> ids = controller.getConnectedGamepadIds();   // sorted
```

**`GamepadButton` enum:** `A, B, X, Y, LeftBumper, RightBumper, Back, Start, Guide, LeftThumb, RightThumb, DpadUp, DpadRight, DpadDown, DpadLeft`

**`GamepadAxis` enum:** `LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger`

Gamepad button events are dispatched in `processInputs()` (same frame-deferred pattern as keyboard/mouse). Axis callbacks fire immediately when polled (same pattern as scroll).

### Tile maps

`IndirectTexture` doubles as a tile-map source: store the unique tile graphics as layers, then call `setMap(mapSize)` to allocate a `mapSize.x * mapSize.y` cell grid where each cell holds a `uint8_t` layer index. The bellota's mesh expands to `mapSize * size()` (per-tile pixel size × grid). Rendering goes through a separate 3-binding GPU pipeline (`atlas` + `map` + `palette`) so a tilemap with N unique tiles uses an N-layer atlas regardless of cell count — tile graphics are reused across cells.

```cpp
constexpr glm::ivec2 tileSize{8, 8};
constexpr glm::ivec2 mapSize {4, 3};
constexpr std::size_t tileCount = 2;          // unique tile graphics

Nothofagus::IndirectTexture tileMap(tileSize, glm::vec4(0.0f), tileCount);
tileMap.setPallete(Nothofagus::ColorPallete{ /* ... */ });

// Populate each tile slot from a contiguous span of palette indices
auto circlePx = makeCircleTile(tileSize);    // std::vector<std::uint8_t>, tileSize.x * tileSize.y bytes
tileMap.setPixels(std::span<const std::uint8_t>(circlePx), 0);
auto ditherPx = makeDitherGradientTile(tileSize);
tileMap.setPixels(std::span<const std::uint8_t>(ditherPx), 1);

tileMap.setMap(mapSize);                      // opt into tile-map mode
for (int row = 0; row < mapSize.y; ++row)
    for (int col = 0; col < mapSize.x; ++col)
        tileMap.setCell(col, row, static_cast<std::uint8_t>((col + row) % 2));

Nothofagus::TextureId tileMapTexId = canvas.addTexture(tileMap);
```

**Constraints / behavior:**
- Pass `mapSize == {0, 0}` to `setMap` to revert back to plain indirect/animation mode.
- `bellota.currentLayer()` is unused for tilemap textures — per-cell layer choice is driven by the cell grid, not by a global layer index. Animation state machines should target non-tilemap `IndirectTexture` instances.
- `setCell` triggers `mMapDirty` and is hot-uploadable per-frame; per-pixel `setPixels` triggers `mAtlasDirty` for tile-graphic mutations.
- The palette is shared between the tile-map and indirect rendering paths — `setPallete` works the same way.
- `setMapBulk(span)` overwrites the entire cell grid in one shot from a row-major byte buffer of `mapSize.x * mapSize.y` layer indices. Faster than per-cell `setCell` when replacing a large region — one memcpy + one dirty-flag set, no per-cell bookkeeping.

### Huge tilemaps via `Tilemap` + `TilemapExplorer`

Single-`IndirectTexture` tilemaps scale poorly: any `setCell` re-uploads the entire world's map texture, and the bellota's mesh covers the whole world even when only a small window is visible. For huge maps (open worlds, side-scrolling levels), use the **`Tilemap` + `TilemapExplorer` pair** instead. The world data lives once in a `Tilemap`; a `TilemapExplorer` owns a small pool of `IndirectTexture` + `Bellota` slots sized to the canvas viewport + a 1-chunk margin. Slots are anchored to pool indices; world chunks rotate through them as the camera scrolls. Only border slots crossing into/out of explorer get their cell data rewritten — smooth scrolling within a chunk is a zero-rebind frame.

```cpp
// Build the tile graphics (palette indices, one std::vector per atlas layer).
std::vector<std::vector<std::uint8_t>> tileGraphics{ /* layer 0, layer 1, ... */ };

// createTilemap registers both the Tilemap (world data) and the TilemapExplorer
// (pooled renderer) in one shot and returns handles for both.
Nothofagus::TilemapHandles handles = Nothofagus::createTilemap(
    canvas,
    /*mapSize  */ glm::ivec2{256, 256},   // world cells
    /*chunkSize*/ glm::ivec2{32, 32},     // cells per pool slot
    /*tileSize */ glm::ivec2{16, 16},     // pixels per cell
    palette,
    std::span<const std::vector<std::uint8_t>>(tileGraphics));

// Edit the world at world-cell coordinates — the owning chunk's generation
// bumps, the pool slot displaying it (if any) re-syncs next frame.
canvas.tilemap(handles.tilemapId).setCell({worldCol, worldRow}, layerIndex);

// Guard arbitrary coordinates against the world extent before editing.
if (canvas.tilemap(handles.tilemapId).inBounds({worldCol, worldRow}))
    canvas.tilemap(handles.tilemapId).setCell({worldCol, worldRow}, layerIndex);

// Pan the explorer via the camera (world pixels; (0,0) = world origin centered).
canvas.tilemapExplorer(handles.explorerId).setCamera({scrollX, scrollY});
```

**How it works:**
- `Tilemap` is internally a single `IndirectTexture` shaped to the full world (atlas + palette + `setMap(mapSize)` cell grid) plus per-chunk generation counters. The cache texture is never registered with the canvas, so no GPU resources are allocated — `IndirectTexture` is reused purely for its storage layout and tested mutation methods (`setCell` / `cell` / `setMapBulk`). Use `tilemap.cacheTexture()` to inspect or clone the underlying texture.
- `TilemapExplorer` is registered against a `TilemapId`; on registration the canvas allocates a `ceil(screenSize / chunkPixelSize) + 2` grid of pool slots. Each slot is an `IndirectTexture` (with its own copy of the atlas + palette, chunk-sized map storage) plus a `Bellota`. Both are **explorer-managed**: calling `canvas.removeBellota`/`canvas.removeTexture` on those ids fires a `debugCheck`. Use `canvas.removeTilemapExplorer(explorerId)` to tear the pool down.
- Per-frame pre-pass (runs between the user update callback and the texture-upload pass): for each explorer, compute which world chunk each slot should display based on the camera; for any slot whose desired chunk changed (or whose chunk's generation advanced), memcpy the chunk's cells into the slot's IndirectTexture via `setMapBulk` and reposition the slot's bellota. The existing dirty-upload path then re-uploads only those small chunk map textures.
- Renderer learns nothing new — pool slots flow through the existing 3-binding tilemap path. No shader, backend, or render-loop changes.
- **Pool resize on `setScreenSize`:** the pre-pass also compares the canvas's current `screenSize()` against the size the pool was built for. If they differ, the pool is torn down and rebuilt against the new size in one frame, then chunk-synced — `canvas.setScreenSize(...)` "just works" with active views. Window resize / fullscreen don't trigger this because the letterbox preserves the logical canvas; only explicit `setScreenSize` does.

**Memory cost:**
- Atlas: 1 copy in `Tilemap` + 1 copy per pool slot (~50 copies for typical viewports).
- Palette: same — 1 + ~pool.
- World cell grid: 1 byte per world cell, held once in `Tilemap`.
- Independent of world size beyond the cell grid itself: a 1000×1000-cell world (~1 MB cell grid) uses ~50 IndirectTextures and ~50 bellotas, regardless of how big the world is.

**Lifecycle rules:**
- `addTilemap` / `addTilemapExplorer` register the data and the renderer; `createTilemap` is a convenience that calls both.
- `removeTilemap(tilemapId)` fails if any `TilemapExplorer` still references it.
- `removeTilemapExplorer(explorerId)` removes all pool bellotas and textures it owns.
- Multiple `TilemapExplorer` instances may reference the same `Tilemap` (e.g., main explorer + mini-map explorer); each polls per-chunk generation counters independently.

**Camera convention (v1):** `setCamera(offset)` sets the world-pixel coordinate that appears at the canvas center. `(0, 0)` = world origin centered. The `Tilemap`'s coordinate space is bottom-left = `(0, 0)` cell, top-right = `(mapSize.x - 1, mapSize.y - 1)`.

**Deferred:** streaming (world cell grid eviction to disk); RTT-targeted tilemap rendering; shader-scrolled single-draw fast path; per-cell partial GPU upload inside a chunk's map texture; direct rendering of small tilemaps via `canvas.addTexture(tilemap.cacheTexture())`.
- **Custom meshes are forbidden on tile-map textures.** The tile-map shader treats incoming UVs as `[0, 1]` over the full tile-map extent and quantises to cell indices, so only the engine-generated auto-quad's UVs sample correctly. `addBellota`, `setMesh`, and `setTexture` enforce the restriction via `debugCheck`.

### Custom meshes

Bellotas draw the implicit centered quad sized to their texture by default. Pass a `MeshId` as the third constructor argument to draw arbitrary triangle geometry instead. The texture is still required — it supplies the pixels the existing shader samples.

```cpp
// Build a triangle mesh in (x, y) pixels with UVs in [0, 1].
Nothofagus::Mesh mesh;
mesh.vertices = {
    {{-10.0f, -10.0f}, {0.0f, 1.0f}},
    {{ 10.0f, -10.0f}, {1.0f, 1.0f}},
    {{  0.0f,  14.0f}, {0.5f, 0.0f}},
};
mesh.indices = {0, 1, 2};

Nothofagus::MeshId meshId = canvas.addMesh(mesh);
// Move-overload also available for callers that can hand off ownership:
//   auto meshId = canvas.addMesh(std::move(mesh));

// Attach the custom mesh; the texture supplies pixels via the same shader.
Nothofagus::BellotaId id = canvas.addBellota({{{x, y}}, texId, meshId});

// Swap geometry mid-frame; the previous mesh becomes eligible for GC.
canvas.setMesh(id, otherMeshId);

// Read-only mesh access (auto-quad or user mesh, transparent).
const Nothofagus::Mesh& currentByBellota = canvas.mesh(id);      // resolves via bellota.meshId()
const Nothofagus::Mesh& currentByMeshId  = canvas.mesh(meshId);  // direct handle lookup

// Explicit removal of a user mesh — must be unreferenced (debugCheck enforces this).
canvas.removeMesh(meshId);

// Disable per-frame auto-GC during bulk loading (re-enable when done).
canvas.setAutoRemoveUnusedMeshes(false);
```

**Storage model:**
- `Vertex { glm::vec2 position; glm::vec2 uv; }` ([include/mesh.h](include/mesh.h)) is the fixed vertex layout — matches the shader binding for both OpenGL and Vulkan backends. No custom attributes.
- Every bellota carries a `MeshId`. If the user does not supply one (`Bellota(Transform, TextureId)`), the canvas materialises an **auto-quad** sized to the texture at `addBellota` time and stamps the id onto the stored bellota. There is no second mesh storage path — both flow through `MeshContainer` / `MeshPack`.
- All bellotas referencing the same `MeshId` share **one GPU upload**. Lazy upload happens once on the next frame; subsequent registrations are zero-cost.
- `MeshUsageMonitor` tracks references analogously to `TextureUsageMonitor`. When the last bellota referencing a `MeshId` goes away, the mesh is freed by `clearUnusedMeshes()` on the following frame (auto-GC is on by default; `setAutoRemoveUnusedMeshes(false)` pauses it for bulk loading, same pattern as `setAutoRemoveUnusedTextures`).
- `setTexture(bellotaId, newTexId)` regenerates the auto-quad sized to the new texture **only when the bellota uses an engine-allocated auto-quad**. User-supplied meshes are left untouched on texture change — that's the user's choice.
- `removeMesh(meshId)` is for user-registered meshes only. Removing an auto-quad fires `debugCheck`; auto-quads are managed exclusively by the canvas.
- **Custom meshes cannot be combined with tile-map textures.** `addBellota` (with a user MeshId), `setMesh`, and `setTexture` all `debugCheck`-reject the combination because the tile-map shader requires the auto-quad's exact UV invariant. See [the tile-map constraints](#tile-maps) for details.

### Animations

Multi-layer `IndirectTexture` stores frames as layers. `AnimationStateMachine` drives `bellota.currentLayer()` automatically each frame.

```cpp
// 1. Build a multi-layer texture (3rd arg = layer count)
Nothofagus::IndirectTexture tex({w, h}, glm::vec4(0,0,0,1), numLayers);
tex.setPallete(palette).setPixels({/* frame 0 */}, 0).setPixels({/* frame 1 */}, 1); // ...
Nothofagus::TextureId texId = canvas.addTexture(tex);
Nothofagus::BellotaId id    = canvas.addBellota({{{x, y}}, texId});

// 2. Define AnimationState objects (layers, times_ms, name) — must outlive the machine
AnimationState idleState({0, 1, 2}, {100.0f, 100.0f, 100.0f}, "idle");
AnimationState runState ({3, 4},    {80.0f,  80.0f},           "run");

// 3. Build state machine bound to the bellota reference
AnimationStateMachine machine(canvas.bellota(id));
machine.addState("idle", &idleState);
machine.addState("run",  &runState);

// 4. Define named transition edges: (fromState, transitionName, toState)
machine.newAnimationTransition("idle", "start_running", "run");
machine.newAnimationTransition("run",  "stop",          "idle");

// 5. Set initial state — required before first update()
machine.setState("idle");

// 6. Per frame:
machine.update(dt);

// 7. Trigger transitions:
machine.transition("start_running");   // fire named edge from current state
machine.goToState("idle");             // direct jump + reset (bypasses transition graph)
```

`AnimationState` loops automatically (after the last frame it restarts from index 0). `goToState` calls `reset()` on the target state before switching; `transition` does the same.

### Display and viewport

```cpp
// Query current game viewport (updated each frame — valid inside canvas.run() callback)
Nothofagus::ViewportRect viewport = canvas.gameViewport();
// viewport.x, viewport.y           — bottom-left offset in framebuffer pixels (OpenGL convention: y from bottom)
// viewport.width, viewport.height  — game area dimensions in framebuffer pixels
```

### Render to texture

Render sprites into an off-screen texture (a **render target**) and then sample that texture from another bellota — the basis for diegetic UI, mirrors, mini-maps, post-processing, etc.

```cpp
// Create a 64×64 RTT with a semi-transparent dark-blue clear color.
Nothofagus::RenderTargetId renderTargetId = canvas.addRenderTarget({64, 64});
canvas.setRenderTargetClearColor(renderTargetId, {0.0f, 0.0f, 0.0f, 0.5f});
Nothofagus::TextureId renderTargetTextureId = canvas.renderTargetTexture(renderTargetId);

// Display bellota — samples the RTT and shows it on the main canvas.
Nothofagus::BellotaId displayId = canvas.addBellota({{{64.0f, 64.0f}}, renderTargetTextureId});

canvas.run([&](float dt) {
    // Schedule these bellotas to be drawn into the RTT this frame.
    // They are rendered in the RTT's coordinate space (origin bottom-left, size 64×64)
    // and also appear on the main canvas at their own positions (dual rendering).
    canvas.renderTo(renderTargetId, {redBellotaId, blueBellotaId});
});
```

**Rules:**
- Call `renderTo(...)` from inside the `run()` / `tick()` update callback. It enqueues the pass; execution happens before the main draw each frame.
- The bellotas passed to `renderTo` render **both** into the RTT and onto the main canvas — they don't disappear from the main explorer.
- The RTT uses its own coordinate space: bottom-left = (0, 0), top-right = (width, height), in RTT pixels. The bellotas' own `x, y` are interpreted in that space when rendered into the RTT.
- `renderTargetTexture(renderTargetId)` returns a `TextureId` proxy valid for the lifetime of the RTT. Do **not** call `removeTexture()` on it — the RTT owns the underlying GPU texture.
- `removeRenderTarget(renderTargetId)` frees the FBO / VkImage + framebuffer and the proxy texture in one call.

### Render ImGui into a render target

Draw an interactive ImGui panel into an RTT that a bellota samples — enabling *diegetic* UI (ImGui text and widgets living inside the game world).

```cpp
auto renderTargetId = canvas.addRenderTarget({160, 120});
canvas.setRenderTargetClearColor(renderTargetId, {0.02f, 0.04f, 0.12f, 1.0f});
auto displayId = canvas.addBellota({{{96.0f, 80.0f}}, canvas.renderTargetTexture(renderTargetId)});

// Bake a font from the canvas's built-in default source at a specific
// *logical* (game-canvas) pixel size for crisp glyphs inside the RTT.
// Returns a stable ImguiFontId; dedups by (sourceId, sizePx) — repeat
// calls with the same args return the same id.
Nothofagus::ImguiFontId diegeticId =
    canvas.bakeImguiFont(canvas.defaultImguiFontSourceId(), 12.0f);

float sliderValue = 0.42f;
int   clickCount  = 0;

canvas.run([&](float dt) {
    // Queue ImGui draws for the RTT. The callback runs on a secondary
    // ImGuiContext owned by this render target — state is isolated from
    // the main UI. The diegeticId is auto-pushed before the callback and
    // popped after, so the body never has to mention ImFont.
    canvas.renderImguiTo(renderTargetId, diegeticId, [&] {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(160, 120), ImGuiCond_Always);
        ImGui::Begin("In-World Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::SliderFloat("value", &sliderValue, 0.0f, 1.0f);
        if (ImGui::Button("click")) clickCount++;
        ImGui::End();
    });

    // Main-canvas ImGui draws normally on the main context.
    ImGui::Begin("stats"); ImGui::Text("clicks=%d", clickCount); ImGui::End();
});
```

If the panel doesn't have a specific font of its own, pass `canvas.defaultImguiFontId()` to render with the secondary-context default that the manager already sets up at construction:

```cpp
canvas.renderImguiTo(renderTargetId, canvas.defaultImguiFontId(), [&] { ... });
```

**How it works (multi-context design):**
- The ImGui-RTT flow is encapsulated in `ImguiRttManager` ([source/imgui_rtt_manager.h](source/imgui_rtt_manager.h)). It owns the per-frame queue of pending RTT passes and the per-RTT secondary `ImGuiContext` cache (`std::unordered_map<RenderTargetId, std::unique_ptr<ImGuiContext, ImGuiContextDeleter>>`). Lazy-create on first use; tear-down on `removeRenderTarget()` and in the `Canvas` destructor (before `mBackend.shutdown()`).
- The callback runs during the pre-main RTT pass phase on the secondary context — `ImGui::Begin/End/Text/...` calls inside it target that context's draw list only.
- The OpenGL ImGui backend is FBO-agnostic; the Vulkan backend's per-context pipeline is built against `mRttRenderPass`, so it is render-pass-compatible with `beginRttPass`. **No changes to `imgui_impl_opengl3.*` / `imgui_impl_vulkan.*` are required.**
- The platform backend (GLFW/SDL3) is skipped for secondary contexts — they run headless-style with `IO.DisplaySize` / `IO.DeltaTime` set manually. This means the feature works identically across GLFW+OpenGL, GLFW+Vulkan, SDL3+OpenGL, SDL3+Vulkan, and headless Vulkan.

**Font handling — `ImguiFontManager`, `ImguiFontSourceId`, `ImguiFontId`:**

`ImguiFontManager` ([source/imgui_font_manager.h](source/imgui_font_manager.h), held by `ImguiRttManager`) owns the entire ImGui-font lifecycle for a Canvas: the main HiDPI font (used by main-canvas UI), the secondary-context default font, every registered TTF buffer, every baked `(source, size)` pair, and the deferred bake/remove queue + atlas-rebuild flow. Two `IndexedContainer`s back the manager — one of `FontSource` (each registered TTF buffer + its `GlyphRange`) keyed by `ImguiFontSourceId` ([include/imgui_font_source_id.h](include/imgui_font_source_id.h)), and one of `FontEntry` (each baked size, with a per-entry `sourceId`) keyed by `ImguiFontId` ([include/imgui_font_id.h](include/imgui_font_id.h)). Atlas glyphs are owned by the shared `ImFontAtlas`; the manager stores non-owning observer pointers and `rebakeAll()` patches them in place across rebuilds.

- At canvas construction, `ImguiFontManager::initialize(contentScale)` registers the embedded TTF as the default source (id exposed via `Canvas::defaultImguiFontSourceId()`), adds the main HiDPI font at `imguiFontSize * contentScale * contentScale` for crisp DPI-aware glyphs on the main UI, then bakes the unscaled `imguiFontSize` from the default source and registers it as `io.FontDefault` for every secondary RTT context (id exposed via `Canvas::defaultImguiFontId()`). Result: ImGui text inside an RTT renders at its logical pixel height *in RTT pixels* — OS DPI scaling has no meaning in the game-canvas pixel grid, and the secondary-context default deliberately ignores it.
- `Canvas::addImguiFontSource(span<const std::byte>, GlyphRange) -> ImguiFontSourceId` registers a user-supplied TTF. Bytes are copied internally; safe to call before `run()` or from inside an update / `renderImguiTo` callback. `GlyphRange` (also in `imgui_font_source_id.h`) is a tiny enum — `Default`, `Greek`, `Cyrillic`, `Korean`, `Japanese`, `ChineseFull`, `ChineseSimplifiedCommon`, `Thai`, `Vietnamese` — that maps to `ImFontAtlas::GetGlyphRangesXxx()` inside the implementation, keeping `imgui.h` out of the public surface. `Canvas::removeImguiFontSource(sourceId)` cascade-removes every `ImguiFontId` baked from that source via the same deferred path; removing the default source is forbidden (`debugCheck`).
- `Canvas::bakeImguiFont(ImguiFontSourceId, float sizePx) -> ImguiFontId` bakes from a registered source at a logical size. Repeat calls with the same `(sourceId, sizePx)` return the same id (the manager dedups by `(sourceId, sizePx)` — ImGui itself does not dedupe `AddFontFromMemoryTTF` calls). Pass `defaultImguiFontSourceId()` to bake from the embedded TTF.
- `Canvas::removeImguiFont(ImguiFontId)` schedules a full atlas rebuild at the start of the next frame: `ImFontAtlas::Clear()` + re-add main HiDPI font + `rebakeAll()` for surviving entries (each re-baked from its attributed source) + secondary-context `io.FontDefault` refresh + GPU font texture re-upload via `ActiveBackend::rebuildImguiFontTexture()`. Orchestration lives on `ImguiRttManager::drainPendingFontOps(contentScale)` (called once per frame from `runOneFrame`); the cache-side parts (Clear + re-add + rebake) live on `ImguiFontManager::drainPendingOpsAndRebuildAtlas(contentScale)`. The id passed to remove is invalidated; every other id survives the rebuild because each entry's `ImFont*` is patched in place — `Canvas::renderImguiTo(rtId, otherId, cb)` keeps working without intervention. The same drain handles `RemoveSource` ops before per-id removes, so cascade-cleanup of all entries baked from a removed source is automatic.
- `Canvas::pushImguiFont(ImguiFontId) / popImguiFont()` mid-callback override the panel's font without touching `ImFont`. `Canvas::isImguiFontReady(id)` guards against the one-frame deferred-bake window after `bakeImguiFont` returns; `Canvas::getImguiFontPtr(id) -> ImFont*` is the escape hatch for ImGui APIs that take an `ImFont*` directly (`ImGui::CalcTextSizeA` etc.).
- The ID-stable design: only `removeImguiFont(id)` (or a cascade from `removeImguiFontSource`) invalidates `id`. Atlas rebuilds (triggered by any other id's removal) leave every other id valid — the underlying pointer changes but `Canvas::renderImguiTo`, `pushImguiFont`, etc. resolve through the id automatically. See [imgui_font_removal_analysis.md](imgui_font_removal_analysis.md) for the rationale behind eager full rebuild vs alternatives.

**Limitations (v1):**
- **Input is not forwarded** to the secondary context — widgets render correctly but mouse/keyboard events only reach the main context. Forwarding canvas-space mouse coords into the RTT's `IO.MousePos` is a natural follow-up.
- Each secondary context has its own ID stack, window state, and widget values — widgets with the same name in different RTTs do not collide, and neither inherits state from the main UI.
- **Atlas rebuild on remove rasterises every surviving glyph again.** `removeImguiFont` is meant to be a user-driven, infrequent op; cycling it once per frame would be wasteful (ImGui has no incremental remove and no way to retain glyph data across `Clear()`). For long-running apps that don't actually need to free atlas memory, leaving baked fonts alive is the cheaper path.

### Screenshot

`takeScreenshot()` reads the front buffer (the last fully rendered and swapped frame) and returns a `DirectTexture` with the game viewport's RGBA pixels, flipped to top-to-bottom row order.

```cpp
// Call from within the update() callback:
Nothofagus::DirectTexture screenshot = canvas.takeScreenshot();

// Access raw RGBA bytes for saving with an external image library (e.g. stb_image_plus):
Nothofagus::TextureData data = screenshot.generateTextureData();
std::span<std::uint8_t> span = data.getDataSpan(); // width * height * 4 bytes, top-to-bottom

// The screenshot can also be loaded back into the canvas as a texture:
Nothofagus::TextureId texId = canvas.addTexture(screenshot);
```

**OpenGL note:** reads from `GL_FRONT` — valid only while an OpenGL context is current (i.e. inside `canvas.run()`). On the very first frame before any swap, the front buffer content is undefined.

**Vulkan windowed:** blits from the swapchain image through an intermediate R8G8B8A8 image (handles B8G8R8A8 format conversion) to a CPU-visible staging buffer.

**Vulkan headless:** copies directly from the offscreen R8G8B8A8 image to a staging buffer via `vkCmdCopyImageToBuffer` — no intermediate blit or format conversion needed.

## Naming Conventions (C++)

- Variables and functions: **camelCase**
- Member variables: **mCamelCase** prefix
- No abbreviated names — use full descriptive names (e.g. `framebufferWidth` not `fbW`, `viewportX` not `vpX`, `canvasAspectRatio` not `aspect`, `renderTargetPack` not `rtPack`, `renderTargetId` not `rtId`)
- Comments and identifiers use **American English** spelling (`color` not `colour`, `center` not `centre`, `initialize` not `initialise`, `behavior` not `behaviour`)

## Important Details

- **Default canvas size**: 256×240 pixels, 4px scale → 1024×960 window
- **Depth/Z-ordering**: `bellota.mDepthOffset` (-128 to 127)
- **Opacity**: `bellota.mOpacity` (0.0–1.0)
- **Layers**: multi-layer textures use `bellota.currentLayer()` (managed automatically by `AnimationStateMachine::update()`, or set manually)
- **Angles**: degrees, not radians
- **MSVC/clang-cl workaround**: `FMT_UNICODE=0` in CMake for spdlog on Windows
- **C++ standard**: C++20 required
- **Aspect ratio**: in fullscreen and on manual window resize, game content is letterboxed/pillarboxed to preserve the canvas aspect ratio — black bands fill unused screen area. Viewport is recomputed every frame from `mWindow->getFramebufferSize()`, so it adapts automatically.
- **Automatic texture GC**: `TextureUsageMonitor` tracks which textures are referenced by bellotas. After each `update()` callback, `clearUnusedTextures()` automatically removes any texture not referenced by at least one bellota. Calling `canvas.removeTexture()` on a texture still in use triggers a `debugCheck` assert. Use `canvas.setTexture(bellotaId, newTexId)` to swap textures — the old one is marked unused and removed automatically next frame.

## Examples Reference

| File | Demonstrates |
|------|-------------|
| `hello_nothofagus.cpp` | Basic setup, IndirectTexture, ImGui, rotation |
| `hello_animation.cpp` | AnimationState frame-by-frame |
| `hello_animation_state_machine.cpp` | Full FSM with WASD transitions |
| `hello_direct_texture.cpp` | DirectTexture (raw RGBA) |
| `hello_layers.cpp` | Depth-based layering |
| `hello_text.cpp` | Text rendering |
| `hello_tint.cpp` | Color tinting |
| `test_keyboard.cpp` | Keyboard input handling |
| `test_gamepad.cpp` | Gamepad input: stick movement, D-pad, buttons, ImGui status |
| `test_create_destroy.cpp` | Object lifecycle |
| `hello_screenshot.cpp` | `takeScreenshot()` — capture frame as DirectTexture, display thumbnail |
| `hello_headless.cpp` | Headless mode + `tick()` — no window, manual frame stepping, screenshot to terminal |
| `hello_tilemap.cpp` | Tile-map mode of `IndirectTexture` — `setMap` + `setCell` over a layered atlas |
| `hello_tilemap_huge.cpp` | Huge tilemaps via `Tilemap` + `TilemapExplorer` pool — WASD camera, teleport, recreate, live memory breakdown, stress controls (auto-pan + edits/frame) |
| `hello_mesh.cpp` | Custom triangle meshes via `addMesh` + `Bellota(Transform, TextureId, MeshId)` — register geometry once, attach to bellotas, swap with `setMesh` |
| `hello_render_to_texture.cpp` | `addRenderTarget` / `renderTo` — sprites drawn into an off-screen texture sampled by another bellota |
| `hello_nested_render_targets.cpp` | Nested RTTs — one render target's output feeds another |
| `hello_imgui_rtt.cpp` | `renderImguiTo` — diegetic ImGui panel drawn into an RTT, sampled by a rotating bellota |
| `hello_custom_font.cpp` | User-supplied TTF via `addImguiFontSource` — typeable path field, editable text, integer min/max + slider for size, default-vs-user side-by-side with `TextWrapped` |

## Tests

Enable with `-DNOTHOFAGUS_BUILD_TESTS=ON`. Two independent groups, each behind its own sub-option (both default ON when tests are enabled):

| Group | Folder | Sub-option | Stack |
|-------|--------|------------|-------|
| Visual (pixel-level golden-image comparison) | [tests/visual/](tests/visual/) | `NOTHOFAGUS_BUILD_TESTS_VISUAL` | Catch2 + render backend + golden-image infrastructure |
| Nonvisual (CPU-only data/logic checks) | [tests/nonvisual/](tests/nonvisual/) | `NOTHOFAGUS_BUILD_TESTS_NONVISUAL` | Catch2 only |

Run via CTest from the build directory. Both groups use Catch2 (`catch_discover_tests` registers each `TEST_CASE` as a separate CTest entry); the visual group additionally requires a render backend and the golden-image helpers in [tests/visual/golden_image.h](tests/visual/golden_image.h).

## Dependencies (third_party/ submodules)

- **glfw** — window + input (default backend)
- **SDL** — window + input (SDL3 backend, used when `NOTHOFAGUS_WINDOW_BACKEND=SDL3`)
- **glad** — OpenGL loader (3.3 core)
- **glm** — math (vec2, vec3, mat3, etc.)
- **imgui** — immediate-mode GUI
- **spdlog** — logging
- **font8x8** — embedded bitmap font
- **imgui_cmake** — CMake wrapper for imgui
