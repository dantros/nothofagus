# Migration: GLFW → SDL3

This document records every change made when replacing GLFW with SDL3 as the windowing, OpenGL-context, and input backend for Nothofagus.

---

## Motivation

SDL3 was chosen over GLFW to:

- Broaden platform coverage (SDL3 targets Windows, Linux, macOS, iOS, Android, and more).
- Adopt SDL3's event-driven input model (no per-frame polling callbacks to manage).
- Align with SDL3's modern gamepad API (event-driven connect/disconnect, axis-motion events).
- Simplify HiDPI handling (single `float` scale factor instead of two-component content scale).

GLFW sources remain in `third_party/glfw/` but are **not** referenced by the build system.

---

## Prerequisites

SDL3 must be present as a git subtree at `third_party/SDL`:

```bash
git subtree add --prefix=third_party/SDL https://github.com/libsdl-org/SDL.git release-3.2.x --squash
```

---

## File-by-file changes

### `CMakeLists.txt` (root)

**Removed** the GLFW option block and `add_subdirectory`:

```cmake
# REMOVED:
option(GLFW_INSTALL "" OFF)
option(BUILD_SHARED_LIBS "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
add_subdirectory("third_party/glfw")
```

**Added** the SDL3 static-build block (before `add_subdirectory("third_party/imgui_cmake")`):

```cmake
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
add_subdirectory("third_party/SDL")
```

**In `target_include_directories`:** removed `"third_party/glfw/include"`.

**In `target_link_libraries`:** replaced `glfw` with `SDL3::SDL3-static`.

**In the install block:** added `hello_screenshot` to the installed example targets.

---

### `third_party/imgui_cmake/CMakeLists.txt`

Replaced the entire file. Key changes:

- `imgui_impl_glfw.cpp` → `imgui_impl_sdl3.cpp`
- Removed `GLFW_PATH` variable and GLFW include paths.
- Added `target_compile_definitions(imgui PRIVATE SDL_ENABLE_OLD_NAMES)` — **required** (see [Gotchas](#gotchas)).
- Added `target_link_libraries(imgui PRIVATE SDL3::SDL3-static)`.

```cmake
set(IMGUI_PATH "../imgui/")

add_library(imgui STATIC
    "${IMGUI_PATH}/imgui.cpp"
    "${IMGUI_PATH}/imgui_demo.cpp"
    "${IMGUI_PATH}/imgui_draw.cpp"
    "${IMGUI_PATH}/imgui_tables.cpp"
    "${IMGUI_PATH}/imgui_widgets.cpp"
    "${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_PATH}/backends/imgui_impl_sdl3.cpp"
)
set_property(TARGET imgui PROPERTY CXX_STANDARD 11)
target_include_directories(imgui PRIVATE "${IMGUI_PATH}")
target_compile_definitions(imgui PRIVATE SDL_ENABLE_OLD_NAMES)
target_link_libraries(imgui PRIVATE SDL3::SDL3-static)
```

---

### `include/keyboard.h`

The internal namespace was renamed to be implementation-neutral:

| Before | After |
|--------|-------|
| `using GLFWKeyCode = int` | `using InternalKeyCode = int` |
| `toGLFWKeyCode(Key)` | `toInternalKeyCode(Key)` |

Public `Key` enum is **unchanged**.

---

### `source/keyboard.cpp`

- `#include <GLFW/glfw3.h>` → `#include <SDL3/SDL.h>`
- All `GLFW_KEY_*` constants replaced with `SDL_SCANCODE_*`.

Non-obvious name differences:

| GLFW | SDL3 |
|------|------|
| `GLFW_KEY_EQUAL` | `SDL_SCANCODE_EQUALS` |
| `GLFW_KEY_ENTER` | `SDL_SCANCODE_RETURN` |
| `GLFW_KEY_CAPS_LOCK` | `SDL_SCANCODE_CAPSLOCK` |
| `GLFW_KEY_SCROLL_LOCK` | `SDL_SCANCODE_SCROLLLOCK` |
| `GLFW_KEY_NUM_LOCK` | `SDL_SCANCODE_NUMLOCKCLEAR` |
| `GLFW_KEY_KP_SUBTRACT` | `SDL_SCANCODE_KP_MINUS` |
| `GLFW_KEY_KP_ADD` | `SDL_SCANCODE_KP_PLUS` |
| `GLFW_KEY_KP_EQUAL` | `SDL_SCANCODE_KP_EQUALS` |
| `GLFW_KEY_LEFT_SUPER` | `SDL_SCANCODE_LGUI` |
| `GLFW_KEY_RIGHT_SUPER` | `SDL_SCANCODE_RGUI` |
| `GLFW_KEY_WORLD_1` / `WORLD_2` | `SDL_SCANCODE_UNKNOWN` |
| `GLFW_KEY_F25` | `SDL_SCANCODE_UNKNOWN` (SDL3 max is F24) |

---

### `include/mouse.h`

The internal function was renamed to be implementation-neutral:

| Before | After |
|--------|-------|
| `toGLFWMouseButton(MouseButton)` | `toInternalButtonCode(MouseButton)` |

Public `MouseButton` enum is **unchanged**.

---

### `source/mouse.cpp`

- `#include <GLFW/glfw3.h>` → `#include <SDL3/SDL.h>`
- `GLFW_MOUSE_BUTTON_LEFT/MIDDLE/RIGHT` → `SDL_BUTTON_LEFT/MIDDLE/RIGHT`
- Function renamed to `toInternalButtonCode`.

---

### `source/canvas_impl.cpp`

This is the largest change. A summary of each section:

#### Includes

```cpp
// Removed:
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>

// Added:
#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
```

#### `Window` struct

```cpp
// Before (GLFW):
struct Window { GLFWwindow* glfwWindow = nullptr; };

// After (SDL3):
struct Window
{
    SDL_Window*   sdlWindow   = nullptr;
    SDL_GLContext glContext   = nullptr;
    bool          shouldClose = false;
    std::unordered_map<SDL_JoystickID, SDL_Gamepad*> openGamepads;
};
```

#### Constructor

```cpp
// Initialisation
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

SDL_Window* sdlWindow = SDL_CreateWindow(title, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
SDL_GLContext glContext = SDL_GL_CreateContext(sdlWindow);
SDL_GL_MakeCurrent(sdlWindow, glContext);

// HiDPI: single float scale (GLFW used two-component content scale)
float scale = SDL_GetWindowDisplayScale(sdlWindow);

// GLAD
gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);

// ImGui backend
ImGui_ImplSDL3_InitForOpenGL(sdlWindow, glContext);
```

#### Destructor

```cpp
SDL_GL_DestroyContext(mWindow->glContext);
SDL_DestroyWindow(mWindow->sdlWindow);
SDL_Quit();
```

#### Monitor / fullscreen helpers

| GLFW | SDL3 |
|------|------|
| `glfwGetWindowMonitor` | `SDL_GetDisplayForWindow` |
| `glfwGetMonitors(&count)` | `SDL_GetDisplays(&count)` |
| `glfwGetVideoMode(monitor)` | `SDL_GetDesktopDisplayMode(displayId)` |
| `glfwSetWindowMonitor(…, w, h, refresh)` | `SDL_SetWindowFullscreen(window, true)` |
| `glfwSetWindowMonitor(…, 0)` (windowed) | `SDL_SetWindowFullscreen(window, false)` |
| `glfwGetWindowPos / glfwGetWindowSize` | `SDL_GetWindowPosition / SDL_GetWindowSize` |
| `glfwGetFramebufferSize` | `SDL_GetWindowSizeInPixels` |

#### `run()` — event loop

Replaced `glfwPollEvents()` plus all `glfwSet*Callback` registrations with a `SDL_PollEvent` loop:

```cpp
SDL_Event event;
while (SDL_PollEvent(&event))
{
    ImGui_ImplSDL3_ProcessEvent(&event);
    switch (event.type)
    {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        mWindow->shouldClose = true; break;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        // ignore auto-repeat; map scancode via KeyboardImplementation::toKeyCode
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        // map via MouseImplementation::toMouseButton
        break;

    case SDL_EVENT_MOUSE_MOTION:
        // window-space → framebuffer-space (HiDPI) → Y-flip → game-space (via viewport)
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        // controller.scrolled({event.wheel.x, event.wheel.y})
        break;

    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        // event-driven gamepad handling (see Gamepad section)
        break;
    }
}
```

The `framebufferSizeCallback` free function and `InputContext` struct were deleted entirely — SDL3's event loop makes them unnecessary.

#### Mouse motion coordinate mapping

SDL3 delivers mouse motion in **window-space float coordinates** (top-left origin). The conversion chain to game-space is:

```cpp
// 1. HiDPI: scale up to framebuffer pixels
float fbX = event.motion.x * (framebufferWidth  / windowWidth);
float fbY = event.motion.y * (framebufferHeight / windowHeight);
// 2. Y-flip (OpenGL convention: y from bottom)
fbY = framebufferHeight - fbY;
// 3. Map through the letterboxed/pillarboxed game viewport
glm::vec2 gamePos = {
    (fbX - viewport.x) / viewport.width  * screenSize.width,
    (fbY - viewport.y) / viewport.height * screenSize.height
};
```

#### Gamepad API

GLFW polled all 16 joystick slots every frame (`glfwGetGamepadState`). SDL3 is fully event-driven:

| Event | Action |
|-------|--------|
| `SDL_EVENT_GAMEPAD_ADDED` | `SDL_OpenGamepad` → store in `openGamepads` map; fire `gamepadConnected` |
| `SDL_EVENT_GAMEPAD_REMOVED` | `SDL_CloseGamepad` → remove from map; fire `gamepadDisconnected` |
| `SDL_EVENT_GAMEPAD_BUTTON_DOWN/UP` | Map button, queue `Press`/`Release` event |
| `SDL_EVENT_GAMEPAD_AXIS_MOTION` | Normalize value, apply deadzone, fire axis callback |

Trigger axis normalization differs from GLFW:

```cpp
// GLFW triggers arrive in [-1, 1]; remapped to [0, 1]:
//   value = (rawValue + 1.0f) * 0.5f;   ← GLFW, REMOVED

// SDL3 triggers arrive in [0, 32767]; normalize directly:
float value = (float)event.gaxis.value / 32767.0f;
if (value < DEADZONE) value = 0.0f;
```

`LeftY` and `RightY` are negated so that positive values point up (matching canvas convention), same as before.

#### Timing

```cpp
// GLFW:
float time = (float)glfwGetTime();

// SDL3:
float time = static_cast<float>(SDL_GetTicks()) / 1000.0f;
```

#### ImGui frame start

```cpp
// Before:
ImGui_ImplGlfw_NewFrame();

// After:
ImGui_ImplSDL3_NewFrame();
```

#### Buffer swap

```cpp
// Before:
glfwSwapBuffers(mWindow->glfwWindow);

// After:
SDL_GL_SwapWindow(mWindow->sdlWindow);
```

#### `close()`

```cpp
// Before:
glfwSetWindowShouldClose(mWindow->glfwWindow, GLFW_TRUE);

// After:
mWindow->shouldClose = true;
```

#### `getPrimaryMonitorSize()`

```cpp
SDL_Init(SDL_INIT_VIDEO);
SDL_DisplayID primary = SDL_GetPrimaryDisplay();
const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(primary);
return { (unsigned int)mode->w, (unsigned int)mode->h };
```

---

### `source/canvas_impl.h`

- Updated destructor comment (GLFW → SDL).
- Added `DirectTexture takeScreenshot() const` declaration.

---

### `include/canvas.h` and `source/canvas.cpp`

Added the `takeScreenshot()` API (brought in from main alongside the SDL3 migration):

```cpp
// canvas.h
DirectTexture takeScreenshot() const;

// canvas.cpp
DirectTexture Canvas::takeScreenshot() const { return mCanvasImpl->takeScreenshot(); }
```

---

### `examples/CMakeLists.txt`

Added `hello_screenshot` executable target (mirroring the existing pattern).

---

## Gotchas

### `SDL_ENABLE_OLD_NAMES` required for the imgui SDL3 backend

SDL3 deliberately breaks code that still uses the old `SDL_bool` type (`SDL_TRUE` / `SDL_FALSE`) by mapping them to undefined symbols `SDL_TRUE_renamed_true` / `SDL_FALSE_renamed_false`. The imgui SDL3 backend (`imgui_impl_sdl3.cpp`) still uses these names. Without the define you get a link error similar to:

```
error: use of undeclared identifier 'SDL_TRUE_renamed_true'
```

Fix: add to the imgui CMake target:

```cmake
target_compile_definitions(imgui PRIVATE SDL_ENABLE_OLD_NAMES)
```

This instructs SDL3's `SDL_oldnames.h` to define backward-compatible aliases (`SDL_TRUE = true`, `SDL_FALSE = false`) rather than the intentional error symbols.

### Scissor test must be disabled before RTT (render-to-texture) passes

After clearing the main framebuffer to the game area, `GL_SCISSOR_TEST` is left enabled with a viewport-relative rect (e.g., `{100, 0, 600, 600}` when the window is pillarboxed). Render-to-texture FBOs use their own `(0, 0)`-origin coordinate space; an offset scissor rect clips everything inside the FBO, silently discarding every draw. This only manifests after a non-square window resize (when the viewport offset is non-zero).

Fix applied in `canvas_impl.cpp` before the RTT pass loop:

```cpp
// Scissor test was enabled for the game-area clear above.
// Each RTT FBO uses its own coordinate space starting at (0,0), so a
// letterboxed/pillarboxed scissor rect (offset from the origin) would
// clip all draws and clears inside the render target.  Disable it here;
// it is restored with the correct game-viewport rect after the loop.
glDisable(GL_SCISSOR_TEST);
```

### Single-float HiDPI scale

GLFW exposes `glfwGetWindowContentScale(window, &xscale, &yscale)` (two components). SDL3 exposes `SDL_GetWindowDisplayScale(window)` (one float). On all current platforms the two components are equal, so the simplification is safe.

---

## Verification

Build and run from the repository root:

```bash
cmake --preset ninja-debug-examples
cd ../build/ninja-debug-examples
ninja install
cd ../../install/ninja-debug-examples/bin
```

| Example | What to verify |
|---------|---------------|
| `hello_nothofagus` | Window opens, ImGui renders, sprite rotates |
| `test_keyboard` | All key presses register correctly |
| `hello_animation_state_machine` | WASD transitions animate the sprite |
| `hello_nested_render_targets` | Resize window — RTT textures continue updating |
| `hello_screenshot` | Screenshot saved successfully each frame |
