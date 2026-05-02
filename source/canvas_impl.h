#pragma once

#include "canvas.h"
#include "texture_container.h"
#include "bellota_container.h"
#include "render_target_container.h"
#include "texture_usage_monitor.h"
#include "imgui_rtt_manager.h"
#include "imgui_font_source_id.h"
#include "aa_box.h"
#include "backends/render_backend_select.h"
#include <vector>
#include <utility>
#include <span>
#include <cstddef>

struct ImFont;

namespace Nothofagus
{

/**
 * @class Canvas::CanvasImpl
 * @brief Implementation of the Canvas class, responsible for managing the actual window, textures, Bellotas, and rendering.
 *
 * This class encapsulates the low-level details of the Canvas, such as handling textures, Bellotas, and the OpenGL context.
 * It provides methods to manage graphical objects, set window properties, and handle the main rendering loop.
 */
class Canvas::CanvasImpl
{
public:

    CanvasImpl(
        const ScreenSize& screenSize,
        const std::string& title,
        const glm::vec3 clearColor,
        const unsigned int pixelSize,
        const float imguiFontSize,
        bool headless = false
    );

    /// Destructor to clean up resources and terminate the window backend
    ~CanvasImpl();

    // the number of monitor where the top left corner of the canvas is currently at
    std::size_t getCurrentMonitor() const;

    bool isFullscreen() const;

    void setFullScreenOnMonitor(std::size_t monitor = 0);

    AABox getWindowAABox() const;

    void setWindowed();

    /**
     * @brief Gets the screen size of the canvas.
     * @return The screen size as a ScreenSize object.
     */
    const ScreenSize& screenSize() const;

    void setScreenSize(const ScreenSize& screenSize);

    void setClearColor(glm::vec3 clearColor);

    void setWindowTitle(const std::string& title);

    ScreenSize windowSize() const;

    ViewportRect gameViewport() const;

    /**
     * @brief Adds a Bellota object to the canvas.
     * @param bellota The Bellota object to add.
     * @return The ID of the added Bellota.
     */
    BellotaId addBellota(const Bellota& bellota);

    /**
     * @brief Removes a Bellota object from the canvas.
     * @param bellotaId The ID of the Bellota to remove.
     */
    void removeBellota(const BellotaId bellotaId);

    /**
     * @brief Adds a Texture object to the canvas.
     * @param texture The Texture object to add.
     * @return The ID of the added Texture.
     */
    TextureId addTexture(const Texture& texture);

    /**
     * @brief Removes a Texture object from the canvas.
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

    void renderImguiTo(RenderTargetId renderTargetId, ImguiFontId fontId, ImguiDrawCallback imguiDrawCallback);

    ImguiFontSourceId addImguiFontSource(std::span<const std::byte> ttfBytes, GlyphRange glyphRange);

    void removeImguiFontSource(ImguiFontSourceId sourceId);

    ImguiFontSourceId defaultImguiFontSourceId() const;

    ImguiFontId bakeImguiFont(ImguiFontSourceId sourceId, float sizePx);

    void removeImguiFont(ImguiFontId id);

    bool isImguiFontReady(ImguiFontId id) const;

    ImFont* getImguiFontPtr(ImguiFontId id) const;

    void pushImguiFont(ImguiFontId id);

    void popImguiFont();

    ImguiFontId defaultImguiFontId() const;

    void setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor);

    /**
     * @brief Sets a tint color for a Bellota.
     * @param bellotaId The ID of the Bellota.
     * @param tint The tint color to apply.
     */
    void setTint(const BellotaId bellotaId, const Tint& tint);

    /**
     * @brief Removes the tint color from a Bellota.
     * @param bellotaId The ID of the Bellota.
     */
    void removeTint(const BellotaId bellotaId);

    /**
     * @brief Retrieves a Bellota by its ID.
     * @param bellotaId The ID of the Bellota.
     * @return A reference to the Bellota object.
     */
    Bellota& bellota(BellotaId bellotaId);

    /**
     * @brief Retrieves a const Bellota by its ID.
     * @param bellotaId The ID of the Bellota.
     * @return A const reference to the Bellota object.
     */
    const Bellota& bellota(BellotaId bellotaId) const;

    /**
     * @brief Retrieves a Texture by its ID.
     * @param textureId The ID of the Texture.
     * @return A reference to the Texture object.
     */
    Texture& texture(TextureId textureId);

    /**
     * @brief Retrieves a const Texture by its ID.
     * @param textureId The ID of the Texture.
     * @return A const reference to the Texture object.
     */
    const Texture& texture(TextureId textureId) const;

    /**
     * @brief Retrieves a TextureArray by its ID.
     * @param textureId The ID of the TextureArray.
     * @return A reference to the TextureArray object.
     */
    Texture& textureArray(TextureId textureId);

    /**
     * @brief Retrieves a const TextureArray by its ID.
     * @param textureId The ID of the TextureArray.
     * @return A const reference to the TextureArray object.
     */
    const Texture& textureArray(TextureId textureId) const;

    /**
     * @brief Accesses or modifies the stats flag.
     * @return A reference to the stats flag (true to display stats).
     */
    bool& stats();

    /**
     * @brief Retrieves the current stats flag.
     * @return A const reference to the stats flag.
     */
    const bool& stats() const;

    /**
     * @brief Runs the main loop of the canvas with a custom update function.
     * @param update The custom update function to be called every frame.
     * @param controller The Controller object that handles user input.
     */
    void run(std::function<void(float deltaTime)> update, Controller& controller);

    /// Execute a single frame with a caller-supplied delta time (in milliseconds).
    void tick(float deltaTimeMS, std::function<void(float)> update, Controller& controller);
    void tick(float deltaTimeMS, std::function<void(float)> update);
    void tick(float deltaTimeMS);

    void setAutoRemoveUnusedTextures(bool enabled);

    /// Close the canvas and release resources.
    void close();

    /// Captures the last rendered frame visible to the user as a DirectTexture (RGBA).
    DirectTexture takeScreenshot() const;

private:
    void replaceBellota(const BellotaId bellotaId, const Bellota& bellota);
    void clearUnusedTextures();
    void ensureSessionStarted(Controller& controller);
    void runOneFrame(float deltaTimeMS, std::function<void(float)> update, Controller& controller);

    ScreenSize mScreenSize; ///< The screen size of the canvas.
    std::string mTitle; ///< The title of the canvas window.
    glm::vec3 mClearColor; ///< The background color of the canvas.
    unsigned int mPixelSize; ///< The pixel size on the canvas.

    TextureContainer mTextures; ///< Container for Texture objects.
    BellotaContainer mBellotas; ///< Container for Bellota objects.
    RenderTargetContainer mRenderTargets; ///< Container for RenderTarget objects.
    TextureUsageMonitor mTextureUsageMonitor;

    /// RTT passes queued by renderTo() during the update callback, executed before the main render.
    std::vector<std::pair<RenderTargetId, std::vector<BellotaId>>> mPendingRttPasses;

    ActiveBackend mBackend; ///< GPU rendering backend (compile-time selected).

    /// Owns per-RTT secondary ImGuiContexts + the per-frame RTT pass queue,
    /// AND the canvas-wide ImGui font manager (main HiDPI font + RTT default
    /// + user-baked sizes; deferred bake/remove queue + atlas rebuild).
    /// Declared after mBackend / mRenderTargets so initialization order is
    /// well-defined.
    ImguiRttManager mImguiRtt;

    bool mStats; ///< Flag to indicate whether stats should be displayed.
    bool mHeadless{false}; ///< When true, the window is hidden (no visible UI).
    bool mSessionStarted{false}; ///< True after ensureSessionStarted() has been called.
    bool mAutoTextureGC{true}; ///< When true, unreferenced textures are removed each frame.
    std::vector<const BellotaPack*> mSortedBellotaPacks; ///< Reusable depth-sorted draw list.

    struct Window; ///< Forward declaration for window management.
    std::unique_ptr<Window> mWindow; ///< Pointer to the window object.

    AABox mLastWindowedAABox;
    ViewportRect mGameViewport; ///< Current letterboxed game viewport (set each frame in run()).
};

}
