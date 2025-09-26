#pragma once

#include <glm/glm.hpp>
#include "bellota.h"
#include "texture.h"
#include "controller.h"
#include "tint.h"
#include <memory>
#include <functional>
#include <string>

namespace Nothofagus
{

/// @struct ScreenSize
/// @brief Structure representing the size of the screen in pixels.
struct ScreenSize
{
    unsigned int width, height;
};

// Default screen size for the canvas.
constexpr static ScreenSize DEFAULT_SCREEN_SIZE{256, 240};

/// Default window title for the canvas.
const std::string DEFAULT_TITLE{"Nothofagus App"};

/// Default background color for the canvas (black).
const glm::vec3 DEFAULT_CLEAR_COLOR{0.0f, 0.0f, 0.0f};

/// Default pixel size for rendering.
constexpr static unsigned int DEFAULT_PIXEL_SIZE{ 4 }; 



/**
 * @class Canvas
 * @brief A class representing the main drawing surface where objects like Bellotas, AnimatedBellotas, and textures are rendered.
 * 
 * The Canvas manages a variety of objects including textures, Bellotas, AnimatedBellotas, and handles user input.
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
     */
    Canvas(
        const ScreenSize& screenSize = DEFAULT_SCREEN_SIZE,
        const std::string& title = DEFAULT_TITLE,
        const glm::vec3 clearColor = DEFAULT_CLEAR_COLOR,
        const unsigned int pixelSize = DEFAULT_PIXEL_SIZE
    );

    /// Destructor
    ~Canvas();

    /**
     * @brief Returns reference to the screen size.
     * @return The screen size.
     */
    const ScreenSize& screenSize() const;

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
     * @brief Add an AnimatedBellota to the canvas.
     * @param animatedBellota The AnimatedBellota object to add.
     * @return The ID of the added AnimatedBellota.
     */
    AnimatedBellotaId addAnimatedBellota(const AnimatedBellota& animatedBellota);

    /**
     * @brief Remove an AnimatedBellota from the canvas.
     * @param animatedBellotaId The ID of the AnimatedBellota to remove.
     */
    void removeAnimatedBellota(const AnimatedBellotaId animatedBellotaId);

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
    
    /**
     * @brief Add a TextureArray to the canvas.
     * @param textureArray The TextureArray object to add.
     * @return The ID of the added TextureArray.
     */
    TextureId addTextureArray(const TextureArray& textureArray);

    /**
     * @brief Remove a TextureArray from the canvas.
     * @param textureId The ID of the TextureArray to remove.
     */
    void removeTextureArray(const TextureId textureId);

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
     * @brief Get an AnimatedBellota by its ID.
     * @param animatedBellotaId The ID of the AnimatedBellota.
     * @return A reference to the AnimatedBellota.
     */
    AnimatedBellota& animatedBellota(AnimatedBellotaId animatedBellotaId);

    /**
     * @brief Get a const AnimatedBellota by its ID.
     * @param animatedBellotaId The ID of the AnimatedBellota.
     * @return A const reference to the AnimatedBellota.
     */
    const AnimatedBellota& animatedBellota(AnimatedBellotaId animatedBellotaId) const;

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
     * @brief Get a TextureArray by its ID.
     * @param textureId The ID of the TextureArray.
     * @return A reference to the TextureArray.
     */
    TextureArray& textureArray(TextureId textureId);

    /**
     * @brief Get a const TextureArray by its ID.
     * @param textureId The ID of the TextureArray.
     * @return A const reference to the TextureArray.
     */
    const TextureArray& textureArray(TextureId textureId) const;

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

    /// Close the canvas and clean up resources.
    void close();

private:
    class CanvasImpl;
    std::unique_ptr<CanvasImpl> mCanvasImpl;
};

}