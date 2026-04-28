Q: when was opengl4.6 released?

OpenGL 4.6 was released on July 31, 2017 by the Khronos Group at SIGGRAPH 2017. It's the final OpenGL version — Khronos has shifted focus to Vulkan since, so 4.6 is effectively the end of the line.

The headline feature for your situation is GL_ARB_gl_spirv (and GL_ARB_spirv_extensions), both made core in 4.6 — that's what enables glShaderBinary + glSpecializeShader to ingest SPIR-V directly. Driver support is universal on modern desktop GPUs (NVIDIA, AMD, Intel) but macOS caps at 4.1, so a 4.6 bump would drop Mac support if that matters to you.

---

Q: what are the main differences between 4.1 and 4.6 ?

Big jump — 4.1 is from 2010, 4.6 is from 2017, so it's 7 years and roughly a dozen core promotions. The headline differences:

Compute & GPU programming

Compute shaders (4.3) — generic GPU work outside the graphics pipeline. Probably the biggest single addition.
Shader storage buffer objects / SSBOs (4.3) — large read-write buffers from shaders, replaces a lot of UBO/texture hacks.
Image load/store (4.2) — random read-write access to textures from any shader stage.
Atomic counters (4.2) and shader atomic operations on SSBOs (4.3).
Shader pipeline

SPIR-V ingestion (4.6) — glShaderBinary + glSpecializeShader. The thing you were just asking about.
Separate shader objects were already in 4.1, but 4.4–4.5 added glBindBuffersBase/glBindTextures for multi-bind.
Shader subroutines were in 4.0; 4.3 added shader resource introspection (glGetProgramInterface).
Tessellation & geometry

4.1 already had tessellation shaders. 4.3 added vertex attribute binding (decouples format from buffer — closer to Vulkan's vertex input model).
Texturing

Texture views (4.3) — alias one storage with different formats/levels.
ASTC and BPTC compressed formats (4.2/4.3).
Bindless-ish ergonomics via multi-bind (4.4) and immutable storage glTexStorage* (4.2). True bindless is still NV-only extensions.
Texture buffer ranges, clear texture (4.3/4.4).
Framebuffers & rendering

Framebuffers without attachments (4.3) — useful for compute-driven rasterisation.
glClearBufferData (4.3), glCopyImageSubData (4.3) — bulk operations without binds.
Multi-draw indirect (4.3) and glMultiDrawElementsIndirectCount (4.6) — GPU-driven rendering.
Debug & robustness

KHR_debug / glDebugMessageCallback (4.3) — proper debug callbacks instead of polling error queues. Quality-of-life improvement that's hard to give up once you've used it.
Robustness (KHR_robustness, 4.5) and no-error contexts (4.6) — strip safety checks in release builds for a small perf win.
Driver-side perf

Direct State Access / DSA (4.5) — glNamedBufferData, glTextureStorage2D, etc. No more bind-to-modify dance. This is the single biggest API ergonomics change between 4.1 and 4.6.
Buffer storage with persistent mapping (4.4) — keep buffers mapped across draws, write to them like normal memory.
Anisotropic filtering finally promoted to core (4.6) — was an extension forever.
For your project specifically, the realistic upgrade carrots if you ever bumped from 3.3 → 4.6 would be:

SPIR-V (the original question)
DSA — would simplify a lot of glBind* boilerplate in opengl_backend.cpp
KHR_debug — much nicer error reporting than the current glGetError/info-log dance
Persistent-mapped buffers — useful if you ever hit per-frame upload bottlenecks
Cost: drops macOS (capped at 4.1) and any GL ES / WebGL ambitions.

---

Q: what are the main challenges of moving from opengl 3.3 to webgl?

Good question, and the framing matters: WebGL 1.0 ≈ OpenGL ES 2.0 and WebGL 2.0 ≈ OpenGL ES 3.0. So "GL 3.3 → WebGL" is really "desktop GL → GL ES via the browser." That changes what's hard.

The hard structural stuff
1. GLSL version and dialect

GL 3.3 uses #version 330 core. WebGL 2 requires #version 300 es and a precision qualifier on every float/int. WebGL 1 uses #version 100 with no in/out, only attribute/varying. Your four sprite shaders would all need rewrites — not deep rewrites, but every file.
ES is stricter than desktop: implicit conversions banned, loops must have constant bounds in WebGL 1, no gl_FragColor in 300 es, etc.
2. No raw pointers / no C API

Everything goes through JS bindings (WebGL2RenderingContext). glBufferData takes a typed array (Uint8Array, Float32Array), not a raw pointer. With Emscripten this is mostly handled, but you pay a copy on every upload unless you carefully use views into the WASM heap.
No client-side vertex arrays — must use VBOs. (You're already doing this, so fine.)
3. Context loss is a real lifecycle event

The browser can yank your GL context any time (tab backgrounded, GPU reset, driver hiccup). All textures, buffers, programs become invalid. You need a webglcontextlost / webglcontextrestored handler that recreates everything from CPU-side data. Desktop GL has no equivalent. This usually means refactoring resource ownership so every GPU object can be rebuilt from a CPU-side description.
4. No threading

WebGL calls must happen on the main thread (or one OffscreenCanvas worker). No multi-context resource sharing the way desktop GL allows. Audio/input/render all share the same event loop.
The features you'd lose
Going from 3.3 to WebGL 2 specifically:

No glTextureView — but you weren't using it.
No geometry shaders, no tessellation — you weren't using these either.
No glDrawElementsBaseVertex until extensions.
gl_DrawID, multi-draw indirect, SSBOs, compute shaders — none. (Compute is a separate spec, WebGPU, not WebGL.)
Limited texture formats — fewer integer/depth combos, R8UI is fine but some formats need EXT_color_buffer_float-style extensions.
sampler1D doesn't exist in ES — your palette texture (sampler1D paletteSampler in the indirect/tilemap shaders) would become a Nx1 sampler2D. Minor but every palette lookup needs adjusting.
Specific to your codebase
Skimming what I saw earlier in opengl_backend.cpp:

usampler2DArray with R8UI — WebGL 2 supports this (it's core in GL ES 3.0). Good.
texelFetch on sampler1D — port the palette to a 2D Nx1 texture. Small change in 3 fragment shaders.
#version 330 core literals — replace with #version 300 es + add precision highp float; precision highp int; precision highp usampler2DArray; etc.
Vertex attribute names and layout(location=N) — WebGL 2 supports explicit locations, so this carries over cleanly.
glDeleteShader / glDeleteProgram — same API, but context-loss handlers must reissue the entire chain.
The R"(...)" string-literal approach you're externalizing to files doesn't help on the web — Emscripten can package files into the virtual FS, but you still need to fetch them async at startup. Most WebGL projects keep shaders inline or load them via a single bundled JSON. Worth keeping in mind: the file-based plan we just drafted maps fine to Emscripten's --preload-file, but the loading sequencing changes.
Toolchain reality
Emscripten is the path: compile your C++20 + GLAD-using code to WASM, replace GLAD with Emscripten's GL ES headers, link with -s USE_WEBGL2=1 -s FULL_ES3=1.
GLFW works — Emscripten ships an emulation layer. SDL3 works too.
ImGui has a working WebGL backend (imgui_impl_opengl3.cpp already supports GL ES 3.0 via #define IMGUI_IMPL_OPENGL_ES3).
std::filesystem, std::ifstream — available, but they read from Emscripten's virtual FS, not the real disk. Files need to be pre-packaged.
std::thread — works via pthreads + SharedArrayBuffer, but adds COOP/COEP header requirements when serving the page. Most projects go single-threaded.
Honest summary
For this project specifically, the painful parts are:

Context loss handling — biggest architectural change. Your Canvas/CanvasImpl would need a "rebuild all GPU state from CPU descriptions" path.
Shader port to #version 300 es + the sampler1D → sampler2D palette change.
Async asset loading — canvas.run() can't start until shaders/fonts are fetched.
Build pipeline — adding an Emscripten preset alongside the existing 10+ presets.
Things that are not hard for you:

Your shaders are simple (no compute, no tessellation, no geometry).
You already use VBOs, immutable-ish texture storage patterns, texelFetch — all WebGL 2-friendly.
The Vulkan backend would obviously not come along, but the OpenGL backend abstraction is already clean (the RenderBackend concept), so adding a WebGL2Backend sibling is structurally a smaller change than the Vulkan port was.
If you ever want to seriously look at this, the realistic path is WebGL 2 only (skip WebGL 1) and Emscripten + emscripten-glfw, and budget the work mostly around context loss, async asset loading, and the shader dialect port.