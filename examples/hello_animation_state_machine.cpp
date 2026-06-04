#include <array>
#include <span>
#include <string>
#include <vector>
#include <nothofagus.h>

template <std::size_t COL, std::size_t ROW>
using Grid = std::array<Nothofagus::Pixel::ColorId, COL * ROW>;

std::array<Grid<16, 16>, 8> makeBooLayers()
{
    constexpr Grid<16, 16> booBase{
        0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,
        0,0,1,1,2,2,2,2,2,2,2,1,1,0,0,0,
        0,0,1,2,2,2,2,2,2,2,2,2,3,1,0,0,
        0,1,1,2,2,2,2,2,2,2,2,2,3,3,1,0,
        0,1,2,2,1,1,2,2,2,2,2,1,1,3,1,0,
        1,1,2,2,1,1,1,2,2,2,1,1,1,3,1,1,
        1,2,2,2,1,1,1,1,2,1,1,1,1,3,3,1,
        1,2,2,1,1,1,1,1,2,1,1,1,1,1,3,1,
        1,2,2,2,1,1,1,2,2,2,1,1,1,3,3,1,
        1,2,2,2,2,2,2,2,2,2,2,2,2,3,3,1,
        1,2,2,2,2,2,2,2,2,2,2,2,3,3,3,1,
        1,2,2,2,2,2,2,3,3,2,3,3,3,3,3,1,
        1,2,2,3,2,2,3,3,3,3,3,3,3,3,3,1,
        1,2,3,1,3,3,3,1,3,3,3,1,3,3,3,1,
        1,2,1,1,1,3,1,1,1,3,1,1,1,3,1,1,
        1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,
    };

    auto setPixelId = [](Grid<16, 16>& grid, int col, int row, Nothofagus::Pixel::ColorId pixelId)
    {
        grid[static_cast<std::size_t>(row) * 16 + static_cast<std::size_t>(col)] = pixelId;
    };

    auto getPixelId = [](const Grid<16, 2>& grid, int col, int row)
    {
        return grid[static_cast<std::size_t>(row) * 16 + static_cast<std::size_t>(col)];
    };

    Nothofagus::Pixel::ColorId eyeColor = 4;

    Grid<16, 16> idleBoo = booBase;
    // left Eye
    setPixelId(idleBoo, 5, 6, eyeColor);
    setPixelId(idleBoo, 5, 7, eyeColor);

    // right Eye
    setPixelId(idleBoo, 11, 6, eyeColor);
    setPixelId(idleBoo, 11, 7, eyeColor);

    Grid<16, 16> rightBoo = booBase;
    // left Eye
    setPixelId(rightBoo, 6, 6, eyeColor);
    setPixelId(rightBoo, 6, 7, eyeColor);

    // right Eye
    setPixelId(rightBoo, 12, 6, eyeColor);
    setPixelId(rightBoo, 12, 7, eyeColor);

    Grid<16, 16> leftBoo = booBase;
    // left Eye
    setPixelId(leftBoo, 4, 6, eyeColor);
    setPixelId(leftBoo, 4, 7, eyeColor);

    // right Eye
    setPixelId(leftBoo, 10, 6, eyeColor);
    setPixelId(leftBoo, 10, 7, eyeColor);

    Grid<16, 16> upBoo = booBase;
    // left Eye
    setPixelId(upBoo, 5, 5, eyeColor);
    setPixelId(upBoo, 5, 6, eyeColor);

    // right Eye
    setPixelId(upBoo, 11, 5, eyeColor);
    setPixelId(upBoo, 11, 6, eyeColor);

    Grid<16, 2> shiftedBottom{
        1,1,3,1,1,1,3,1,1,1,3,1,1,1,3,1,
        0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,
    };

    auto overlayBottom = [&setPixelId, &getPixelId](const Grid<16, 16>& base, const Grid<16, 2>& bottom)
    {
        Grid<16, 16> output = base;
        for (std::size_t col = 0; col < 16; col++)
        {
            for (std::size_t row = 14; row < 16; row++)
            {
                Nothofagus::Pixel::ColorId bottomColor = getPixelId(bottom, col, row - 14);
                setPixelId(output, col, row, bottomColor);
            }
        }
        return output;
    };

    std::array<Grid<16, 16>, 8> layers{
        idleBoo , overlayBottom(idleBoo , shiftedBottom),  // 0, 1
        leftBoo , overlayBottom(leftBoo , shiftedBottom),  // 2, 3
        rightBoo, overlayBottom(rightBoo, shiftedBottom),  // 4, 5
        upBoo   , overlayBottom(upBoo   , shiftedBottom),  // 6, 7
    };

    return layers;
}

int main()
{
    spdlog::info("Boo Animation");

    Nothofagus::Canvas canvas({150, 100}, "Boo", {0.15, 0.15, 0.2}, 6);

    // id == palette index; id 0 stays transparent regardless of clear color.
    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.0},  // 0 transparent
        {0.0, 0.0, 0.0, 1.0},  // 1 black  (outline / eyes / mouth)
        {1.0, 1.0, 1.0, 1.0},  // 2 white  (body)
        {0.8, 0.8, 0.8, 1.0},  // 3 gray   (shading)
        {1.0, 1.0, 1.0, 1.0},  // 4 white  (eye color)
    };

    const std::array<Grid<16, 16>, 8> layers = makeBooLayers();

    Nothofagus::IndirectTexture texture({16, 16}, glm::vec4(0, 0, 0, 0), 8);
    texture.setPallete(pallete);
    for (std::size_t layer = 0; layer < layers.size(); ++layer)
        texture.setPixels(std::span<const Nothofagus::Pixel::ColorId>(layers[layer]), layer);

    Nothofagus::TextureId textureId = canvas.addTexture(texture);
    Nothofagus::BellotaId booId = canvas.addBellota({{{75.0f, 50.0f}}, textureId});

    // Each state is a 2-frame loop over its levitation pair, so the boo keeps
    // bobbing in every gaze direction.
    Nothofagus::AnimationState idle({0, 1}, {300.0f, 300.0f}, "forward");
    Nothofagus::AnimationState left ({2, 3}, {300.0f, 300.0f}, "left");
    Nothofagus::AnimationState right({4, 5}, {300.0f, 300.0f}, "right");
    Nothofagus::AnimationState up   ({6, 7}, {300.0f, 300.0f}, "up");

    Nothofagus::AnimationStateMachine machine(canvas.bellota(booId));
    machine.addState("idle", &idle);
    machine.addState("left", &left);
    machine.addState("right", &right);
    machine.addState("up", &up);
    machine.setState("idle");

    auto update = [&](float dt)
    {
        canvas.bellota(booId).transform().scale() = glm::vec2(5.0f, 5.0f);
        machine.update(dt);
    };

    // Hold a direction to look that way; release recenters the gaze. Arrows and
    // WASD are aliases. (Known demo limitation: holding two direction keys and
    // releasing one recenters even though the other is still down.)
    Nothofagus::Controller controller;
    auto goLeft    = [&]() { machine.goToState("left"); };
    auto goRight   = [&]() { machine.goToState("right"); };
    auto goUp      = [&]() { machine.goToState("up"); };
    auto goIdle    = [&]() { machine.goToState("idle"); };

    using Nothofagus::Key;
    using Nothofagus::DiscreteTrigger;
    for (Key key : {Key::LEFT, Key::A})
    {
        controller.registerAction({key, DiscreteTrigger::Press},   goLeft);
        controller.registerAction({key, DiscreteTrigger::Release}, goIdle);
    }
    for (Key key : {Key::RIGHT, Key::D})
    {
        controller.registerAction({key, DiscreteTrigger::Press},   goRight);
        controller.registerAction({key, DiscreteTrigger::Release}, goIdle);
    }
    for (Key key : {Key::UP, Key::W})
    {
        controller.registerAction({key, DiscreteTrigger::Press},   goUp);
        controller.registerAction({key, DiscreteTrigger::Release}, goIdle);
    }
    controller.registerAction({Key::DOWN, DiscreteTrigger::Press}, goIdle);
    controller.registerAction({Key::S,    DiscreteTrigger::Press}, goIdle);

    canvas.run(update, controller);

    return 0;
}
