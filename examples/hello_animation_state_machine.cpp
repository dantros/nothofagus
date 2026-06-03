#include <array>
#include <span>
#include <string>
#include <vector>
#include <nothofagus.h>

// Boo ghost animation demo.
//
// The sprite is hand-encoded from boo.png (16x16). Its eyes track the user's
// active direction (left / right / up, recentering to a forward gaze when no
// direction key is held), and the whole ghost bobs up and down across 2 frames
// to fake a levitation float.
//
// Palette ids (index == palette slot): 0 transparent, 1 black, 2 white, 3 gray.
// Array row 0 renders at the TOP of the sprite, so the grid below reads upright.

namespace
{
    using Grid = std::array<Nothofagus::Pixel::ColorId, 16 * 16>;

    // Forward-gaze Boo, quantized straight from boo.png.
    constexpr Grid booBase{
        0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,
        0,0,1,1,2,2,2,2,2,2,2,1,1,0,0,0,
        0,0,1,2,2,2,2,2,2,2,2,2,3,1,0,0,
        0,1,1,2,2,2,2,2,2,2,2,2,3,3,1,0,
        0,1,2,2,1,1,2,2,2,2,2,1,1,3,1,0,
        1,1,2,2,1,1,1,2,2,2,1,1,1,3,1,1,
        1,2,2,2,1,1,2,1,2,1,1,2,1,3,3,1,
        1,2,2,1,1,1,2,1,2,1,1,2,1,1,3,1,
        1,2,2,2,1,1,1,2,2,2,1,1,1,3,3,1,
        1,2,2,2,2,2,2,2,2,2,2,2,2,3,3,1,
        1,2,2,2,2,2,2,2,2,2,2,2,3,3,3,1,
        1,2,2,2,2,2,2,3,3,2,3,3,3,3,3,1,
        1,2,2,3,2,2,3,3,3,3,3,3,3,3,3,1,
        1,2,3,1,3,3,3,1,3,3,3,1,3,3,3,1,
        1,2,1,1,1,3,1,1,1,3,1,1,1,3,1,1,
        1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,
    };

    Nothofagus::Pixel::ColorId& at(Grid& grid, int col, int row)
    {
        return grid[static_cast<std::size_t>(row) * 16 + static_cast<std::size_t>(col)];
    }

    // Clear the whole eye band to white so eyes can be redrawn cleanly anywhere.
    void eraseEyes(Grid& grid)
    {
        for (int row = 4; row <= 8; ++row)
            for (int col = 4; col <= 11; ++col)
                at(grid, col, row) = 2;
    }

    // Stamp a 2x5 black eye, but only over white pixels so a shifted eye can
    // never punch black into the transparent silhouette margin.
    void drawEye(Grid& grid, int col, int row)
    {
        for (int dy = 0; dy < 5; ++dy)
            for (int dx = 0; dx < 2; ++dx)
            {
                const int x = col + dx, y = row + dy;
                if (x >= 0 && x < 16 && y >= 0 && y < 16 && at(grid, x, y) == 2)
                    at(grid, x, y) = 1;
            }
    }

    // Forward eyes sit at (4,4) and (10,4); offsets shift the gaze.
    Grid makeFace(int dCol, int dRow)
    {
        Grid grid = booBase;
        eraseEyes(grid);
        drawEye(grid, 4 + dCol, 4 + dRow);
        drawEye(grid, 10 + dCol, 4 + dRow);
        return grid;
    }

    // Levitation bob: the "high" frame is the ghost at rest (full sprite); the
    // "low" frame sinks the whole ghost down one row, the vacated top row going
    // transparent. Sinking (rather than rising) clips only the bottom fringe
    // instead of the top-of-head outline, so the silhouette stays clean.
    Grid levitate(const Grid& face, bool high)
    {
        if (high)
            return face;
        Grid shifted{};
        for (int row = 0; row < 15; ++row)
            for (int col = 0; col < 16; ++col)
                at(shifted, col, row + 1) = face[static_cast<std::size_t>(row) * 16 + col];
        return shifted;
    }
}

int main()
{
    spdlog::info("Boo Ghost Animation");

    Nothofagus::Canvas canvas({150, 100}, "Boo Ghost", {0.15, 0.15, 0.2}, 6);

    // id == palette index; id 0 stays transparent regardless of clear color.
    Nothofagus::ColorPallete pallete{
        {0.0, 0.0, 0.0, 0.0},  // 0 transparent
        {0.0, 0.0, 0.0, 1.0},  // 1 black  (outline / eyes / mouth)
        {1.0, 1.0, 1.0, 1.0},  // 2 white  (body)
        {0.5, 0.5, 0.5, 1.0},  // 3 gray   (shading)
    };

    // 8 layers = {forward, left, right, up} x {bob-low, bob-high}.
    const Grid forwardFace = makeFace(0, 0);
    const Grid leftFace    = makeFace(-1, 0);
    const Grid rightFace   = makeFace(1, 0);
    const Grid upFace      = makeFace(0, -1);

    const std::array<Grid, 8> layers{
        levitate(forwardFace, false), levitate(forwardFace, true),  // 0, 1
        levitate(leftFace,    false), levitate(leftFace,    true),  // 2, 3
        levitate(rightFace,   false), levitate(rightFace,   true),  // 4, 5
        levitate(upFace,      false), levitate(upFace,      true),  // 6, 7
    };

    Nothofagus::IndirectTexture texture({16, 16}, glm::vec4(0, 0, 0, 0), 8);
    texture.setPallete(pallete);
    for (std::size_t layer = 0; layer < layers.size(); ++layer)
        texture.setPixels(std::span<const Nothofagus::Pixel::ColorId>(layers[layer]), layer);

    Nothofagus::TextureId textureId = canvas.addTexture(texture);
    Nothofagus::BellotaId ghostId = canvas.addBellota({{{75.0f, 50.0f}}, textureId});

    // Each state is a 2-frame loop over its levitation pair, so the ghost keeps
    // bobbing in every gaze direction.
    Nothofagus::AnimationState forward({0, 1}, {300.0f, 300.0f}, "forward");
    Nothofagus::AnimationState left   ({2, 3}, {300.0f, 300.0f}, "left");
    Nothofagus::AnimationState right  ({4, 5}, {300.0f, 300.0f}, "right");
    Nothofagus::AnimationState up     ({6, 7}, {300.0f, 300.0f}, "up");

    Nothofagus::AnimationStateMachine machine(canvas.bellota(ghostId));
    machine.addState("forward", &forward);
    machine.addState("left",    &left);
    machine.addState("right",   &right);
    machine.addState("up",      &up);
    machine.setState("forward");

    auto update = [&](float dt)
    {
        canvas.bellota(ghostId).transform().scale() = glm::vec2(5.0f, 5.0f);
        machine.update(dt);
    };

    // Hold a direction to look that way; release recenters the gaze. Arrows and
    // WASD are aliases. (Known demo limitation: holding two direction keys and
    // releasing one recenters even though the other is still down.)
    Nothofagus::Controller controller;
    auto goLeft    = [&]() { machine.goToState("left"); };
    auto goRight   = [&]() { machine.goToState("right"); };
    auto goUp      = [&]() { machine.goToState("up"); };
    auto goForward = [&]() { machine.goToState("forward"); };

    using Nothofagus::Key;
    using Nothofagus::DiscreteTrigger;
    for (Key key : {Key::LEFT, Key::A})
    {
        controller.registerAction({key, DiscreteTrigger::Press},   goLeft);
        controller.registerAction({key, DiscreteTrigger::Release}, goForward);
    }
    for (Key key : {Key::RIGHT, Key::D})
    {
        controller.registerAction({key, DiscreteTrigger::Press},   goRight);
        controller.registerAction({key, DiscreteTrigger::Release}, goForward);
    }
    for (Key key : {Key::UP, Key::W})
    {
        controller.registerAction({key, DiscreteTrigger::Press},   goUp);
        controller.registerAction({key, DiscreteTrigger::Release}, goForward);
    }
    controller.registerAction({Key::DOWN, DiscreteTrigger::Press}, goForward);
    controller.registerAction({Key::S,    DiscreteTrigger::Press}, goForward);

    canvas.run(update, controller);

    return 0;
}
