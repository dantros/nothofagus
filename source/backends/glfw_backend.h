#pragma once

#include "canvas.h"
#include "controller.h"
#include "aa_box.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <array>
#include <utility>

namespace Nothofagus
{

// Input context stored as GLFW window user pointer so static callbacks can
// access the controller and the current letterboxed viewport.
struct GlfwInputContext
{
    Controller* controller = nullptr;
    ViewportRect viewport  = {};
    ScreenSize screenSize  = {};
};

class GlfwBackend
{
public:
    GlfwBackend(const std::string& title, int width, int height);
    ~GlfwBackend();

    // Non-copyable, non-movable (owns a GLFW window handle)
    GlfwBackend(const GlfwBackend&)            = delete;
    GlfwBackend& operator=(const GlfwBackend&) = delete;

    void initImGuiPlatform();

    float contentScale() const { return mContentScale; }
    void* nativeHandle() const { return static_cast<void*>(mGlfwWindow); }

    void beginSession(Controller& controller);

    bool isRunning() const;

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
    GLFWwindow*       mGlfwWindow = nullptr;
    float             mContentScale = 1.0f;
    GlfwInputContext  mInputContext;
    std::array<GLFWgamepadstate, GLFW_JOYSTICK_LAST + 1> mPreviousGamepadStates = {};
    std::array<bool,             GLFW_JOYSTICK_LAST + 1> mGamepadConnectedState  = {};
};

} // namespace Nothofagus
