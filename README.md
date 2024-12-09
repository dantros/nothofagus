# Nothofagus Renderer

Sandbox C++ pixel art real time renderer using OpenGL 3.3 under the hood.
You define some textures in your code, some dynamic locations and you are ready to quick start your game.
Nothofagus also gives you access to ImGui and other third party libs to speed up your development journey.

Your code will look like this.

```
Nothofagus::ColorPallete pallete{
    {0.0, 0.0, 0.0, 0.0},
    {0.0, 0.4, 0.0, 1.0},
    {0.2, 0.8, 0.2, 1.0},
    {0.5, 1.0, 0.5, 1.0},
};

Nothofagus::Texture texture({8, 8}, {0.5, 0.5, 0.5, 1.0});
texture.setPallete(pallete)
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
Nothofagus::TextureId textureId = canvas.addTexture(texture);
Nothofagus::BellotaId bellotaId = canvas.addBellota({{{75.0f, 75.0f}}, textureId});

canvas.run(update);
```

A `Bellota` is a drawable element. Each number in the texture corresponds to the index of the color specified in the `TexturePallete`. Yes, it is an indirect color scheme.

You can make animations by providing an `update` function.

```
float time = 0.0f;
bool rotate = true;

auto update = [&](float dt)
{
    time += dt;

    ImGui::Begin("Hello there!");
    ImGui::Text("May ImGui be with you...");
    ImGui::Checkbox("Rotate?", &rotate);
    if (rotate)
    {
        Nothofagus::Bellota& bellota = canvas.bellota(bellotaId);
        bellota.transform().angle() = 0.1f * time;
    }
    ImGui::End();
};

canvas.run(update);
```

And if you want to make it interactive, just add a `Controller`

```
Nothofagus::Controller controller;
controller.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press}, [&]()
{
    canvas.bellota(bellotaId).transform().location().y += 10.0f;
});

canvas.run(update, controller);
```

This is a screenshot of [examples/hello_nothofagus.cpp](examples/hello_nothofagus.cpp)
![screenshot](assets/screenshot.webp "screenshot")

## Setting up your project

You can use this repository as a git submodule

```
git submodule add https://github.com/dantros/nothofagus.git third_party/nothofagus
```

And then use `add_subdirectory` from your project's CMake file.

```
option(NOTHOFAGUS_INSTALL "Disabling installation of Nothofagus" OFF)
add_subdirectory("third_party/nothofagus")

add_executable(nothofagus_demo
    "source/nothofagus_demo.cpp"
)
set_property(TARGET nothofagus_demo PROPERTY CXX_STANDARD 20)
target_include_directories(nothofagus_demo PRIVATE ${NOTHOFAGUS_INCLUDE})
target_link_libraries(nothofagus_demo PRIVATE nothofagus)
```

You can check the friend repo [nothofagus_demo](https://github.com/dantros/nothofagus_demo) with a full example.

Of course there are other ways to work, choose whatever suits you best.

## Quick start

```
git clone https://github.com/dantros/nothofagus.git
cd nothofagus
git submodule update --init --recursive
cmake --presets ninja-release
cd ../build_cmake/ninja-release/
ninja
ninja install
cd ../install_cmake/ninja-release/
```

There you will fing the nothofagus static library and some demos.

## Dependencies

You should have [cmake](https://cmake.org/), [ninja](https://ninja-build.org/) and [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/) (or another proper compiler) in your development environment.

## New Features

Nothofagus was a single-sprite-per-object engine, thus for representing movement or interaction with the world depicted in the scene it meant using sound effects and/or a new object appearing. Visual motion was not something easily achivable for every case and needed to hardcode a lot of objects for every frame of the movement.

Motivated by this, a feature to improve was the ability to animate characters by accumulating frames in a texture array, and creating a way to manage transitions between different animations.

For this assignment, an animation method was implemented in a class called AnimatedBellota, which uses array textures (also known as texture atlases, see more at https://www.khronos.org/opengl/wiki/Array_Texture) to send information about the sprites that could be shown for the character. These sprites are distributed on the layers of the texture.

A state machine was created for animation transitions. It's nodes are animation states, associated with the layers of an animation and the time intervals each has to be shown. It also has a transition map, which allows to move from one state to another along the edges of the departing node.

### Animated Bellota

The AnimatedBellota class represents a graphical element in the system capable of dynamically switching between multiple layers (has a variable with that name) of a TextureArray. This allows it to display animations by cycling through the layers of a texture array, which is especially useful for representing animated characters or objects with dynamic states.

#### Why is AnimatedBellota different from Bellota?

While Bellota is designed to represent static elements that use a single texture (TextureId), AnimatedBellota extends this functionality to support multiple layers of textures via a TextureArray. The key differences are:

- Support for TextureArray: AnimatedBellota uses a TextureArrayId instead of a TextureId, enabling it to access textures arrays in the canvas.
- Animation Capabilities: It includes logic to manage which layer is currently displayed using actualLayer, a property that can dynamically change to show different animation frames.
- Flexibility for Graphical Representation: While Bellota is suitable for static elements like backgrounds or stationary objects, AnimatedBellota is ideal for characters, enemies, or other objects requiring real-time visual changes.

Implementations has numerous similarities with Bellota, including a special vector for AnimatedBellotas (and for TextureArrays) in canvas, its own fragment shader, use of a mesh, etc. Yet it has its own way to be loaded to GPU, stores different inforamtion and keeps states (layer been shown).

#### How to Use AnimatedBellota

- Creating an AnimatedBellota: An AnimatedBellota is created with a Transform, a TextureArrayId, and the number of layers in the TextureArray. As in Bellota an optional depthOffset can be included for z-ordering.

- Dynamic Layer Changes: You can change the currently visible layer using the setActualLayer method. This is crucial for handling transitions between frames in an animation.

- GPU Rendering: In the OpenGL implementation, AnimatedBellota instances are rendered using a specific shader program (mAnimatedShaderProgram) that considers the TextureArray layers and applies real-time transformations.

- Management in Canvas: The Canvas class allows adding and managing AnimatedBellota instances through methods like addAnimatedBellota and animatedBellota. This centralizes graphical element handling.

#### Advantages

- Graphical Optimization: By using TextureArray and specific shaders, it minimizes the overhead of switching textures on the GPU.
- Flexibility: Supports various types of animations, including looping and custom transitions.

### AnimationState

The AnimationState class encapsulates the logic for managing animation frames for an individual animation state.

#### Purpose:

- To represent a sequence of texture array layers (mLayers) associated with a particular animation, cycling through them based on custom timings (mTimes).
- Supports seamless transitions between layers based on elapsed time (deltaTime).

#### Key Attributes:

1. mLayers: A vector of integers representing the indices of texture array layers.
2. mTimes: A vector of floats specifying the time duration for each layer in mLayers.
3. mCurrentLayerIndex: Tracks the current layer in the animation sequence.
4. mTimeAccumulator: Accumulates elapsed time to determine when to switch to the next layer.
5. mName: A string to identify the animation state.

#### Core Methods:

1. update(float deltaTime):

   > - Updates the animation state by checking if the current layer's time duration has elapsed.
   > - Switches to the next layer in the sequence and resets the accumulator when the duration is met.

2. getCurrentLayer() const: Returns the current texture array layer being displayed.

3. getName() const: Provides the name of the animation state.

4. reset(): Resets the animation state to its initial configuration by setting the layer index and time accumulator to 0.

### AnimationStateMachine

The AnimationStateMachine manages multiple AnimationState objects and facilitates transitions between them.

Purpose:

- To control animations dynamically, allowing for state-specific behavior and transitions.
- Integrates with the AnimatedBellota class to update texture layers.

#### Key Components:

- mAnimationStates: A map linking state names (State, a std::string) to their corresponding AnimationState pointers.

- transitions: A map that defines transitions between states. Keys are (State, transition_name) pairs, and values are the resulting states.

- currentState: Tracks the name of the active animation state.

- mAnimatedBellota: A reference to the AnimatedBellota instance associated with this state machine, used for updating its texture layer.

#### Core Methods:

1. addState(const State& stateName, AnimationState\* state): Adds a new AnimationState to the state machine.

2. setState(const State& stateName): Sets the initial state of the animation.

3. newAnimationTransition(const State& state, const std::string& transition_name, const State& resultingState): Adds a new state transition rule, specifying the resulting state when a given transition is triggered from a specific state.

4. transition(const std::string& transition_name):

   > - Executes a state transition based on the current state and the provided transition name.
   > - Resets the new state's animation progress.

5. goToState(const State& stateName): Directly changes to a specific state without requiring a named transition.

6. update(float deltaTime): Updates the animation of the current state and adjusts the texture layer of the associated AnimatedBellota.

7. getCurrentLayer() const: Retrieves the current texture layer of the active animation state.

## Creating Documentation

This project uses Doxygen to create documentation, so you need to have Doxygen installed (https://www.doxygen.nl/manual/install.html) and once in this directory you should run the next command:

```bash
    doxygen Doxyfile
```

You can also use ninja to generate documentation at installation. In this directory you should run the following command:

```bash
    cmake --preset ninja-release-examples
```

Then change directory to:

```bash
    ../build/ninja-release-examples
```

Here you can install with

```bash
    ninja install
```

And generate documentation with

```bash
    ninja doc_doxygen
```
