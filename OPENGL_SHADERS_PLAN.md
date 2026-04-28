# Externalize OpenGL shaders (mirror the Vulkan layout)

## Context

The Vulkan backend authors GLSL in [source/shaders/](source/shaders/) (`sprite.vert`, `sprite.frag`, `sprite_indirect.frag`, `sprite_tilemap.frag`), compiles them to SPIR-V at build time via `glslc`/`glslangValidator` ([CMakeLists.txt:150-197](CMakeLists.txt#L150-L197)), and loads them at runtime through `VulkanBackend::createShaderModule(const char* spirvPath)` ([source/backends/vulkan_backend.cpp:114-134](source/backends/vulkan_backend.cpp#L114-L134)). The absolute build-output paths are baked in via `target_compile_definitions` (`NOTHOFAGUS_SPRITE_*_SPV`).

The OpenGL backend, by contrast, embeds all four shaders as inline `R"(...)"` raw string literals inside [source/backends/opengl_backend.cpp:87-199](source/backends/opengl_backend.cpp#L87-L199). Editing them requires touching C++, recompiling, and they cannot be diffed or shared with tooling.

The goal: move OpenGL shaders into stand-alone files alongside the existing Vulkan ones, mirroring the file-based pipeline so both backends look the same architecturally.

## Key constraint

OpenGL 3.3 core (current target — see `#version 330 core` in every embedded shader) cannot consume SPIR-V binaries. SPIR-V loading via `glShaderBinary(... GL_SHADER_BINARY_FORMAT_SPIR_V_ARB ...)` requires OpenGL 4.6 / `GL_ARB_gl_spirv`. The Vulkan and OpenGL shaders are also semantically different (push constants + descriptor sets vs. plain `uniform`), so they cannot share source files unmodified.

## Approach

Treat the OpenGL shaders as **plain GLSL text files loaded at runtime**, keeping `#version 330 core`. No SPIR-V step on the OpenGL path (staying on GL 3.3). This mirrors Vulkan's *external-file* aspect without forcing a GL version bump or a cross-compilation step.

### File layout

Add four new files under [source/shaders/](source/shaders/):

```
source/shaders/
  sprite.vert                     (existing, Vulkan)
  sprite.frag                     (existing, Vulkan)
  sprite_indirect.frag            (existing, Vulkan)
  sprite_tilemap.frag             (existing, Vulkan)
  sprite_gl.vert                  (NEW — OpenGL)
  sprite_gl.frag                  (NEW — OpenGL, direct RGBA)
  sprite_gl_indirect.frag         (NEW — OpenGL, palette-indexed)
  sprite_gl_tilemap.frag          (NEW — OpenGL, tilemap)
```

The `_gl` suffix keeps the two backends visually distinct in the directory listing and avoids any chance of feeding a Vulkan-flavored shader (push constants, descriptor sets) into the GL compiler. Contents are taken verbatim from the existing raw-string blocks in `opengl_backend.cpp`.

### CMake changes ([CMakeLists.txt](CMakeLists.txt))

Inside the `else()` branch at line 204 (the OpenGL branch), add path constants and `target_compile_definitions` mirroring the Vulkan block:

```cmake
set(SHADER_GL_VERT          "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite_gl.vert")
set(SHADER_GL_FRAG          "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite_gl.frag")
set(SHADER_GL_INDIRECT_FRAG "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite_gl_indirect.frag")
set(SHADER_GL_TILEMAP_FRAG  "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite_gl_tilemap.frag")

target_compile_definitions(nothofagus PRIVATE
    NOTHOFAGUS_SPRITE_GL_VERT="${SHADER_GL_VERT}"
    NOTHOFAGUS_SPRITE_GL_FRAG="${SHADER_GL_FRAG}"
    NOTHOFAGUS_SPRITE_GL_INDIRECT_FRAG="${SHADER_GL_INDIRECT_FRAG}"
    NOTHOFAGUS_SPRITE_GL_TILEMAP_FRAG="${SHADER_GL_TILEMAP_FRAG}")
```

No `add_custom_command` is needed (no precompilation step). Source-file paths point straight at the repo `source/shaders/` directory, identical in spirit to how Vulkan macros point at `${CMAKE_CURRENT_BINARY_DIR}/*.spv`.

### Code changes ([source/backends/opengl_backend.cpp](source/backends/opengl_backend.cpp))

1. Add a small helper next to `compileShader` (around line 21):

   ```cpp
   static std::string readShaderFile(const char* path)
   {
       std::ifstream file(path, std::ios::ate);
       if (!file.is_open())
           throw std::runtime_error(std::string("Failed to open shader file: ") + path);
       const std::size_t size = static_cast<std::size_t>(file.tellg());
       std::string source(size, '\0');
       file.seekg(0);
       file.read(source.data(), static_cast<std::streamsize>(size));
       return source;
   }
   ```

   This is the OpenGL twin of [vulkan_backend.cpp:114-134](source/backends/vulkan_backend.cpp#L114-L134) — same `std::ifstream` + `seekg(0, ate)` trick, just text-mode.

2. In `OpenGLBackend::initialize()` ([opengl_backend.cpp:83-228](source/backends/opengl_backend.cpp#L83-L228)), replace each raw-string block with a `readShaderFile(NOTHOFAGUS_SPRITE_GL_*)` call. Remove the four `R"(...)"` string literals; everything else (compile/link/uniform-cache flow) stays identical.

3. Add `#include <fstream>` at the top.

### Why not embed SPIR-V then cross-compile to GLSL?

Possible (`spirv-cross --version 330`) but adds a build dependency, a translation step, and a divergence between authored source and what gets compiled — for zero runtime benefit on GL 3.3. Skipped.

### Why not share a single `.glsl` per shader between both backends with `#ifdef`?

The divergence is structural (push constants vs. `uniform`, descriptor-set bindings vs. nothing, attribute naming). Any preprocessor split would dominate the file. Cleaner to keep two siblings.

## Files modified

- [CMakeLists.txt](CMakeLists.txt) — add 4 path vars + `target_compile_definitions` block in the OpenGL `else()` branch
- [source/backends/opengl_backend.cpp](source/backends/opengl_backend.cpp) — add `readShaderFile`, replace 4 raw-string literals with file reads, add `<fstream>` include
- [source/shaders/sprite_gl.vert](source/shaders/sprite_gl.vert) — NEW (extract lines 87-99)
- [source/shaders/sprite_gl.frag](source/shaders/sprite_gl.frag) — NEW (extract lines 100-117)
- [source/shaders/sprite_gl_indirect.frag](source/shaders/sprite_gl_indirect.frag) — NEW (extract lines 131-152)
- [source/shaders/sprite_gl_tilemap.frag](source/shaders/sprite_gl_tilemap.frag) — NEW (extract lines 173-199)

## Verification

1. Configure + build with an OpenGL preset that enables examples:
   ```bash
   cmake --preset windows-debug-glfw-opengl-examples
   cmake --build build/windows-debug-glfw-opengl-examples
   cmake --install build/windows-debug-glfw-opengl-examples
   ```
2. Run the demos that exercise each shader path:
   - `hello_nothofagus` / `hello_animation` — direct + indirect path
   - `hello_direct_texture` — direct RGBA path
   - one of the tilemap examples (recent commits indicate a tilemap demo on this branch) — tilemap path
3. Edit one of the new `.glsl` files (e.g. tweak a tint blend), rebuild, confirm change appears without rerunning CMake configure.
4. Sanity-check that nothing changed for the Vulkan path: `cmake --preset windows-debug-glfw-vulkan-examples && cmake --build ...` still runs.
