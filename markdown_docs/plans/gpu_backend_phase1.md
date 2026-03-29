# GPU Backend Abstraction — Phase 1: Extract OpenGLBackend

## Goal
Introduce a rendering backend abstraction layer so OpenGL and (future) Vulkan can coexist. Phase 1 extracts all OpenGL-specific code from `canvas_impl.cpp` into a self-contained `OpenGLBackend`, leaving `canvas_impl` GPU-agnostic.

## Design Decisions
- **Compile-time polymorphism** via C++20 `concept RenderBackend<T>` — zero virtual dispatch overhead, backend selected at build time through `NOTHOFAGUS_BACKEND_VULKAN` CMake option + `using ActiveBackend = OpenGLBackend` alias in `canvas_impl.h`.
- **D-types as opaque handles** — `DMesh`, `DTexture`, `DRenderTarget` become simple structs holding a `std::size_t id` (plus `glm::ivec2 size` on `DRenderTarget`). They are backend-agnostic identifiers; no GL state lives in them.
- **OpenGL-prefixed concrete types** — `OpenGLMesh`, `OpenGLTexture`, `OpenGLRenderTarget` hold the actual GL handles and live entirely inside `OpenGLBackend`'s private maps. Never visible to `canvas_impl`.

## New Files

| File | Purpose |
|---|---|
| `source/render_backend.h` | `RenderBackend<T>` concept, `SpriteDrawParams`, `ScreenshotPixels` |
| `source/opengl_backend.h/.cpp` | Full OpenGL implementation satisfying the concept |
| `source/opengl_mesh.h/.cpp` | GL VAO/VBO/EBO wrapper (former `DMesh` internals) |
| `source/opengl_texture.h/.cpp` | GL texture wrapper (former `DTexture` internals) |
| `source/opengl_render_target.h/.cpp` | GL FBO wrapper (former `DRenderTarget` internals) |

## Deleted Files
`source/dmesh.cpp`, `source/dtexture.cpp`, `source/drender_target.cpp` — opaque handle structs need no implementation.

## Modified Files

**`source/dmesh.h` / `dtexture.h` / `drender_target.h`** — stripped to opaque handle structs:
```cpp
struct DMesh        { std::size_t id; };
struct DTexture     { std::size_t id; };
struct DRenderTarget{ std::size_t id; glm::ivec2 size; };
```

**`source/canvas_impl.h`** — removed `unsigned int mShaderProgram`; added `ActiveBackend mBackend` (compile-time selected via `#if NOTHOFAGUS_BACKEND_VULKAN`).

**`source/canvas_impl.cpp`** — all direct GL calls replaced with `mBackend.*` calls:
- Constructor: removed GLAD init, shader compilation, ImGui GL init → replaced with `mBackend.initialize()`
- `removeBellota` / `removeTexture` / `markTextureAsDirty`: call `mBackend.freeMesh` / `freeTexture` before clearing packs
- `setTextureMinFilter` / `setTextureMagFilter`: delegate to `mBackend.setTextureMinFilter/MagFilter`
- `removeRenderTarget`: call `mBackend.freeRenderTarget(dRenderTarget, proxyDTexture)` which handles the shared GL handle correctly
- `run()` loop: lazy-init calls `mBackend.uploadTexture` / `uploadMesh` / `createRenderTarget` / `getRenderTargetTexture`; frame calls `beginFrame` → `imguiNewFrame` → (RTT passes via `beginRttPass` / `drawSprite` / `endRttPass`) → `beginMainPass` → `drawSprite` per bellota → `endFrame`
- Teardown: explicit `mBackend.freeMesh` / `freeRenderTarget` / `freeTexture` loops before `mBackend.shutdown()`
- `takeScreenshot()`: delegates to `mBackend.takeScreenshot()`, reconstructs `DirectTexture` from returned `ScreenshotPixels`

**`source/bellota_container.h`** / **`texture_container.h`** / **`render_target_container.h`** — `clear()` methods simplified to null out optionals only; GPU resource deallocation is now the caller's responsibility via the backend.

**`CMakeLists.txt`** — removed deleted D-type `.cpp` files; added `NOTHOFAGUS_BACKEND_VULKAN` option; added new OpenGL backend sources under an `if/else` block so they are excluded when the Vulkan option is set.

## Key Design Details

- **Proxy texture lifetime**: `freeRenderTarget(DRenderTarget, DTexture proxyTexture)` removes the proxy entry from the backend's internal texture map *without* calling `glDeleteTextures` — the GL handle is owned by the render target's color attachment and freed as part of FBO teardown.
- **Scissor fix preserved**: `beginRttPass` disables `GL_SCISSOR_TEST` (RTT FBOs use `(0,0)` origin); `endFrame` restores full-framebuffer viewport before ImGui rendering.
- **`static_assert(RenderBackend<OpenGLBackend>)`** at the bottom of `opengl_backend.h` gives a compile-time guarantee that the implementation satisfies the concept.

## Result
All 13 examples build and link cleanly. No behavioral changes — identical rendering to the pre-refactor state.
