#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <nothofagus.h>

int main()
{

    // Create a canvas for rendering
    // - Size: 150x100
    // - Title: "Set Layer"
    // - Background color: light gray (RGB: 0.7, 0.7, 0.7)
    // - Pixel Size: 6
    Nothofagus::Canvas canvas({150, 100}, "Set Layer", {0.7, 0.7, 0.7}, 6);

    // Define 5 different color palettes, each with a single color in RGBA format
    Nothofagus::ColorPallete pallete1{{0.0, 0.0, 0.0, 1.0}};  // Black
    Nothofagus::ColorPallete pallete2{{1.0, 0.0, 0.0, 1.0}};  // Red
    Nothofagus::ColorPallete pallete3{{0.0, 1.0, 0.0, 1.0}};  // Green
    Nothofagus::ColorPallete pallete4{{0.0, 0.0, 1.0, 1.0}};  // Blue
    Nothofagus::ColorPallete pallete5{{1.0, 1.0, 0.0, 1.0}};  // Yellow

    // Create a TextureArray with dimensions 4x4 and 5 layers
    Nothofagus::TextureArray textureArray({4, 4}, 5);

    // Assign each color palette to a specific layer in the texture array
    // Each layer is filled with the corresponding palette color
    textureArray.setLayerPallete(pallete1, 0)
        .setPixelsInLayer({
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0
        }, 0);

    textureArray.setLayerPallete(pallete2, 1)
        .setPixelsInLayer({
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0
        }, 1);

    textureArray.setLayerPallete(pallete3, 2)
        .setPixelsInLayer({
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0
        }, 2);

    textureArray.setLayerPallete(pallete4, 3)
        .setPixelsInLayer({
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0
        }, 3);

    textureArray.setLayerPallete(pallete5, 4)
        .setPixelsInLayer({
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0
        }, 4);

    // Add the TextureArray to the canvas
    Nothofagus::TextureId textureId = canvas.addTextureArray(textureArray);

    // Create an Bellota using the TextureArray
    // - Position: (75, 50)
    // - Layer count: 5
    Nothofagus::BellotaId animatedBellotaId = canvas.addAnimatedBellota({{{75.0f, 50.0f}}, textureId, 5});

    // Access the Bellota object to modify its properties
    Nothofagus::Bellota& animatedbellota = canvas.animatedBellota(animatedBellotaId);

    // Scale the Bellota to 10x10 units
    animatedbellota.transform().scale() = glm::vec2(10.0f, 10.0f);

    // Timer for updating logic
    float time = 0.0f;

    // Define the update function for the canvas
    auto update = [&](float dt)
    {
        time += dt;
        // No specific update logic here, but time accumulates for potential future use
    };

    // Initialize layer index variable
    int l = 0;

    // Create a controller for handling user inputs
    Nothofagus::Controller controller;

    // Register a keybinding for "W" to increment the current layer
    controller.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        l = (l + 1) % 5;  // Cycle to the next layer (0-4)
        animatedbellota.setActualLayer(l);  // Update the visible layer
    });

    // Register a keybinding for "S" to decrement the current layer
    controller.registerAction({Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        l = (l + 4) % 5;  // Cycle to the previous layer (0-4)
        animatedbellota.setActualLayer(l);  // Update the visible layer
    });

    // Run the canvas with the update function and controller logic
    canvas.run(update, controller);

    return 0;
}
