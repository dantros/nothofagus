#pragma once

#include <glm/glm.hpp>
#include "bellota.h"
#include "texture.h"
#include "render_target.h"
#include "controller.h"
#include "tint.h"
#include "screen_size.h"
#include "imgui_draw_callback.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>

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
     * @brief Queue an ImGui draw callback to be rendered into the given render target.
     *
     * The callback runs on a secondary ImGuiContext owned by this render target —
     * window positions, widget values and open/closed state are isolated from the main
     * UI and from other RTTs. Coordinate space matches the render target size in pixels.
     *
     * Call from inside the update() callback, same phase as renderTo() for sprites.
     * The callback itself is invoked later (during the pre-main RTT pass phase) on the
     * secondary context — do NOT call ImGui functions on the main context from inside it.
     *
     * Limitation (v1): input events (mouse, keyboard) are not forwarded to the secondary
     * context. Widgets inside the RTT are displayed but not interactive.
     */
    void renderImguiTo(RenderTargetId renderTargetId, ImguiDrawCallback imguiDrawCallback);

    /**
     * @brief Bake an ImGui font at the requested pixel size (or return the
     *        cached one if already baked).
     *
     * Bakes a font in **logical pixels** (no OS-DPI scaling) and caches it,
     * or returns the cached ImFont if one was already baked at this size.
     * Intended for diegetic UI inside RTTs where one RTT pixel maps to one
     * game-canvas pixel. Pass the returned pointer to ImGui::PushFont(...)
     * / ImGui::PopFont() inside a renderImguiTo() callback to render crisp
     * glyphs at exactly that size.
     *
     * Behaviour by call-site state:
     *   - **Cache hit**: returns the cached ImFont* (always non-null). O(1).
     *   - **Cache miss outside an ImGui frame** (e.g. before the first
     *     run()/tick()): bakes synchronously, returns the new pointer.
     *   - **Cache miss inside an ImGui frame** (called from a run() / tick()
     *     update callback or a renderImguiTo() callback — the atlas is
     *     locked there): enqueues a deferred bake and returns nullptr.
     *     The bake completes at the start of the next frame; caller must
     *     re-poll bakeImguiFont(sameSize) on a later frame to retrieve the
     *     pointer.
     *
     * Repeat calls with the same `sizePx` return a pointer to the same
     * cached ImFont — the atlas is only baked once per size. Callers wanting
     * to mutate per-font state like `ImFont::Scale` should be aware they are
     * sharing it with every other caller of the same size.
     *
     * Reference invalidation: any ImFont* returned by this function is
     * invalidated by a subsequent removeImguiFont() (the next-frame atlas
     * rebuild gives every surviving size a fresh pointer). Callers that
     * hold pointers across frames should refresh them via a fresh
     * bakeImguiFont() before each use.
     *
     * @return Pointer to the cached or newly baked font on a synchronous
     *         path; nullptr on a deferred-bake path. Lifetime owned by the
     *         shared ImGui atlas; do not delete or take ownership.
     */
    ImFont* bakeImguiFont(float sizePx);

    /**
     * @brief Remove a previously baked ImGui font, freeing its atlas glyphs.
     *
     * Schedules a full atlas rebuild at the start of the next frame:
     * ImFontAtlas::Clear() + re-add the main HiDPI font + re-bake every
     * surviving size + GPU font texture re-upload. Always deferred — safe
     * to call from inside a run() / renderImguiTo() callback.
     *
     * Asserts (during the drain) that sizePx was previously baked. Invalidates
     * every ImFont* previously returned by bakeImguiFont() — callers must
     * re-fetch fresh pointers via bakeImguiFont(sameSize) for any size they
     * still need.
     */
    void removeImguiFont(float sizePx);

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