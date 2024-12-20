#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <nothofagus.h>

int main()
{
    // Initial log message using spdlog
    spdlog::info("Animation State Machine");

    // Create a canvas for rendering with dimensions, title, background color, and pixel size
    Nothofagus::Canvas canvas({150, 100}, "Hello Nothofagus", {0.7, 0.7, 0.7}, 6);

    // Define a color palette with 7 colors (RGBA format)
    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 1.0},  // Black
        {1.0, 0.0, 0.0, 1.0},  // Red
        {0.0, 1.0, 0.0, 1.0},  // Green
        {0.0, 0.0, 1.0, 1.0},  // Blue
        {1.0, 1.0, 0.0, 1.0},  // Yellow
        {1.0, 0.0, 1.0, 1.0},  // Magenta
        {1.0, 1.0, 1.0, 1.0}   // White
    };
  
    // Create a TextureArray with 4x4 textures and 7 layers
    Nothofagus::TextureArray textureArray({4, 4}, 10);

    // Initialize each layer in the texture array with specific color indices
    textureArray.setLayerPallete(pallete, 0)
        .setPixelsInLayer({
            0,1,1,0, // Layer 0
            0,1,1,0,
            0,0,0,0,
            0,0,0,0
        }, 0);
    textureArray.setLayerPallete(pallete, 1)
        .setPixelsInLayer({
            0,0,0,0, // Layer 1
            0,0,0,0,
            0,1,1,0,
            0,1,1,0
        }, 1);

    textureArray.setLayerPallete(pallete, 2)
        .setPixelsInLayer({
            3,3,0,0, // Layer 2
            3,3,0,0,
            0,0,0,0,
            0,0,0,0
        }, 2);
    textureArray.setLayerPallete(pallete, 3)
        .setPixelsInLayer({
            4,4,0,0, // Layer 3
            4,4,0,0,
            0,0,0,0,
            0,0,0,0
            }, 3);
    textureArray.setLayerPallete(pallete, 4)
        .setPixelsInLayer({
            0,0,3,3, // Layer 4
            0,0,3,3,
            0,0,0,0,
            0,0,0,0
        }, 4);
    // Similar initialization for layers 4, 5, 6...
    textureArray.setLayerPallete(pallete, 5)
        .setPixelsInLayer({
            0,0,4,4, // Layer 5
            0,0,4,4,
            0,0,0,0,
            0,0,0,0
        }, 5);
    textureArray.setLayerPallete(pallete, 6)
        .setPixelsInLayer({
            0,0,0,0, // Layer 6
            0,0,0,0,
            5,5,0,0,
            5,5,0,0
        }, 6);
    textureArray.setLayerPallete(pallete, 7)
        .setPixelsInLayer({
            0,0,0,0, // Layer 7
            0,0,0,0,
            6,6,0,0,
            6,6,0,0
        }, 7);
    textureArray.setLayerPallete(pallete, 8)
        .setPixelsInLayer({
            0,0,0,0, // Layer 6
            0,0,0,0,
            0,0,5,5,
            0,0,5,5
            }, 8);
    textureArray.setLayerPallete(pallete, 9)
        .setPixelsInLayer({
            0,0,0,0, // Layer 7
            0,0,0,0,
            0,0,6,6,
            0,0,6,6
            }, 9);

    // Add the TextureArray to the canvas and create an AnimatedBellota using it
    Nothofagus::TextureArrayId textureArrayId = canvas.addTextureArray(textureArray);
    Nothofagus::AnimatedBellotaId animatedBellotaId = canvas.addAnimatedBellota({{{75.0f, 50.0f}}, textureArrayId, 5});

    // Define animation states (layers, times, names)
    Nothofagus::AnimationState animation1({0}, {500.0f}, "W");
    Nothofagus::AnimationState animation2({1}, {500.0f}, "S");
    Nothofagus::AnimationState animation4({2, 3}, { 500.0f, 500.0f }, "Wleft");
    Nothofagus::AnimationState animation5({4, 5}, { 500.0f, 500.0f }, "Wright");
    Nothofagus::AnimationState animation6({6, 7}, { 500.0f, 500.0f }, "Sleft");
    Nothofagus::AnimationState animation7({8, 9}, { 500.0f, 500.0f }, "Sright");

    // Create an AnimationStateMachine associated with the AnimatedBellota
    Nothofagus::AnimationStateMachine textureArrayAnimationTree(canvas.animatedBellota(animatedBellotaId));

    // Add animation states to the state machine
    textureArrayAnimationTree.addState("W", &animation1);
    textureArrayAnimationTree.addState("S", &animation2);
    textureArrayAnimationTree.addState("Wleft", &animation4);
    textureArrayAnimationTree.addState("Wright", &animation5);
    textureArrayAnimationTree.addState("Sleft", &animation6);
    textureArrayAnimationTree.addState("Sright", &animation7);

    // Define transitions for "right" movement
    std::string right_t = "right";
    textureArrayAnimationTree.newAnimationTransition("W", right_t, "Wright");
    textureArrayAnimationTree.newAnimationTransition("Wleft", right_t, "Wright");
    textureArrayAnimationTree.newAnimationTransition("Wright", right_t, "Wright");
    textureArrayAnimationTree.newAnimationTransition("S", right_t, "Sright");
    textureArrayAnimationTree.newAnimationTransition("Sleft", right_t, "Sright");
    textureArrayAnimationTree.newAnimationTransition("Sright", right_t, "Sright");
    
    // Define transitions for "left" movement
    std::string left_t = "left";
    textureArrayAnimationTree.newAnimationTransition("W", left_t, "Wleft");
    textureArrayAnimationTree.newAnimationTransition("Wright", left_t, "Wleft");
    textureArrayAnimationTree.newAnimationTransition("Wleft", left_t, "Wleft");
    textureArrayAnimationTree.newAnimationTransition("S", left_t, "Sleft");
    textureArrayAnimationTree.newAnimationTransition("Sright", left_t, "Sleft");
    textureArrayAnimationTree.newAnimationTransition("Sleft", left_t, "Sleft");
    
    // Set the initial animation state to "W"
    textureArrayAnimationTree.setState("W");

    // Game loop initialization
    float time = 0.0f;

    // Define the update function
    auto update = [&](float dt)
    {
        time += dt;

        // Update AnimatedBellota properties and AnimationStateMachine
        Nothofagus::AnimatedBellota& animatedbellota = canvas.animatedBellota(animatedBellotaId);
        animatedbellota.transform().scale() = glm::vec2(10.0f, 10.0f);
        textureArrayAnimationTree.update(dt);
    };

    // Define input controller actions
    Nothofagus::Controller controller;
    controller.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        textureArrayAnimationTree.goToState("W");
    });
    controller.registerAction({Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        textureArrayAnimationTree.goToState("S");
    });
    controller.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        textureArrayAnimationTree.transition("left");
    });
    controller.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Press}, [&]()
    {
        textureArrayAnimationTree.transition("right");
    });

    // Run the canvas with the update and controller logic
    canvas.run(update, controller);

    return 0;
}
