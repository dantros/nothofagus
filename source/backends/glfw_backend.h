#pragma once

#include "canvas.h"
#include "controller.h"
#include "aa_box.h"
#include "check.h"
#include "keyboard.h"
#include "mouse.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <glm/vec2.hpp>
#include <string>
#include <array>
#include <utility>
#include <cmath>
#include <stdexcept>

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

static void glfwKeyCallback(GLFWwindow* window, int glfwKey, int scancode, int action, int mods)
{
    ImGui_ImplGlfw_KeyCallback(window, glfwKey, scancode, action, mods);

    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    if (not (action == GLFW_PRESS or action == GLFW_RELEASE))
        return;

    auto* ctx = static_cast<GlfwInputContext*>(glfwGetWindowUserPointer(window));
    debugCheck(ctx != nullptr, "GLFW key callback: window user pointer is null");

    Key key = KeyboardImplementation::toKeyCode(glfwKey);
    DiscreteTrigger trigger = (action == GLFW_PRESS) ? DiscreteTrigger::Press : DiscreteTrigger::Release;
    ctx->controller->activate({key, trigger});
}

static void glfwMouseButtonCallback(GLFWwindow* window, int glfwButton, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, glfwButton, action, mods);

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    if (not (action == GLFW_PRESS or action == GLFW_RELEASE))
        return;

    auto* ctx = static_cast<GlfwInputContext*>(glfwGetWindowUserPointer(window));
    debugCheck(ctx != nullptr, "GLFW mouse button callback: window user pointer is null");

    MouseButton button = MouseImplementation::toMouseButton(glfwButton);
    DiscreteTrigger trigger = (action == GLFW_PRESS) ? DiscreteTrigger::Press : DiscreteTrigger::Release;
    ctx->controller->activateMouseButton({button, trigger});
}

static void glfwCursorPosCallback(GLFWwindow* window, double cursorX, double cursorY)
{
    ImGui_ImplGlfw_CursorPosCallback(window, cursorX, cursorY);

    auto* ctx = static_cast<GlfwInputContext*>(glfwGetWindowUserPointer(window));
    debugCheck(ctx != nullptr, "GLFW cursor pos callback: window user pointer is null");

    // cursorX/Y are in top-left window coords. Scale to framebuffer pixels (HiDPI).
    int windowWidth, windowHeight, framebufferWidth, framebufferHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    const float scaleX = (windowWidth  > 0) ? static_cast<float>(framebufferWidth)  / static_cast<float>(windowWidth)  : 1.0f;
    const float scaleY = (windowHeight > 0) ? static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight) : 1.0f;

    // Convert to framebuffer coords with bottom-left origin.
    const float fbCursorX = static_cast<float>(cursorX) * scaleX;
    const float fbCursorY = static_cast<float>(framebufferHeight) - static_cast<float>(cursorY) * scaleY;

    // Map through the letterboxed viewport to game canvas coords.
    const ViewportRect& vp = ctx->viewport;
    const glm::vec2 gamePosition = {
        (fbCursorX - static_cast<float>(vp.x)) / static_cast<float>(vp.width)  * static_cast<float>(ctx->screenSize.width),
        (fbCursorY - static_cast<float>(vp.y)) / static_cast<float>(vp.height) * static_cast<float>(ctx->screenSize.height)
    };
    ctx->controller->updateMousePosition(gamePosition);
}

static void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    auto* ctx = static_cast<GlfwInputContext*>(glfwGetWindowUserPointer(window));
    debugCheck(ctx != nullptr, "GLFW scroll callback: window user pointer is null");

    ctx->controller->scrolled({static_cast<float>(xoffset), static_cast<float>(yoffset)});
}

static void glfwFramebufferSizeCallback(GLFWwindow* /*window*/, int /*width*/, int /*height*/) {}

class GlfwBackend
{
public:
    GlfwBackend(const std::string& title, int width, int height)
    {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        mGlfwWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!mGlfwWindow)
        {
            spdlog::error("Failed to create GLFW window");
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        float scaleWidth, scaleHeight;
        glfwGetWindowContentScale(mGlfwWindow, &scaleWidth, &scaleHeight);
        mContentScale = scaleWidth;

        glfwMakeContextCurrent(mGlfwWindow);
        glfwSetFramebufferSizeCallback(mGlfwWindow, glfwFramebufferSizeCallback);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            spdlog::error("Failed to initialize GLAD");
            throw std::runtime_error("Failed to initialize GLAD");
        }
    }

    ~GlfwBackend()
    {
        ImGui_ImplGlfw_Shutdown();
        glfwTerminate();
    }

    // Non-copyable, non-movable (owns a GLFW window handle)
    GlfwBackend(const GlfwBackend&)            = delete;
    GlfwBackend& operator=(const GlfwBackend&) = delete;

    void initImGui(float imguiFontSize, const void* fontData, int fontDataLen)
    {
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(mGlfwWindow, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(mContentScale);
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<void*>(fontData), fontDataLen, imguiFontSize * mContentScale);
    }

    float contentScale() const { return mContentScale; }

    void beginSession(Controller& controller)
    {
        mInputContext.controller = &controller;
        glfwSetWindowUserPointer(mGlfwWindow, &mInputContext);
        glfwSetKeyCallback(mGlfwWindow, glfwKeyCallback);
        glfwSetMouseButtonCallback(mGlfwWindow, glfwMouseButtonCallback);
        glfwSetCursorPosCallback(mGlfwWindow, glfwCursorPosCallback);
        glfwSetScrollCallback(mGlfwWindow, glfwScrollCallback);
        glfwSetWindowShouldClose(mGlfwWindow, false);
        mPreviousGamepadStates = {};
        mGamepadConnectedState = {};
    }

    bool isRunning() const
    {
        return !glfwWindowShouldClose(mGlfwWindow);
    }

    void newImGuiFrame()
    {
        ImGui_ImplGlfw_NewFrame();
    }

    void endFrame(Controller& controller, const ViewportRect& viewport, const ScreenSize& screenSize)
    {
        mInputContext.viewport   = viewport;
        mInputContext.screenSize = screenSize;

        glfwSwapBuffers(mGlfwWindow);
        glfwPollEvents();

        constexpr float GAMEPAD_AXIS_DEADZONE = 0.1f;

        for (int i = 0; i <= GLFW_JOYSTICK_LAST; ++i)
        {
            const bool nowConnected = glfwJoystickPresent(i) && glfwJoystickIsGamepad(i);
            if (nowConnected != mGamepadConnectedState[i])
            {
                mGamepadConnectedState[i] = nowConnected;
                if (nowConnected)
                {
                    controller.gamepadConnected(i);
                }
                else
                {
                    controller.gamepadDisconnected(i);
                    mPreviousGamepadStates[i] = {};
                    continue;
                }
            }
            if (!nowConnected)
                continue;

            GLFWgamepadstate state{};
            if (!glfwGetGamepadState(i, &state))
                continue;

            for (int buttonIndex = 0; buttonIndex <= GLFW_GAMEPAD_BUTTON_LAST; ++buttonIndex)
            {
                if (state.buttons[buttonIndex] != mPreviousGamepadStates[i].buttons[buttonIndex])
                {
                    DiscreteTrigger trigger = (state.buttons[buttonIndex] == GLFW_PRESS)
                        ? DiscreteTrigger::Press : DiscreteTrigger::Release;
                    controller.activateGamepadButton({i, static_cast<GamepadButton>(buttonIndex), trigger});
                }
            }

            for (int axisIndex = 0; axisIndex <= GLFW_GAMEPAD_AXIS_LAST; ++axisIndex)
            {
                GamepadAxis axis = static_cast<GamepadAxis>(axisIndex);
                float value = state.axes[axisIndex];

                if (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger)
                {
                    value = (value + 1.0f) * 0.5f;
                    if (value < GAMEPAD_AXIS_DEADZONE) value = 0.0f;
                }
                else
                {
                    if (axis == GamepadAxis::LeftY || axis == GamepadAxis::RightY) value = -value;
                    if (std::abs(value) < GAMEPAD_AXIS_DEADZONE) value = 0.0f;
                }
                controller.updateGamepadAxis(i, axis, value);
            }

            mPreviousGamepadStates[i] = state;
        }
    }

    std::pair<int, int> getFramebufferSize() const
    {
        int width, height;
        glfwGetFramebufferSize(mGlfwWindow, &width, &height);
        return {width, height};
    }

    float getTime() const
    {
        return static_cast<float>(glfwGetTime());
    }

    std::size_t getCurrentMonitor() const
    {
        int monitorCount;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        if (!monitors)
        {
            spdlog::error("No monitors were found");
            throw std::runtime_error("No monitors found");
        }

        const AABox windowBox = getWindowAABox();

        for (int monitorIndex = 0; monitorIndex < monitorCount; ++monitorIndex)
        {
            GLFWmonitor* currentMonitor = monitors[monitorIndex];
            debugCheck(currentMonitor != nullptr, "GLFW returned a null monitor pointer in monitors array");
            AABox monitorBox;
            glfwGetMonitorPos(currentMonitor, &monitorBox.x, &monitorBox.y);
            const GLFWvidmode* videoMode = glfwGetVideoMode(currentMonitor);
            debugCheck(videoMode != nullptr, "glfwGetVideoMode returned null for current monitor");
            monitorBox.width  = videoMode->width;
            monitorBox.height = videoMode->height;

            if (monitorBox.contains(windowBox.x, windowBox.y))
                return static_cast<std::size_t>(monitorIndex);
        }

        spdlog::warn("Top left corner of the window is outside of all monitors. Returning primary monitor.");
        return 0;
    }

    bool isFullscreen() const
    {
        return glfwGetWindowMonitor(mGlfwWindow) != nullptr;
    }

    void setFullscreenOnMonitor(std::size_t monitorIndex)
    {
        int monitorCount;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        debugCheck(static_cast<int>(monitorIndex) < monitorCount, "Monitor index out of range");
        GLFWmonitor* monitor = monitors[monitorIndex];
        debugCheck(monitor != nullptr, "GLFW returned a null pointer for the selected monitor index");
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        debugCheck(mode != nullptr, "glfwGetVideoMode returned null for the selected monitor");

        glfwSetWindowMonitor(mGlfwWindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }

    AABox getWindowAABox() const
    {
        AABox box;
        glfwGetWindowPos(mGlfwWindow, &box.x, &box.y);
        glfwGetWindowSize(mGlfwWindow, &box.width, &box.height);
        return box;
    }

    void setWindowed(const AABox& box)
    {
        glfwSetWindowMonitor(mGlfwWindow, nullptr, box.x, box.y, box.width, box.height, 0);
    }

    ScreenSize getWindowSize() const
    {
        debugCheck(mGlfwWindow != nullptr, "GLFW Window has not been initialized.");
        int width, height;
        glfwGetWindowSize(mGlfwWindow, &width, &height);
        return {static_cast<unsigned int>(width), static_cast<unsigned int>(height)};
    }

    void requestClose()
    {
        glfwSetWindowShouldClose(mGlfwWindow, true);
    }

    static ScreenSize getPrimaryMonitorSize()
    {
        glfwInit();  // idempotent — safe if Canvas has already initialised it
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (!mode)
        {
            spdlog::warn("getPrimaryMonitorSize: could not query primary monitor, returning default 1920x1080");
            return {1920, 1080};
        }
        return {static_cast<unsigned int>(mode->width), static_cast<unsigned int>(mode->height)};
    }

private:
    GLFWwindow*       mGlfwWindow = nullptr;
    float             mContentScale = 1.0f;
    GlfwInputContext  mInputContext;
    std::array<GLFWgamepadstate, GLFW_JOYSTICK_LAST + 1> mPreviousGamepadStates = {};
    std::array<bool,             GLFW_JOYSTICK_LAST + 1> mGamepadConnectedState  = {};
};

} // namespace Nothofagus
