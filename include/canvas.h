#pragma once

#include <glm/glm.hpp>
#include "bellota.h"
#include "texture.h"
#include "render_target.h"
#include "controller.h"
#include "tint.h"
#include "screen_size.h"
#include "imgui_draw_callback.h"
#include "imgui_font_id.h"
#include "imgui_font_source_id.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <span>
#include <cstddef>

struct ImFont;

namespace Nothofagus
{

// Default screen size for the canvas.
constexpr static ScreenSize DEFAULT_SCREEN_SIZE{256, 240};

/// Default window title for the canvas.
const std::string DEFAULT_TITLE{"Nothofagus App"};

/// Default background color for the canvas (black).
const glm::vec3 DEFAULT_CLEAR_COLOR{0.0f, 0.0f, 0.0f};

/// Default pixel size for rendering.
constexpr static unsigned int DEFAULT_PIXEL_SIZE{ 4 }; 

constexpr static float DEFAULT_IMGUI_FONT_SIZE{14};

/// @brief Returns the resolution of the primary monitor using GLFW.
/// Safe to call before constructing a Canvas — initialises GLFW internally (idempotent).
ScreenSize getPrimaryMonitorSize();

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
     */
    Canvas(
        const ScreenSize& screenSize = DEFAULT_SCREEN_SIZE,
        const std::string& title = DEFAULT_TITLE,
        const glm::vec3 clearColor = DEFAULT_CLEAR_COLOR,
        const unsigned int pixelSize = DEFAULT_PIXEL_SIZE,
        const float imguiFontSize = DEFAULT_IMGUI_FONT_SIZE,
        bool headless = false
    );

    /// Destructor
    ~Canvas();

    // the monitor index where the top left corner of the canvas is currently located
    std::size_t getCurrentMonitor() const;

    bool isFullscreen() const;

    void setFullScreenOnMonitor(std::size_t monitor = 0);

    void setWindowed();

    /**
     * @brief Returns reference to the screen size.
     * @return The screen size.
     */
    const ScreenSize& screenSize() const;

    void setScreenSize(const ScreenSize& screenSize);

    void setClearColor(glm::vec3 clearColor);

    void setWindowTitle(const std::string& title);

    ScreenSize windowSize() const;

    /// Returns the current game viewport in framebuffer pixels (letterboxed or pillarboxed).
    ViewportRect gameViewport() const;

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

    RenderTargetId addRenderTarget(ScreenSize size);

    void removeRenderTarget(RenderTargetId renderTargetId);

    TextureId renderTargetTexture(RenderTargetId renderTargetId) const;

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
     *        Roboto blob registered at construction).
     *
     * Stable for the canvas lifetime. Pass this to `bakeImguiFont(...)` when
     * no custom TTF is required. Sibling accessor to `defaultImguiFontId()`,
     * which returns the secondary-context default *font* (a baked size from
     * this source).
     */
    ImguiFontSourceId defaultImguiFontSourceId() const;

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

    /// Enable or disable automatic removal of unreferenced textures each frame.
    /// Enabled by default. Disable during bulk asset loading to prevent premature removal.
    void setAutoRemoveUnusedTextures(bool enabled);

    /// Close the canvas and clean up resources.
    void close();

    /// Captures the last rendered frame visible to the user as a DirectTexture (RGBA).
    /// Must be called from within the update() callback.
    DirectTexture takeScreenshot() const;

private:
    class CanvasImpl;
    std::unique_ptr<CanvasImpl> mCanvasImpl;
};

}