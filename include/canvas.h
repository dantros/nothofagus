#pragma once

#include <glm/glm.hpp>
#include "bellota.h"
#include "mesh.h"
#include "texture.h"
#include "render_target.h"
#include "dense_land.h"
#include "sparse_land.h"
#include "explorer.h"
#include "controller.h"
#include "tint.h"
#include "screen_size.h"
#include "present_mode.h"
#include "imgui_overlay.h"
#include "imgui_draw_callback.h"
#include "imgui_font_id.h"
#include "imgui_font_source_id.h"
#include "imgui_image_id.h"
#include "imgui_image_size.h"
#include "markdown_renderer.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <span>
#include <optional>
#include <cstddef>
#include <cstdint>

struct ImFont;

namespace Nothofagus
{

using DenseLandExplorer   = Explorer<DenseLand>;
using SparseLandExplorer = Explorer<SparseLand>;

// Default screen size for the canvas.
constexpr static ScreenSize DEFAULT_SCREEN_SIZE{256, 240};

/// Default window title for the canvas.
const std::string DEFAULT_TITLE{"Nothofagus App"};

/// Default background color for the canvas (black).
const glm::vec3 DEFAULT_CLEAR_COLOR{0.0f, 0.0f, 0.0f};

/// Default pixel size for rendering.
constexpr static unsigned int DEFAULT_PIXEL_SIZE{ 4 }; 

constexpr static float DEFAULT_IMGUI_FONT_SIZE{14};

/// Default presentation mode. Mailbox is vsync'd + triple-buffered (no tearing,
/// compositor-friendly) and avoids the ~45 fps FIFO/compositor pacing on Linux.
constexpr static PresentMode DEFAULT_PRESENT_MODE{PresentMode::Mailbox};

/// @brief Returns the resolution of the primary monitor using GLFW.
/// Safe to call before constructing a Canvas — initialises GLFW internally (idempotent).
ScreenSize getPrimaryMonitorSize();

/// @brief Returns the OS content (DPI) scale of the primary monitor.
/// Safe to call before constructing a Canvas — initialises GLFW/SDL internally
/// (idempotent). Returns 1.0 if the monitor cannot be queried. Note: this is the
/// primary monitor's scale read before any window exists, so it can differ from a
/// Canvas's live contentScale() if the window opens on a different-DPI monitor or
/// a contentScale override is set.
float getPrimaryMonitorContentScale();

/**
 * @class Canvas
 * @brief A class representing the main drawing surface where objects like Bellotas, and textures are rendered.
 * 
 * The Canvas manages a variety of objects including textures, Bellotas, and handles user input.
 */
class Canvas
{
public:

    /**
     * @brief Construct a new Canvas object.
     * @param screenSize The screen size in pixels (default is 256x240).
     * @param title The window title (default is "Nothofagus App").
     * @param clearColor The background color of the canvas (default is black).
     * @param pixelSize The pixel size (default is 4).
     * @param imguiFontSize font size used for DearImGui (default is 14.f).
     * @param headless When true, the window is hidden (offscreen rendering).
     * @param presentMode Swapchain / vsync preference (default Mailbox). Set at
     *        construction only; affects windowed builds (Vulkan present mode and
     *        OpenGL swap interval). Ignored in pure-offscreen headless-Vulkan builds.
     * @param targetFps Optional frame-rate cap for run() (std::nullopt = unlimited,
     *        the default). See setTargetFps for semantics.
     */
    Canvas(
        const ScreenSize& screenSize = DEFAULT_SCREEN_SIZE,
        const std::string& title = DEFAULT_TITLE,
        const glm::vec3 clearColor = DEFAULT_CLEAR_COLOR,
        const unsigned int pixelSize = DEFAULT_PIXEL_SIZE,
        const float imguiFontSize = DEFAULT_IMGUI_FONT_SIZE,
        bool headless = false,
        PresentMode presentMode = DEFAULT_PRESENT_MODE,
        std::optional<float> targetFps = std::nullopt
    );

    /// Destructor
    ~Canvas();

    // the monitor index where the top left corner of the canvas is currently located
    std::size_t getCurrentMonitor() const;

    bool isFullscreen() const;

    void setFullScreenOnMonitor(std::size_t monitor = 0);

    void setWindowed();

    /**
     * @brief Returns the logical screen size (by value).
     *
     * Returned by value (not by reference) because the size is stored atomically:
     * on the threaded path the sim thread may `setScreenSize()` from commit's
     * update while the render thread reads it. Safe to call from any thread.
     * @return The screen size.
     */
    ScreenSize screenSize() const;

    void setScreenSize(const ScreenSize& screenSize);

    void setClearColor(glm::vec3 clearColor);

    /// @brief Caps the run() loop to at most `targetFps` frames per second.
    ///
    /// Pass std::nullopt (or a value <= 0) to remove the cap (unlimited — the default).
    /// Runtime-settable: takes effect within a frame. Only affects run(); tick() is
    /// caller-driven and never throttled.
    ///
    /// Most useful with a non-blocking present mode (Vulkan Mailbox/Immediate, OpenGL
    /// Immediate). Under hard vsync (Fifo, or OpenGL swap interval 1) the loop is already
    /// blocked at the display refresh, so a cap above refresh does nothing and a cap below
    /// it competes with vsync.
    void setTargetFps(std::optional<float> targetFps);

    /// The current frame-rate cap, or std::nullopt when unlimited.
    std::optional<float> targetFps() const;

    void setWindowTitle(const std::string& title);

    ScreenSize windowSize() const;

    /// Returns the current game viewport in framebuffer pixels (letterboxed or pillarboxed).
    ViewportRect gameViewport() const;

    /// The game viewport expressed in ImGui display coordinates (top-left
    /// origin, "points"), ready for ImGui::SetNextWindowPos/Size. Converts
    /// gameViewport() (framebuffer pixels) through the live DisplaySize /
    /// DisplayFramebufferScale, so an overlay placed at this rect tracks the
    /// pillarboxed/letterboxed canvas on any backend or contentScale. Call
    /// inside a run()/tick() update or renderImguiTo() callback (ImGui must be
    /// in a frame).
    ImguiOverlayRect imguiOverlayViewport() const;

    /// The logical base ImGui font size (points) the canvas was built with —
    /// the authoritative, contentScale-independent unit for sizing overlay
    /// bars (e.g. barHeight = imguiBaseFontSize() * ratio), decoupled from any
    /// font pushed during the frame.
    float imguiBaseFontSize() const;

    /// imguiBaseFontSize() pre-multiplied by the effective content scale
    /// (contentScale()). The main (standard-UI) context auto-scales its *text* by
    /// the OS DPI, but manually-computed dimensions (e.g. an overlay bar height) do
    /// not — size them from this so screen-space overlays grow with the UI on HiDPI.
    float imguiScaledFontSize() const;

    /// Effective OS content (DPI) scale applied to the main standard-UI ImGui
    /// context: the override if one was set, else the window backend's reported
    /// scale (always 1.0 in headless). Standard-UI fonts and widget metrics scale
    /// by this; diegetic RTT contexts are unaffected.
    float contentScale() const;

    /// Override the OS content scale used by the main context. Drives an
    /// accessibility/zoom knob and is the deterministic seam used by the visual
    /// tests. Pass std::nullopt to revert to the backend-reported value.
    void setContentScaleOverride(std::optional<float> scale);

    /**
     * @brief Add a Bellota to the canvas.
     * @param bellota The Bellota object to add.
     * @return The ID of the added Bellota.
     */
    BellotaId addBellota(const Bellota& bellota);

    /**
     * @brief Remove a Bellota from the canvas.
     * @param bellotaId The ID of the Bellota to remove.
     */
    void removeBellota(const BellotaId bellotaId);

    /**
     * @brief Add a Texture to the canvas.
     * @param texture The Texture object to add.
     * @return The ID of the added Texture.
     */
    TextureId addTexture(const Texture& texture);

    /**
     * @brief Remove a Texture from the canvas.
     * @param textureId The ID of the Texture to remove.
     */
    void removeTexture(const TextureId textureId);

    void setTexture(const BellotaId bellotaId, const TextureId textureId);

    void markTextureAsDirty(const TextureId textureId);
    void setTextureMinFilter(const TextureId textureId, TextureSampleMode mode);
    void setTextureMagFilter(const TextureId textureId, TextureSampleMode mode);

    /**
     * @brief Register a triangle mesh asset and return a stable handle.
     *
     * The mesh is copied internally and uploaded to the GPU on the next frame.
     * Attach to one or more Bellotas via the `Bellota(Transform, TextureId, MeshId)`
     * constructor — bellotas sharing a `MeshId` share the same GPU buffers.
     *
     * Custom meshes are only valid on plain (non-tile-map) textures. The
     * tile-map render path requires the auto-quad's exact UV invariant
     * (UVs in `[0, 1]` over the full tile-map extent); attaching a custom
     * mesh to a bellota whose texture is in tile-map mode is rejected via
     * `debugCheck` in `addBellota` / `setMesh` / `setTexture`.
     */
    MeshId addMesh(const Mesh& mesh);

    /// Move-overload of `addMesh` for callers that can hand off ownership of the mesh data.
    MeshId addMesh(Mesh&& mesh);

    /**
     * @brief Remove a previously registered user mesh.
     *
     * The mesh must not be referenced by any Bellota (asserts via `debugCheck`).
     * Engine-allocated auto-quads cannot be removed through this entry point —
     * they are managed exclusively by the canvas and freed automatically when
     * their last referencing bellota goes away.
     */
    void removeMesh(MeshId meshId);

    /**
     * @brief Swap the mesh referenced by a Bellota. The new mesh must be a
     * previously registered `MeshId`. The previous mesh is left in place if
     * other bellotas still reference it; otherwise it becomes eligible for GC
     * on the next frame.
     *
     * Forbidden when the bellota's current texture is in tile-map mode —
     * see `addMesh` for the rationale.
     */
    void setMesh(const BellotaId bellotaId, const MeshId meshId);

    /**
     * @brief Read-only access to a registered mesh (user or auto-quad).
     */
    const Mesh& mesh(MeshId meshId) const;

    /**
     * @brief Read-only access to the mesh a Bellota currently draws (auto-quad
     * for textured bellotas with no explicit mesh, or the registered user mesh
     * otherwise).
     */
    const Mesh& mesh(BellotaId bellotaId) const;

    RenderTargetId addRenderTarget(ScreenSize size);

    void removeRenderTarget(RenderTargetId renderTargetId);

    TextureId renderTargetTexture(RenderTargetId renderTargetId) const;

    /// Register a DenseLand (world data) with the canvas. Returns a stable id.
    /// No GPU resources are allocated until a DenseLandExplorer is registered against this DenseLand.
    DenseLandId addDenseLand(DenseLand denseLand);

    /// Remove a DenseLand. debugChecks that no DenseLandExplorer still references it.
    void removeDenseLand(DenseLandId denseLandId);

    /// Access a registered DenseLand (mutable; use `setCell` to edit world data).
    DenseLand& denseLand(DenseLandId denseLandId);
    const DenseLand& denseLand(DenseLandId denseLandId) const;

    /// Register a DenseLandExplorer (renderer) against a previously-added DenseLand.
    /// Allocates the chunk pool (small IndirectTexture + Bellota slots tagged explorer-managed).
    DenseLandExplorerId addDenseLandExplorer(DenseLandExplorer explorer);

    /// Remove a DenseLandExplorer and tear down its pool slots.
    void removeDenseLandExplorer(DenseLandExplorerId explorerId);

    /// Access a registered DenseLandExplorer (mutable; use `setCamera` to scroll).
    DenseLandExplorer& denseLandExplorer(DenseLandExplorerId explorerId);
    const DenseLandExplorer& denseLandExplorer(DenseLandExplorerId explorerId) const;

    /// Register a SparseLand (sparse world data) with the canvas. Returns a stable id.
    /// No GPU resources are allocated until a SparseLandExplorer is registered against this SparseLand.
    SparseLandId addSparseLand(SparseLand sparseLand);

    /// Remove a SparseLand. debugChecks that no SparseLandExplorer still references it.
    void removeSparseLand(SparseLandId sparseLandId);

    /// Access a registered SparseLand (mutable; use `addChunk` / `setCell` to edit world data).
    SparseLand& sparseLand(SparseLandId sparseLandId);
    const SparseLand& sparseLand(SparseLandId sparseLandId) const;

    /// Register a SparseLandExplorer (renderer) against a previously-added SparseLand.
    /// Allocates the chunk pool (small IndirectTexture + Bellota slots tagged explorer-managed).
    SparseLandExplorerId addSparseLandExplorer(SparseLandExplorer explorer);

    /// Remove a SparseLandExplorer and tear down its pool slots.
    void removeSparseLandExplorer(SparseLandExplorerId explorerId);

    /// Access a registered SparseLandExplorer (mutable; use `setCamera` to scroll).
    SparseLandExplorer& sparseLandExplorer(SparseLandExplorerId explorerId);
    const SparseLandExplorer& sparseLandExplorer(SparseLandExplorerId explorerId) const;

    void renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds);

    /**
     * @brief Queue an ImGui draw callback to be rendered into the given render target,
     *        with an auto push/pop of `fontId` wrapping the callback body.
     *
     * The callback runs on a secondary ImGuiContext owned by this render target —
     * window positions, widget values and open/closed state are isolated from the main
     * UI and from other RTTs. Coordinate space matches the render target size in pixels.
     *
     * Call from inside the update() callback, same phase as renderTo() for sprites.
     * The callback itself is invoked later (during the pre-main RTT pass phase) on the
     * secondary context — do NOT call ImGui functions on the main context from inside it.
     *
     * `fontId` is auto-pushed via `pushImguiFont` before the callback runs and popped
     * after it returns, so the body never has to touch `ImFont`. Mid-callback font
     * overrides are still allowed via `canvas.pushImguiFont(otherId)` (paired with
     * `popImguiFont()`). Pass `defaultImguiFontId()` to render with the canvas's
     * default secondary-context font.
     *
     * Graceful fallback: if `fontId`'s bake is still pending or its entry has been
     * removed, the callback runs without an explicit push (text falls back to the
     * secondary-context default font set at canvas construction).
     *
     * Limitation (v1): input events (mouse, keyboard) are not forwarded to the secondary
     * context. Widgets inside the RTT are displayed but not interactive.
     */
    void renderImguiTo(RenderTargetId renderTargetId, ImguiFontId fontId, ImguiDrawCallback imguiDrawCallback);

    /**
     * @brief Register a Visual at a fixed size as a persistent ImGui-bindable image.
     *
     * The single entry point for drawing an engine Visual inside ImGui: register once, then
     * draw the returned id every frame. The entry point is a Visual (texture + optional mesh
     * + current layer + opacity), NOT a Bellota — placement (transform / depth) is meaningless
     * in an ImGui cell. Pass `canvas.bellota(id).visual()` for a bellota's current appearance,
     * or a standalone `Visual{textureId}`. Works for every texture kind (Direct / Indirect /
     * tile-map / animation frame); a custom mesh is honored; magnification follows the
     * texture's `magFilter`.
     *
     * Registration decouples allocation from display: the internal render target is allocated
     * now and its `ImTextureID` handle is created on the next render tick, then stays valid for
     * the registration's lifetime. So any draw at least one rendered frame after registration
     * is **warm-up-free**, and the handle never churns on size changes or garbage collection.
     * The id is a first-class ImGui image handle: pass it to `imguiImage(id)`, or fetch
     * `imguiImageHandle(id)` / `imguiImageSize(id)` and drive `ImGui::Image` yourself.
     *
     * @p sizeSpec fixes the rasterization resolution along two orthogonal axes: the size
     * source — `ImguiImageSize::Natural{}` (default; the visual's real size, its mesh's
     * bounding box), `ImguiImageSize::Scaled{factor}`, or `ImguiImageSize::Explicit{size, fit}`
     * — and the units (an `ImguiImageUnits` field on each, default `Logical`). `Logical` units
     * scale with OS DPI and the target is rasterized at size × contentScale so mesh geometry
     * stays crisp; `Device` units are exact physical pixels (1 texel → 1 display pixel,
     * bypassing OS DPI). For responsive layouts, register at a generous size and vary only the
     * *draw* size at `imguiImage` time (a GPU downscale of the fixed-resolution handle).
     *
     * Call before/inside the loop. Free with `unregisterImguiImage`.
     */
    ImguiImageId registerImguiImage(const Visual& visual,
                                    const ImguiImageSize::Spec& sizeSpec = ImguiImageSize::Natural{});

    /**
     * @brief Refresh a registered image's source appearance without changing its id.
     *
     * Re-renders with the new `visual` (e.g. an advanced animation layer, a new opacity,
     * a swapped texture/mesh) next tick, keeping the same `sizeSpec`. The handle stays
     * valid unless the resolved physical size or the source texture/mesh changes, in which
     * case the RTT is recreated and the handle re-warms for one tick.
     */
    void updateImguiImage(ImguiImageId imageId, const Visual& visual);

    /// Free a registered image's render target, ImGui handle, and resource pins.
    void unregisterImguiImage(ImguiImageId imageId);

    /**
     * @brief Draw a registered image in the current ImGui window (`ImGui::Image`).
     *
     * `drawSize` (logical px) overrides the registered display size for this draw only —
     * a cheap GPU downscale of the fixed-resolution handle (used e.g. for markdown
     * fit-to-width). Honors the texture's magFilter and the registered opacity. Reserves
     * layout (empty cell) while the handle is not yet ready or the visual is invisible.
     */
    void imguiImage(ImguiImageId imageId, std::optional<glm::vec2> drawSize = std::nullopt);

    /// The registered image's ImGui-bindable `ImTextureID` (as a 64-bit value), or 0 if the
    /// id is unknown or its handle is not yet ready. Usable directly in `ImGui::Image`.
    std::uint64_t imguiImageHandle(ImguiImageId imageId) const;

    /// The registered image's resolved logical display size, or {0, 0} if the id is unknown.
    glm::vec2 imguiImageSize(ImguiImageId imageId) const;

    /// Whether the registered image's handle has been created (false during the one-tick
    /// window right after registration / a size-changing update).
    bool isImguiImageReady(ImguiImageId imageId) const;

    /**
     * @brief Register a TTF buffer as a new font source.
     *
     * Bytes are copied internally; the caller's span only needs to live
     * until this call returns. Safe to call before run() OR from inside an
     * update / renderImguiTo callback. Result is immediately usable as the
     * source argument to `bakeImguiFont(source, sizePx)`; the resulting
     * ImguiFontId becomes resolvable on the NEXT frame if the bake call
     * lands while ImGui has the atlas locked (same deferred-bake semantics
     * as `bakeImguiFont`).
     *
     * @param ttfBytes    Raw TTF file bytes. Copied internally.
     * @param glyphRange  Glyph-range preset for this source (default = Latin).
     * @return Stable handle valid until `removeImguiFontSource(thisId)`.
     */
    ImguiFontSourceId addImguiFontSource(std::span<const std::byte> ttfBytes,
                                         GlyphRange glyphRange = GlyphRange::Default);

    /**
     * @brief Drop a previously registered font source.
     *
     * Schedules a deferred atlas rebuild; every `ImguiFontId` baked from
     * this source is also invalidated (cascade-removed). Safe inside a
     * frame callback. Asserts (during the drain) that the id is registered.
     * Removing the canvas's built-in default source is forbidden and will
     * fire a `debugCheck`.
     */
    void removeImguiFontSource(ImguiFontSourceId sourceId);

    /**
     * @brief Id of the canvas's built-in default font source (the embedded
     *        Noto Sans Regular face registered at construction).
     *
     * Stable for the canvas lifetime. Pass this to `bakeImguiFont(...)` when
     * no custom TTF is required. Sibling accessor to `defaultImguiFontId()`,
     * which returns the secondary-context default *font* (a baked size from
     * this source).
     */
    ImguiFontSourceId defaultImguiFontSourceId() const;

    /**
     * @brief Ids of the other embedded built-in faces — Noto Sans Bold,
     *        Italic, BoldItalic, and Noto Sans Mono.
     *
     * Stable for the canvas lifetime and, like the default source, protected
     * from `removeImguiFontSource()`. These back true bold / italic /
     * bold-italic / monospace rendering; pass any of them to
     * `bakeImguiFont(...)`. `defaultMarkdownStyle()` wires them up for you.
     */
    ImguiFontSourceId boldImguiFontSourceId() const;
    ImguiFontSourceId italicImguiFontSourceId() const;
    ImguiFontSourceId boldItalicImguiFontSourceId() const;
    ImguiFontSourceId monoImguiFontSourceId() const;

    /**
     * @brief Source id for an embedded CJK script (Simplified/Traditional
     *        Chinese, Japanese, Korean), or std::nullopt when that script was
     *        not compiled in (its NOTHOFAGUS_EMBED_CJK_* option was OFF).
     *
     * The signature is stable across build configs, so consumer code never
     * needs the build defines — just check the optional at runtime. CJK glyphs
     * are NOT part of the default UI font: bake the returned source yourself
     * via `bakeImguiFont(*src, sizePx)` and push it where CJK text is needed,
     * so the atlas only grows when you actually use it.
     */
    std::optional<ImguiFontSourceId> embeddedCjkFontSource(CjkScript script) const;

    /**
     * @brief Build a ready-to-use MarkdownStyle from the embedded Noto Sans
     *        family.
     *
     * Bakes the regular / bold / italic / bold-italic / mono faces at the body
     * size, plus the six heading levels from the Bold face at descending sizes
     * (h1 = 1.8×, h2 = 1.5×, h3 = 1.25×, h4 = 1.1×, h5 = h6 = 1.0× the body
     * size). Hand the result straight to `MarkdownRenderer::setStyle(...)`.
     *
     * `bodySizePx` is a logical size: it is internally multiplied by
     * `contentScale²` to match the main-canvas HiDPI UI-font recipe, so the
     * result is sized correctly for **main-canvas** markdown (this is the
     * common case). For markdown drawn into a render target (1:1 logical
     * pixels) build a `MarkdownStyle` by hand from the source-id accessors at
     * unscaled sizes instead.
     *
     * **Call before run()/tick()** so the bakes are synchronous and ready on
     * the first frame. If called from inside a frame callback the underlying
     * bakes are deferred one frame; `MarkdownRenderer` already falls back to
     * the current font for not-yet-ready ids, so rendering self-heals on the
     * next frame.
     */
    MarkdownStyle defaultMarkdownStyle(float bodySizePx = 16.0f);

    /**
     * @brief Bake an ImGui font from a previously-added source at the
     *        requested pixel size and return a stable handle to it.
     *
     * Repeat calls with the same `(sourceId, sizePx)` return the same id
     * (dedup). Bakes in **logical pixels** (no OS-DPI scaling) — intended
     * for diegetic UI inside RTTs where one RTT pixel maps to one
     * game-canvas pixel. Resolve the id to an `ImFont*` via
     * `getImguiFontPtr(id)` and pass that to `ImGui::PushFont(...)` /
     * `ImGui::PopFont()`, or use the higher-level `pushImguiFont(id)` /
     * `popImguiFont()` so user code never has to mention `ImFont`.
     *
     * To bake from the embedded default font, pass
     * `defaultImguiFontSourceId()` as `sourceId`.
     *
     * The returned `ImguiFontId` is **stable across atlas rebuilds**: when
     * any rebuild (e.g. from a deferred bake or `removeImguiFontSource`)
     * fires, the cache walks every surviving entry and patches its
     * underlying `ImFont*` in place — the id stays valid; only the
     * resolved pointer changes.
     *
     * Call-site behaviour:
     *   - **Outside an ImGui frame** (before first run()/tick()): the bake
     *     happens synchronously. `getImguiFontPtr(id)` returns the pointer
     *     immediately.
     *   - **Inside an ImGui frame** (run/tick update or renderImguiTo
     *     callback): the entry is created with a null pointer; a rebuild
     *     fires at the start of the next frame. `getImguiFontPtr(id)`
     *     returns nullptr until the rebuild completes (one frame later).
     *
     * The id is invalidated by `removeImguiFont(thisId)` or by
     * `removeImguiFontSource(sourceId)` (cascade). Callers wanting to mutate
     * per-font state like `ImFont::Scale` should be aware they are sharing
     * it with every other caller of the same `(sourceId, sizePx)` pair.
     */
    ImguiFontId bakeImguiFont(ImguiFontSourceId sourceId, float sizePx);

    /**
     * @brief Remove a previously baked ImGui font, freeing its atlas glyphs.
     *
     * Schedules a full atlas rebuild at the start of the next frame:
     * `ImFontAtlas::Clear()` + re-add the main HiDPI font + re-bake every
     * surviving entry + GPU font texture re-upload. Always deferred — safe
     * to call from inside a `run()` / `renderImguiTo()` callback.
     *
     * Asserts (during the drain) that `id` is currently registered.
     * Invalidates only `id` itself — every other `ImguiFontId` survives the
     * rebuild with its underlying `ImFont*` patched in place, so other
     * callers' handles keep working without intervention.
     */
    void removeImguiFont(ImguiFontId id);

    /**
     * @brief True if `id` is currently registered and its bake has completed.
     *
     * Use this to guard `getImguiFontPtr(id)` during the deferred-bake window
     * (one frame between `bakeImguiFont(...)` returning and the next-frame
     * drain finishing the bake). Returns false when:
     *   - `id` is not (or no longer) registered (e.g., after
     *     `removeImguiFont(id)`).
     *   - `id` is registered but a deferred bake is still pending.
     */
    bool isImguiFontReady(ImguiFontId id) const;

    /**
     * @brief Resolve an `ImguiFontId` to its current `ImFont*`.
     *
     * The pointer is meant for handoff to ImGui APIs that take `ImFont*`
     * directly (e.g. `ImGui::PushFont`, `ImGui::CalcTextSizeA`). For the
     * common push-then-pop case, prefer the higher-level
     * `pushImguiFont(id)` / `popImguiFont()` so user code never has to
     * mention `ImFont` at all.
     *
     * Returns nullptr when:
     *   - `id` is not (or no longer) registered (e.g. after `removeImguiFont`).
     *   - `id` is registered but a deferred bake is still pending; the
     *     pointer becomes valid one frame after the `bakeImguiFont` call
     *     that introduced it.
     *
     * Callers can hold the resolved pointer only within one frame —
     * an atlas rebuild between frames may patch the underlying `ImFont*`
     * in place (the `id` is stable; the pointer it resolves to is not).
     */
    ImFont* getImguiFontPtr(ImguiFontId id) const;

    /**
     * @brief Push a previously baked font onto ImGui's font stack.
     *
     * Equivalent to `ImGui::PushFont(&getImguiFontPtr(id))`, with the same
     * preconditions: asserts `id` is registered AND its bake has completed.
     * Guard with `isImguiFontReady(id)` during the deferred-bake window.
     *
     * Pair with `popImguiFont()`. Safe to nest with itself or with raw
     * `ImGui::PushFont(...)` — both forms target ImGui's single global
     * font stack on the current context.
     */
    void pushImguiFont(ImguiFontId id);

    /**
     * @brief Pop the most-recently-pushed font from ImGui's font stack.
     *
     * Forwards to `ImGui::PopFont()`. Use as the partner to
     * `pushImguiFont(...)` (or to a raw `ImGui::PushFont(...)` — both pop
     * through the same stack).
     */
    void popImguiFont();

    /**
     * @brief Id of the secondary-context default font, baked at canvas
     *        construction at the unscaled `imguiFontSize`.
     *
     * Pass this to `renderImguiTo(rtId, id, cb)` when the panel doesn't
     * have a specific font of its own and should just use the same default
     * the manager already sets as `io.FontDefault` on every secondary RTT
     * context. The id is stable for the canvas lifetime.
     */
    ImguiFontId defaultImguiFontId() const;

    void setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor);

    /**
     * @brief Set a tint color for a Bellota.
     * @param bellotaId The ID of the Bellota to tint.
     * @param tint The tint color to apply.
     */
    void setTint(const BellotaId bellotaId, const Tint& tint);

    /**
     * @brief Remove the tint color of a Bellota.
     * @param bellotaId The ID of the Bellota to remove the tint.
     */
    void removeTint(const BellotaId bellotaId);

    /**
     * @brief Get reference of a Bellota by its ID.
     * @param bellotaId The ID of the Bellota.
     * @return A reference to the Bellota.
     */
    Bellota& bellota(BellotaId bellotaId);

    /**
     * @brief Get a const Bellota by its ID.
     * @param bellotaId The ID of the Bellota.
     * @return A const reference to the Bellota.
     */
    const Bellota& bellota(BellotaId bellotaId) const;

    /**
     * @brief Get a Texture by its ID.
     * @param textureId The ID of the Texture.
     * @return A reference to the Texture.
     */
    Texture& texture(TextureId textureId);

    /**
     * @brief Get a const Texture by its ID.
     * @param textureId The ID of the Texture.
     * @return A const reference to the Texture.
     */
    const Texture& texture(TextureId textureId) const;

    /**
     * @brief Access or modify the stats of the canvas.
     * @return A reference to the stats flag (true = show stats).
     */
    bool& stats();

    /**
     * @brief Get the stats of the canvas.
     * @return A const reference to the stats flag.
     */
    const bool& stats() const;

    /**
     * @brief Start the canvas main loop with the default update function.
     */
    void run();

    /**
     * @brief Start the canvas main loop with a custom update function.
     * @param update The custom update function to call each frame.
     */
    void run(std::function<void(float deltaTime)> update);

    /**
     * @brief Start the canvas main loop with a custom update function and controller.
     * @param update The custom update function to call each frame.
     * @param controller The controller object to handle inputs.
     */
    void run(std::function<void(float deltaTime)> update, Controller& controller);

    /// Execute a single frame with a caller-supplied delta time (in milliseconds).
    void tick(float deltaTime, std::function<void(float)> update, Controller& controller);
    void tick(float deltaTime, std::function<void(float)> update);
    void tick(float deltaTime);

    // ----- Threaded driver (two-thread sim/render split) -----
    //
    // Opt-in alternative to run()/tick(). The application owns both loops;
    // nothofagus spawns no threads. Run `commit()` on a simulation thread and
    // `renderFrame()` on the main thread:
    //
    //   canvas.beginThreadedSession(controller);                 // main thread
    //   std::thread sim([&]{
    //       while (canvas.isThreadedRunning())
    //           canvas.commit(dt, update);                       // sim thread
    //   });
    //   while (canvas.isThreadedRunning())
    //       canvas.renderFrame(controller);                      // main thread
    //   sim.join();
    //
    // The sim thread mutates the scene and commits a snapshot; the main thread
    // draws the previous snapshot, so render of frame N overlaps sim of N+1.
    //
    // From inside `commit()`'s update (sim thread) you may: mutate existing bellota
    // values (transform, tint, opacity, layer) and add/remove bellotas at runtime
    // via the regular `addBellota`/`removeBellota` (they serialize against the
    // renderer and defer GPU frees while a threaded session is live — see those
    // methods). Interactive ImGui runs via the `commit(dt, update, uiCallback)`
    // overload (widgets on the sim thread, cloned to the render thread); gamepad
    // input via `commit(dt, update, simController)`. Still single-threaded-only:
    // explorers (Dense/Sparse land) and other structural resource ops
    // (textures/meshes/render targets) — create those up front. `run()`/`tick()`
    // remain the unrestricted single-threaded path.

    /// Main thread: start a threaded session (binds input, marks it running).
    void beginThreadedSession(Controller& controller);

    /// Thread-safe: true until the window is closed. Drives both loop conditions.
    bool isThreadedRunning() const;

    /// Sim thread: run `update(deltaTime)` (game logic) and publish a frame
    /// snapshot. No ImGui.
    void commit(float deltaTime, std::function<void(float)> update);

    /// Sim thread: run `update(deltaTime)` (game logic, lock-free) then
    /// `uiCallback(deltaTime)` as an interactive ImGui frame whose draw data is
    /// cloned into the snapshot and rendered by the main thread. ImGui widgets in
    /// `uiCallback` run on the sim thread and respond to the mouse (input is
    /// marshalled render→sim). Keep ImGui calls in `uiCallback`, not `update`.
    void commit(float deltaTime, std::function<void(float)> update, std::function<void(float)> uiCallback);

    /// Sim thread: run `update(deltaTime)` (game logic) and publish a frame
    /// snapshot, feeding `simController` from the gamepad first so the game can
    /// poll it / receive its callbacks inside `update`. `simController` is the
    /// game's controller, distinct from the render controller passed to
    /// `beginThreadedSession`/`renderFrame` (which owns window input + `close`).
    /// Gamepad state is marshalled render→sim (one frame stale by construction).
    /// No ImGui.
    void commit(float deltaTime, std::function<void(float)> update, Controller& simController);

    /// Main thread: render the latest published snapshot and pump window/input.
    void renderFrame(Controller& controller);

    /// Whether the threaded ImGui UI captured the mouse / keyboard on the most
    /// recent commit. Read these in the game `update` (which runs before the UI
    /// frame) to skip world interaction while the UI is using that input.
    /// Thread-safe; the value is one frame old by construction.
    ///
    /// imguiWantsMouse() is true while the cursor is over a UI window or a widget
    /// is being dragged. imguiWantsKeyboard() is true only while the UI is actively
    /// capturing keystrokes — a widget being edited (e.g. an InputText) or an open
    /// modal — NOT merely because a panel is visible (keyboard nav stays enabled
    /// but does not, on its own, claim the keyboard).
    bool imguiWantsMouse() const;
    bool imguiWantsKeyboard() const;

    /// Enable or disable automatic removal of unreferenced textures each frame.
    /// Enabled by default. Disable during bulk asset loading to prevent premature removal.
    void setAutoRemoveUnusedTextures(bool enabled);

    /// Enable or disable automatic removal of unreferenced meshes each frame.
    /// Enabled by default. Disable during bulk asset loading to prevent premature removal.
    void setAutoRemoveUnusedMeshes(bool enabled);

    /// Close the canvas and clean up resources.
    void close();

    /// Captures the last rendered frame visible to the user as a DirectTexture (RGBA).
    /// Must be called from within the update() callback.
    DirectTexture takeScreenshot() const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> mImplPtr;
};

}