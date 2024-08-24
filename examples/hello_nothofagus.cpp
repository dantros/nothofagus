//#include <nothofagus.h>

#include <iostream>
#include <string>
#include <vector>
#include <ciso646>

#include "../source/texture.h"
#include "../source/canvas.h"

int main()
{
    Nothofagus::Canvas canvas({800,800}, "Hello Nothofagus", {0.5,0.5,0.5});

    Nothofagus::ColorPallete pallete1{
        {0.0, 0.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {0.0, 0.0, 1.0, 1.0},
        {1.0, 1.0, 0.0, 1.0},
        {0.0, 1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 0.0}
    };

    /*Nothofagus::ColorPallete pallete2{
        {0.0, 0.0, 0.0, 0.0},
        {0.0, 0.4, 0.0, 1.0},
        {0.2, 0.8, 0.2, 1.0},
        {0.5, 1.0, 0.5, 1.0},
    };*/

    Nothofagus::Texture texture1({ 4, 4 }, { 0.5, 0.5, 0.5, 1.0 });
    texture1.setPallete(pallete1)
    .setPixels(
        {
            0,1,2,3,
            4,5,6,7,
            0,1,2,3,
            4,5,6,7
        }
    );    
    Nothofagus::TextureId textureId1 = canvas.addTexture(texture1);
    
    /*Nothofagus::Texture texture2({8, 8}, {0.5, 0.5, 0.5, 1.0});
    texture2.setPallete(pallete2)
        .setPixels(
        {
            2,1,3,0,0,3,2,1,
            2,1,1,0,0,0,2,1,
            2,1,1,1,0,0,2,1,
            2,1,2,1,1,0,2,1,
            2,1,0,2,1,1,2,1,
            2,1,0,0,2,1,2,1,
            2,1,0,0,0,2,2,1,
            2,1,3,0,0,3,2,1,
        }
    );
    Nothofagus::TextureId textureId2 = canvas.addTexture(texture2);*/

    Nothofagus::BellotaId bellotaId1 = canvas.addBellota({{{0.0f, 0.0f}, 45.0, 0.125}, textureId1});
    /*Nothofagus::BellotaId bellotaId2 = canvas.addBellota({{{0.0f, 0.75f}}, textureId1});
    Nothofagus::BellotaId bellotaId3 = canvas.addBellota({ {{-0.5f, -0.5f}}, textureId2 });
    Nothofagus::BellotaId bellotaId4 = canvas.addBellota({ {{0.0f, 0.0f}}, textureId2 });
    */
    canvas.run();

    /*bool movingRight = true;
    float speed = 0.0001;

    auto update = [&](float dt)
    {
        Nothofagus::Bellota& bellota = canvas.bellota(bellotaId2);

        if (movingRight)
            bellota.transform().tx() += speed * dt;
        else
            bellota.transform().tx() -= speed * dt;

        if (bellota.transform().tx() > 0.5)
            movingRight = false;
        else if (bellota.transform().tx() < -0.5)
            movingRight = true;
    };

    Nothofagus::Controller controller;
    controller.onKeyPress(Nothofagus::Key::W, [&]() { speed *= 2.0f; });
    controller.onKeyPress(Nothofagus::Key::S, [&]() { speed /= 2.0f; });
    controller.onKeyPress(Nothofagus::Key::ESCAPE, [&]() { canvas.close(); });
    
    canvas.run(controller, update);*/
    
    return 0;
}