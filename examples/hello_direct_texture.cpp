#include <iostream>
#include <string>
#include <vector>
#include <ciso646>
#include <cmath>
#include <nothofagus.h>

int main()
{
    Nothofagus::Canvas canvas({150, 100}, "Hello Direct Texture!", {0.7, 0.7, 0.7}, 6);

    Nothofagus::DirectTexture texture({ 5, 5 });
    texture.setColor(0,0, glm::vec4(0,0,0,1));
    texture.setColor(0,1, glm::vec4(1,1,1,1));
    texture.setColor(1,1, glm::vec4(1,0,0,1));
    texture.setColor(2,1, glm::vec4(0,1,0,1));
    texture.setColor(3,1, glm::vec4(0,0,1,1));
    texture.setColor(1,2, glm::vec4(1,1,0,1));
    texture.setColor(2,2, glm::vec4(0,1,1,1));
    texture.setColor(3,2, glm::vec4(1,0,1,1));
    texture.setColor(1,3, glm::vec4(0,0,0,1));
    texture.setColor(2,3, glm::vec4(0,0,0,1));
    texture.setColor(3,3, glm::vec4(1,1,1,1));
    Nothofagus::TextureId textureId = canvas.addTexture(texture);

    Nothofagus::BellotaId bellotaId1 = canvas.addBellota({{{30.0f, 80.0f}, 4}, textureId});
    Nothofagus::BellotaId bellotaId2 = canvas.addBellota({{{0.0f, 50.0f}, 8}, textureId});
    Nothofagus::BellotaId bellotaId3 = canvas.addBellota({{{50.0f, 20.0f}, 4}, textureId});

    canvas.setTint(bellotaId3, {0.5, glm::vec3(1, 0, 0)});

    float time = 0.0f;
    bool rotate = true;
    bool visible = true;

    auto update = [&](float dt)
    {
        time += dt;

        Nothofagus::Bellota& bellota2 = canvas.bellota(bellotaId2);
        bellota2.transform().location().x = 75.0f + 60.0f * std::sin(0.0005f * time);
    };
    
    canvas.run(update);

    return 0;
}