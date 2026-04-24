# Nothofagus - Claude Code Guide

## Project Overview

Pixel art real-time renderer with OpenGL 3.3 and Vulkan backends, written in C++20. Outputs a static library (`nothofagus`) consumed by user projects.

## Key Terminology

- **Bellota** ("acorn") — a drawable sprite/element on screen
- **IndirectTexture** — paletted texture: pixels hold color indices into a `ColorPallete`
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
- The bellotas passed to `renderTo` render **both** into the RTT and onto the main canvas — they don't disappear from the main view.
- The RTT uses its own coordinate space: bottom-left = (0, 0), top-right = (width, height), in RTT pixels. The bellotas' own `x, y` are interpreted in that space when rendered into the RTT.
- `renderTargetTexture(renderTargetId)` returns a `TextureId` proxy valid for the lifetime of the RTT. Do **not** call `removeTexture()` on it — the RTT owns the underlying GPU texture.
- `removeRenderTarget(renderTargetId)` frees the FBO / VkImage + framebuffer and the proxy texture in one call.

### Render ImGui into a render target

Draw an interactive ImGui panel into an RTT that a bellota samples — enabling *diegetic* UI (ImGui text and widgets living inside the game world).

```cpp
auto renderTargetId = canvas.addRenderTarget({160, 120});
canvas.setRenderTargetClearColor(renderTargetId, {0.02f, 0.04f, 0.12f, 1.0f});
auto displayId = canvas.addBellota({{{96.0f, 80.0f}}, canvas.renderTargetTexture(renderTargetId)});

float sliderValue = 0.42f;
int   clickCount  = 0;

canvas.run([&](float dt) {
    // Queue ImGui draws for the RTT. The callback runs on a secondary ImGuiContext
    // owned by this render target — state is isolated from the main UI.
    canvas.renderImguiTo(renderTargetId, [&] {
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

**How it works (multi-context design):**
- Each RTT that calls `renderImguiTo` owns its own secondary `ImGuiContext`, lazily created on first use and torn down in `removeRenderTarget()` / the `Canvas` destructor. The font atlas is shared with the main context.
- The callback runs during the pre-main RTT pass phase on the secondary context — `ImGui::Begin/End/Text/...` calls inside it target that context's draw list only.
- The OpenGL ImGui backend is FBO-agnostic; the Vulkan backend's per-context pipeline is built against `mRttRenderPass`, so it is render-pass-compatible with `beginRttPass`. **No changes to `imgui_impl_opengl3.*` / `imgui_impl_vulkan.*` are required.**
- The platform backend (GLFW/SDL3) is skipped for secondary contexts — they run headless-style with `IO.DisplaySize` / `IO.DeltaTime` set manually. This means the feature works identically across GLFW+OpenGL, GLFW+Vulkan, SDL3+OpenGL, SDL3+Vulkan, and headless Vulkan.

**Limitations (v1):**
- **Input is not forwarded** to the secondary context — widgets render correctly but mouse/keyboard events only reach the main context. Forwarding canvas-space mouse coords into the RTT's `IO.MousePos` is a natural follow-up.
- Each secondary context has its own ID stack, window state, and widget values — widgets with the same name in different RTTs do not collide, and neither inherits state from the main UI.

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
| `hello_render_to_texture.cpp` | `addRenderTarget` / `renderTo` — sprites drawn into an off-screen texture sampled by another bellota |
| `hello_nested_render_targets.cpp` | Nested RTTs — one render target's output feeds another |
| `hello_imgui_rtt.cpp` | `renderImguiTo` — diegetic ImGui panel drawn into an RTT, sampled by a rotating bellota |

## Dependencies (third_party/ submodules)

- **glfw** — window + input (default backend)
- **SDL** — window + input (SDL3 backend, used when `NOTHOFAGUS_WINDOW_BACKEND=SDL3`)
- **glad** — OpenGL loader (3.3 core)
- **glm** — math (vec2, vec3, mat3, etc.)
- **imgui** — immediate-mode GUI
- **spdlog** — logging
- **font8x8** — embedded bitmap font
- **imgui_cmake** — CMake wrapper for imgui
