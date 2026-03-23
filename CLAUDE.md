# Nothofagus - Claude Code Guide

## Project Overview

Pixel art real-time renderer built on OpenGL 3.3, written in C++20. Outputs a static library (`nothofagus`) consumed by user projects.

## Key Terminology

- **Bellota** ("acorn") — a drawable sprite/element on screen
- **IndirectTexture** — paletted texture: pixels hold color indices into a `ColorPallete`
- **DirectTexture** — raw RGBA texture
- **Transform** — position (`mLocation`), scale (`mScale`), rotation (`mAngle` in degrees)
- **Controller** — keyboard and mouse input handler; maps `KeyboardTrigger`/`MouseButtonTrigger` → `Action` callbacks and tracks mouse position as `glm::vec2`
- **MouseButton** — enum with values `Left`, `Middle`, `Right`
- **AnimationState** — frame sequence with per-frame durations, loops automatically
- **AnimationStateMachine** — manages multiple states with named event-based transitions

## Architecture

```
Canvas (public API)
└── CanvasImpl (Pimpl, hidden GLFW/OpenGL details)
    ├── IndexedContainer<BellotaPack>   → Bellota + Mesh + DMesh + Tint
    └── IndexedContainer<TexturePack>  → Texture + DTexture
```

- `include/` — public API headers
- `source/` — implementation + internal headers (never expose to users)
- `examples/` — standalone demo executables
- `third_party/` — git submodules (glfw, glad, glm, imgui, spdlog, font8x8)

## Build System

**Presets (CMakePresets.json):**

| Preset | Platform | Build |
|--------|----------|-------|
| `ninja-release` / `ninja-debug` | Windows | Ninja + MSVC |
| `ninja-release-examples` / `ninja-debug-examples` | Windows | Same + examples |
| `vs-debug` / `vs-debug-examples` | Windows | Visual Studio 17 2022 |
| `linux-debug` / `linux-release` | Linux | Unix Makefiles + GCC |
| `linux-debug-examples` / `linux-release-examples` | Linux | Same + examples |

**Build and install (examples):**
```bash
cmake --preset ninja-debug-examples
cd ../build/ninja-debug-examples/
ninja install
# Artifacts land in ../install/ninja-debug-examples/
```

**CMake options:**
- `NOTHOFAGUS_BUILD_EXAMPLES` — build demo apps (default OFF, enabled by `-examples` presets)
- `NOTHOFAGUS_INSTALL` — install artifacts (default OFF, presets set ON)
- `NOTHOFAGUS_BUILD_DOCS` — generate Doxygen docs (default OFF)

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
```

### Main loop
```cpp
canvas.run([&](float dt) {
    // ImGui calls go here
    canvas.bellota(id).transform().location() += ...;
});
```

### Keyboard input
```cpp
controller.registerAction({Key::W, DiscreteTrigger::Press}, [&]() { ... });
controller.deleteAction({Key::W, DiscreteTrigger::Press});
canvas.run(update, controller);
```

### Mouse input

Mouse position is delivered in **game canvas coordinates** (origin bottom-left, same space as bellota positions). Coordinate conversion from GLFW window coords through the letterbox viewport happens automatically inside `canvas.run()`.

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

Mouse button callbacks are dispatched via the same `processInputs()` queue as keyboard. The move and scroll callbacks fire immediately from their GLFW callbacks (outside the queue), so they can be called multiple times per frame.

### Animations
```cpp
AnimationStateMachine machine(canvas, bellotaId);
machine.addState({"idle", {0,1,2}, {0.1f, 0.1f, 0.1f}});
machine.addTransition({"idle", "run"}, "start_running");
machine.transition("start_running");
machine.update(dt);
```

### Display and viewport

```cpp
// Query current game viewport (updated each frame — valid inside canvas.run() callback)
Nothofagus::ViewportRect viewport = canvas.gameViewport();
// viewport.x, viewport.y           — bottom-left offset in framebuffer pixels (OpenGL convention: y from bottom)
// viewport.width, viewport.height  — game area dimensions in framebuffer pixels
```

## Naming Conventions (C++)

- Variables and functions: **camelCase**
- Member variables: **mCamelCase** prefix
- No abbreviated names — use full descriptive names (e.g. `framebufferWidth` not `fbW`, `viewportX` not `vpX`, `canvasAspectRatio` not `aspect`)

## Important Details

- **Default canvas size**: 256×240 pixels, 4px scale → 1024×960 window
- **Depth/Z-ordering**: `bellota.mDepthOffset` (-128 to 127)
- **Opacity**: `bellota.mOpacity` (0.0–1.0)
- **Layers**: multi-layer textures use `bellota.mCurrentLayer`
- **Angles**: degrees, not radians
- **MSVC workaround**: `FMT_UNICODE=0` in CMake for spdlog on Windows
- **C++ standard**: C++20 required
- **Aspect ratio**: in fullscreen and on manual window resize, game content is letterboxed/pillarboxed to preserve the canvas aspect ratio — black bands fill unused screen area. Viewport is recomputed every frame from `glfwGetFramebufferSize`, so it adapts automatically.

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
| `test_create_destroy.cpp` | Object lifecycle |

## Dependencies (third_party/ submodules)

- **glfw** — window + input
- **glad** — OpenGL loader (3.3 core)
- **glm** — math (vec2, vec3, mat3, etc.)
- **imgui** — immediate-mode GUI
- **spdlog** — logging
- **font8x8** — embedded bitmap font
- **imgui_cmake** — CMake wrapper for imgui
