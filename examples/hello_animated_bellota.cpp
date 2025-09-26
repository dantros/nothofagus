#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <nothofagus.h>

int main()
{
    // You can directly use spdlog to ease your logging
    spdlog::info("Hello Animated Bellota!");

    Nothofagus::Canvas canvas({150, 100}, "Hello Animated Bellota", {0.7, 0.7, 0.7}, 6);

    Nothofagus::ColorPallete pallete1{
        {0.0, 0.0, 0.0, 1.0}
    };
    Nothofagus::ColorPallete pallete2{
        {1.0, 0.0, 0.0, 1.0}
    };
    Nothofagus::ColorPallete pallete3{
        {0.0, 1.0, 0.0, 1.0}
    };
    Nothofagus::ColorPallete pallete4{
        {0.0, 0.0, 1.0, 1.0}
    };
    Nothofagus::ColorPallete pallete5{
        {1.0, 1.0, 0.0, 1.0}
    };



    //// test de texture arrays
    spdlog::info("textureArray");
    Nothofagus::TextureArray textureArray({ 4, 4 }, 5);
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

    Nothofagus::TextureId textureId = canvas.addTextureArray(textureArray);
    Nothofagus::BellotaId animatedBellotaId = canvas.addAnimatedBellota({{{75.0f, 50.0f}}, textureId, 5});

    std::vector<int> anim1Layers = {0, 1};
    std::vector<float> anim1LayersTimes = {500.0f, 1000.0f};
    std::string anim1name = "W";
    Nothofagus::AnimationState animation1(anim1Layers, anim1LayersTimes, anim1name);
    
    std::vector<int> anim2Layers = {2, 3};
    std::vector<float> anim2LayersTimes = {1500.0f, 2000.0f};
    std::string anim2name = "S";
    Nothofagus::AnimationState animation2(anim2Layers, anim2LayersTimes, anim2name);
    
    std::vector<int> anim3Layers = {4, 0};
    std::vector<float> anim3LayersTimes = {3000.0f, 4000.0f};
    std::string anim3name = "D";
    Nothofagus::AnimationState animation3(anim3Layers, anim3LayersTimes, anim3name);
    
    std::vector<int> anim4Layers = {1, 3};
    std::vector<float> anim4LayersTimes = {500.0f, 500.0f};
    std::string anim4name = "Wleft";
    Nothofagus::AnimationState animation4(anim4Layers, anim4LayersTimes, anim4name);
    
    std::vector<int> anim5Layers = {2, 4};
    std::vector<float> anim5LayersTimes = {500.0f, 500.0f};
    std::string anim5name = "Wright";
    Nothofagus::AnimationState animation5(anim5Layers, anim5LayersTimes, anim5name);
    
    std::vector<int> anim6Layers = {3};
    std::vector<float> anim6LayersTimes = {500.0f};
    std::string anim6name = "Sleft";
    Nothofagus::AnimationState animation6(anim6Layers, anim6LayersTimes, anim6name);
    
    std::vector<int> anim7Layers = {4};
    std::vector<float> anim7LayersTimes = {500.0f};
    std::string anim7name = "Sright";
    Nothofagus::AnimationState animation7(anim7Layers, anim7LayersTimes, anim7name);

    Nothofagus::AnimationStateMachine textureArrayAnimationTree(canvas.animatedBellota(animatedBellotaId));

    textureArrayAnimationTree.addState(anim1name, &animation1);
    textureArrayAnimationTree.addState(anim2name, &animation2);
    textureArrayAnimationTree.addState(anim3name, &animation3);
    textureArrayAnimationTree.addState(anim4name, &animation4);
    textureArrayAnimationTree.addState(anim5name, &animation5);
    textureArrayAnimationTree.addState(anim6name, &animation6);
    textureArrayAnimationTree.addState(anim7name, &animation7);

    // right transitions
    std::string right_t = "right";
    textureArrayAnimationTree.newAnimationTransition(anim1name, right_t, anim5name);
    textureArrayAnimationTree.newAnimationTransition(anim2name, right_t, anim7name);
    
    // left transitions
    std::string left_t = "left";
    textureArrayAnimationTree.newAnimationTransition(anim1name, left_t, anim4name);
    textureArrayAnimationTree.newAnimationTransition(anim2name, left_t, anim6name);
    
    textureArrayAnimationTree.setState(anim1name);

    //// fin de test de texture arrays   


    float time = 0.0f;


    auto update = [&](float dt)
    {
        time += dt;


        Nothofagus::Bellota& animatedbellota = canvas.animatedBellota(animatedBellotaId);
        animatedbellota.transform().scale() = glm::vec2(10.0f, 10.0f);
        textureArrayAnimationTree.update(dt);
        
        // std::cout << animatedbellota.texture().id << std::endl;

        // you can directly use ImGui
        // ImGui::Begin("Hello there!");
        // ImGui::Text("May ImGui be with you...");
        // ImGui::End();
    };
    int l = 0;
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
    
    canvas.run(update, controller);
    
    return 0;
}