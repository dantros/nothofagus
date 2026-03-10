# Nothofagus - Claude Code Guide

## Project Overview

Pixel art real-time renderer built on OpenGL 3.3, written in C++20. Outputs a static library (`nothofagus`) consumed by user projects.

## Key Terminology

- **Bellota** ("acorn") — a drawable sprite/element on screen
- **IndirectTexture** — paletted texture: pixels hold color indices into a `ColorPallete`
- **DirectTexture** — raw RGBA texture
- **Transform** — position (`mLocation`), scale (`mScale`), rotation (`mAngle` in degrees)
- **Controller** — keyboard input handler mapping `KeyboardTrigger` → `Action` callbacks
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

### Input
```cpp
controller.registerAction({Key::W, DiscreteTrigger::Press}, [&]() { ... });
canvas.run(update, controller);
```

### Animations
```cpp
AnimationStateMachine machine(canvas, bellotaId);
machine.addState({"idle", {0,1,2}, {0.1f, 0.1f, 0.1f}});
machine.addTransition({"idle", "run"}, "start_running");
machine.transition("start_running");
machine.update(dt);
```

## Important Details

- **Default canvas size**: 256×240 pixels, 4px scale → 1024×960 window
- **Depth/Z-ordering**: `bellota.mDepthOffset` (-128 to 127)
- **Opacity**: `bellota.mOpacity` (0.0–1.0)
- **Layers**: multi-layer textures use `bellota.mCurrentLayer`
- **Angles**: degrees, not radians
- **MSVC workaround**: `FMT_UNICODE=0` in CMake for spdlog on Windows
- **C++ standard**: C++20 required

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
| `test_keyboard.cpp` | Input handling |
| `test_create_destroy.cpp` | Object lifecycle |

## Dependencies (third_party/ submodules)

- **glfw** — window + input
- **glad** — OpenGL loader (3.3 core)
- **glm** — math (vec2, vec3, mat3, etc.)
- **imgui** — immediate-mode GUI
- **spdlog** — logging
- **font8x8** — embedded bitmap font
- **imgui_cmake** — CMake wrapper for imgui
