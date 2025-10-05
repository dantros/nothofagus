#pragma once

#include "canvas.h"
#include "texture_container.h"
#include "bellota_container.h"
#include "texture_usage_monitor.h"

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

    /**
     * @brief Constructs a CanvasImpl object.
     * @param screenSize The desired screen size for the canvas (in pixels).
     * @param title The title of the window.
     * @param clearColor The background color for the canvas.
     * @param pixelSize The size of each pixel on the screen.
     */
    CanvasImpl(
        const ScreenSize& screenSize,
        const std::string& title,
        const glm::vec3 clearColor,
        const unsigned int pixelSize
    );

    /// Destructor to clean up resources and terminate GLFW
    ~CanvasImpl();

    /**
     * @brief Gets the screen size of the canvas.
     * @return The screen size as a ScreenSize object.
     */
    const ScreenSize& screenSize() const;

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

    void clearUnusedTextures();

    void setTexture(const BellotaId bellotaId, const TextureId textureId);

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

    /// Close the canvas and release resources.
    void close();

private:
    void replaceBellota(const BellotaId bellotaId, const Bellota& bellota);

    ScreenSize mScreenSize; ///< The screen size of the canvas.
    std::string mTitle; ///< The title of the canvas window.
    glm::vec3 mClearColor; ///< The background color of the canvas.
    unsigned int mPixelSize; ///< The pixel size on the canvas.

    TextureContainer mTextures; ///< Container for Texture objects.
    BellotaContainer mBellotas; ///< Container for Bellota objects.
    TextureUsageMonitor mTextureUsageMonitor;

    unsigned int mShaderProgram; ///< The OpenGL shader program for rendering.

    bool mStats; ///< Flag to indicate whether stats should be displayed.

    struct Window; ///< Forward declaration for window management.
    std::unique_ptr<Window> mWindow; ///< Pointer to the window object.
};

}