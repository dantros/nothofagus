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
    // - Title: "Animation State"
    // - Background color: light gray (RGB: 0.7, 0.7, 0.7)
    // - Pixel Size: 6
    Nothofagus::Canvas canvas({150, 100}, "Animation State", {0.7, 0.7, 0.7}, 6);

    // Define a color palette with 5 colors (RGBA format)
    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 1.0},  // Black
        {1.0, 0.0, 0.0, 1.0},  // Red
        {0.0, 1.0, 0.0, 1.0},  // Green
        {0.0, 0.0, 1.0, 1.0},  // Blue
        {1.0, 1.0, 0.0, 1.0}   // Yellow
    };

    // Create a Texture with dimensions 4x4 and 5 layers
    Nothofagus::Texture textureArray({4, 4}, glm::vec4(0,0,0,1), 5);

    // Populate each layer of the Texture with pixel indices corresponding to the palette
    textureArray
        .setPallete(pallete)
        .setPixels({
            0,1,2,3,
            4,0,1,2,
            3,4,0,1,
            2,3,4,0
        }, 0)
        .setPixels({
            4,0,1,2,
            3,4,0,1,
            2,3,4,0,
            1,2,3,4
        }, 1)
        .setPixels({
            3,4,0,1,
            2,3,4,0,
            1,2,3,4,
            0,1,2,3
        }, 2)
        .setPixels({
            2,3,4,0,
            1,2,3,4,
            0,1,2,3,
            4,0,1,2
        }, 3)
        .setPixels({
            1,2,3,4,
            0,1,2,3,
            4,0,1,2,
            3,4,0,1
        }, 4);

    // Add the Texture to the canvas
    Nothofagus::TextureId textureId = canvas.addTextureArray(textureArray);

    // Create an Bellota using the Texture
    // Position: (75, 50)
    // Layer count: 5
    Nothofagus::BellotaId animatedBellotaId = canvas.addAnimatedBellota({{{75.0f, 50.0f}}, textureId, 5});

    // Access the Bellota from the canvas
    Nothofagus::Bellota& animatedbellota = canvas.animatedBellota(animatedBellotaId);

    // Define an animation state with 5 layers and equal time intervals
    std::vector<int> anim1Layers = {0, 1, 2, 3, 4};       // Layers to cycle through
    std::vector<float> anim1LayersTimes = {500.0f, 500.0f, 500.0f, 500.0f, 500.0f};  // 500 ms per layer
    std::string anim1name = "example";                   // Name of the animation state
    Nothofagus::AnimationState animation1(anim1Layers, anim1LayersTimes, anim1name);

    // Create an AnimationStateMachine associated with the Bellota
    Nothofagus::AnimationStateMachine textureArrayAnimationTree(canvas.animatedBellota(animatedBellotaId));

    // Add the animation state to the state machine
    textureArrayAnimationTree.addState(anim1name, &animation1);

    // Set the initial state to "example"
    textureArrayAnimationTree.setState(anim1name);

    // Initialize a timer for the update loop
    float time = 0.0f;

    // Define the update function, which runs during each frame
    auto update = [&](float dt)
    {
        time += dt;

        // Scale the Bellota to 10x10 units
        animatedbellota.transform().scale() = glm::vec2(10.0f, 10.0f);

        // Update the animation state machine with the elapsed time
        textureArrayAnimationTree.update(dt);
    };

    // Run the canvas with the update loop
    canvas.run(update);

    return 0;
}
