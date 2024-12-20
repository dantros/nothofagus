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

    // Create a TextureArray with dimensions 4x4 and 5 layers
    Nothofagus::TextureArray textureArray({4, 4}, 5);

    // Populate each layer of the TextureArray with pixel indices corresponding to the palette
    textureArray.setLayerPallete(pallete, 0)
        .setPixelsInLayer({
            0,1,2,3,
            4,0,1,2,
            3,4,0,1,
            2,3,4,0
        }, 0);
    textureArray.setLayerPallete(pallete, 1)
        .setPixelsInLayer({
            4,0,1,2,
            3,4,0,1,
            2,3,4,0,
            1,2,3,4
        }, 1);
    textureArray.setLayerPallete(pallete, 2)
        .setPixelsInLayer({
            3,4,0,1,
            2,3,4,0,
            1,2,3,4,
            0,1,2,3
        }, 2);
    textureArray.setLayerPallete(pallete, 3)
        .setPixelsInLayer({
            2,3,4,0,
            1,2,3,4,
            0,1,2,3,
            4,0,1,2
        }, 3);
    textureArray.setLayerPallete(pallete, 4)
        .setPixelsInLayer({
            1,2,3,4,
            0,1,2,3,
            4,0,1,2,
            3,4,0,1
        }, 4);

    // Add the TextureArray to the canvas
    Nothofagus::TextureArrayId textureArrayId = canvas.addTextureArray(textureArray);

    // Create an AnimatedBellota using the TextureArray
    // Position: (75, 50)
    // Layer count: 5
    Nothofagus::AnimatedBellotaId animatedBellotaId = canvas.addAnimatedBellota({{{75.0f, 50.0f}}, textureArrayId, 5});

    // Access the AnimatedBellota from the canvas
    Nothofagus::AnimatedBellota& animatedbellota = canvas.animatedBellota(animatedBellotaId);

    // Define an animation state with 5 layers and equal time intervals
    std::vector<int> anim1Layers = {0, 1, 2, 3, 4};       // Layers to cycle through
    std::vector<float> anim1LayersTimes = {500.0f, 500.0f, 500.0f, 500.0f, 500.0f};  // 500 ms per layer
    std::string anim1name = "example";                   // Name of the animation state
    Nothofagus::AnimationState animation1(anim1Layers, anim1LayersTimes, anim1name);

    // Create an AnimationStateMachine associated with the AnimatedBellota
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

        // Scale the AnimatedBellota to 10x10 units
        animatedbellota.transform().scale() = glm::vec2(10.0f, 10.0f);

        // Update the animation state machine with the elapsed time
        textureArrayAnimationTree.update(dt);
    };

    // Run the canvas with the update loop
    canvas.run(update);

    return 0;
}
