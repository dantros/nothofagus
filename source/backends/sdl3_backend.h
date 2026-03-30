#pragma once

#include "canvas.h"
#include "controller.h"
#include "aa_box.h"
#include <SDL3/SDL.h>
#include <string>
#include <utility>
#include <unordered_map>

namespace Nothofagus
{

class Sdl3Backend
{
public:
    Sdl3Backend(const std::string& title, int width, int height, bool visible = true);
    ~Sdl3Backend();

    // Non-copyable, non-movable (owns SDL handles)
    Sdl3Backend(const Sdl3Backend&)            = delete;
    Sdl3Backend& operator=(const Sdl3Backend&) = delete;

    void initImGuiPlatform();

    float contentScale() const { return mContentScale; }
    void* nativeHandle() const { return static_cast<void*>(mSdlWindow); }

    void beginSession(Controller& controller);

    bool isRunning() const { return !mShouldClose; }

    void newImGuiFrame();

    void endFrame(Controller& controller, const ViewportRect& viewport, const ScreenSize& screenSize);

    std::pair<int, int> getFramebufferSize() const;

    float getTime() const;

    std::size_t getCurrentMonitor() const;

    bool isFullscreen() const;

    void setFullscreenOnMonitor(std::size_t monitorIndex);

    AABox getWindowAABox() const;

    void setWindowed(const AABox& box);

    ScreenSize getWindowSize() const;

    void requestClose();

    static ScreenSize getPrimaryMonitorSize();

private:
    SDL_Window*   mSdlWindow    = nullptr;
    SDL_GLContext mGlContext    = nullptr;
    bool          mShouldClose  = false;
    float         mContentScale = 1.0f;
    std::unordered_map<SDL_JoystickID, SDL_Gamepad*> mOpenGamepads;
};

} // namespace Nothofagus
