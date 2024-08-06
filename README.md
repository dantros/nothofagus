# C++ OpenGL App Template

Basic C++ project configuration to start a simple OpenGL based application.

It includes a minimal setup with the following libraries:
- OpenGL 3.3 via glad
- GLFW3
- glm
- Dear ImGui
- argparse
- json
- spdlog
- whereami2cpp

CMake is used as build system and it should work out of the box.

[`main.cpp`](source/main.cpp) includes demo code using all mentioned libraries.

![screenshot](assets/screenshot.webp "screenshot")

## Quick start

You can start by clicking 'Use this template' at the upper right corner of the Github website. You will get a clone of this repo in your repositories to start working.

After making a local clone, you need to initialize and update the git submodules by executing at the root of the repository:
```
git submodule update --init --recursive
```
Once you are done, you can generate the build files with cmake presets.
```
cmake --presets ninja-release
```
And then, just go to `../build_cmake/ninja-release/` and execute
```
ninja
```
and you will get your binary file in the build directory. To install the target just execute
```
ninja install
```
your binary will be copied to `../install_cmake/ninja-release/`. It will be free from every other build dependency or CMake artifact.

You can also get a Visual Studio solution file for your convenience, check [CMakePresets.json](CMakePresets.json) file.

## Dependencies

You should have [cmake](https://cmake.org/), [ninja](https://ninja-build.org/) and [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/) (or another proper compiler) in your development environment.
