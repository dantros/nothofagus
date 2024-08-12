# Nothofagus

Sandbox C++ real time renderer using OpenGL 3.3 under the hood.
You define some textures in your code, some dynamic locations and you are redy to quick start your game.
Nothofagus also gives you access to ImGui and other third party libs to speed up your development journey.

```

```

![screenshot](assets/screenshot.webp "screenshot")

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
