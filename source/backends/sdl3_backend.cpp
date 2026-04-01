#include "sdl3_backend.h"
#include "check.h"
#include "keyboard.h"
#include "mouse.h"
#include <glad/glad.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <spdlog/spdlog.h>
#include <glm/vec2.hpp>
#include <cmath>
#include <stdexcept>

namespace Nothofagus
{

static GamepadButton sdlButtonToGamepadButton(SDL_GamepadButton button)
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:          return GamepadButton::A;
    case SDL_GAMEPAD_BUTTON_EAST:           return GamepadButton::B;
    case SDL_GAMEPAD_BUTTON_WEST:           return GamepadButton::X;
    case SDL_GAMEPAD_BUTTON_NORTH:          return GamepadButton::Y;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return GamepadButton::LeftBumper;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return GamepadButton::RightBumper;
    case SDL_GAMEPAD_BUTTON_BACK:           return GamepadButton::Back;
    case SDL_GAMEPAD_BUTTON_START:          return GamepadButton::Start;
    case SDL_GAMEPAD_BUTTON_GUIDE:          return GamepadButton::Guide;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return GamepadButton::LeftThumb;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return GamepadButton::RightThumb;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:        return GamepadButton::DpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return GamepadButton::DpadRight;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return GamepadButton::DpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return GamepadButton::DpadLeft;
    default:                                return GamepadButton::A;
    }
}

static GamepadAxis sdlAxisToGamepadAxis(SDL_GamepadAxis axis)
{
    // SDL3 axis enum order matches GamepadAxis order — direct cast is valid.
    return static_cast<GamepadAxis>(axis);
}

Sdl3Backend::Sdl3Backend(const std::string& title, int width, int height, bool visible)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_WindowFlags windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (!visible)
        windowFlags |= SDL_WINDOW_HIDDEN;

    mSdlWindow = SDL_CreateWindow(
        title.c_str(),
        width,
        height,
        windowFlags
    );
    if (!mSdlWindow)
    {
        spdlog::error("Failed to create SDL window: {}", SDL_GetError());
        throw std::runtime_error("Failed to create SDL window");
    }

    mGlContext = SDL_GL_CreateContext(mSdlWindow);
    SDL_GL_MakeCurrent(mSdlWindow, mGlContext);

    mContentScale = SDL_GetWindowDisplayScale(mSdlWindow);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
    {
        spdlog::error("Failed to initialize GLAD");
        throw std::runtime_error("Failed to initialize GLAD");
    }
}

Sdl3Backend::~Sdl3Backend()
{
    for (auto& [id, gp] : mOpenGamepads)
        SDL_CloseGamepad(gp);
    mOpenGamepads.clear();

    ImGui_ImplSDL3_Shutdown();
    SDL_GL_DestroyContext(mGlContext);
    SDL_DestroyWindow(mSdlWindow);
    SDL_Quit();
}

void Sdl3Backend::initImGuiPlatform()
{
    ImGui_ImplSDL3_InitForOpenGL(mSdlWindow, mGlContext);
}

void Sdl3Backend::beginSession(Controller& controller)
{
    mShouldClose = false;

    // Open any gamepads already connected at startup
    int gamepadCount;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepadCount);
    if (gamepads)
    {
        for (int i = 0; i < gamepadCount; ++i)
        {
            SDL_Gamepad* gp = SDL_OpenGamepad(gamepads[i]);
            if (gp)
            {
                mOpenGamepads[gamepads[i]] = gp;
                controller.gamepadConnected(static_cast<int>(gamepads[i]));
            }
        }
        SDL_free(gamepads);
    }
}

void Sdl3Backend::newImGuiFrame()
{
    ImGui_ImplSDL3_NewFrame();
}

void Sdl3Backend::endFrame(Controller& controller, const ViewportRect& viewport, const ScreenSize& screenSize)
{
#if !defined(NOTHOFAGUS_BACKEND_VULKAN)
    SDL_GL_SwapWindow(mSdlWindow);
#endif

    constexpr float GAMEPAD_AXIS_DEADZONE = 0.1f;

    // Capture framebuffer size once for mouse coordinate mapping
    int framebufferWidth, framebufferHeight;
    SDL_GetWindowSizeInPixels(mSdlWindow, &framebufferWidth, &framebufferHeight);

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            mShouldClose = true;
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (!event.key.repeat)
            {
                Key key = KeyboardImplementation::toKeyCode(static_cast<int>(event.key.scancode));
                DiscreteTrigger trigger = (event.type == SDL_EVENT_KEY_DOWN)
                    ? DiscreteTrigger::Press : DiscreteTrigger::Release;
                if (!ImGui::GetIO().WantCaptureKeyboard)
                    controller.activate({key, trigger});
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                MouseButton button = MouseImplementation::toMouseButton(event.button.button);
                DiscreteTrigger trigger = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                    ? DiscreteTrigger::Press : DiscreteTrigger::Release;
                controller.activateMouseButton({button, trigger});
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
        {
            // SDL3 gives window-space floats (top-left origin). Scale to framebuffer pixels (HiDPI).
            int windowWidth, windowHeight;
            SDL_GetWindowSize(mSdlWindow, &windowWidth, &windowHeight);
            const float scaleX = (windowWidth  > 0) ? static_cast<float>(framebufferWidth)  / windowWidth  : 1.0f;
            const float scaleY = (windowHeight > 0) ? static_cast<float>(framebufferHeight) / windowHeight : 1.0f;

            // Convert to framebuffer coords with bottom-left origin.
            const float fbCursorX = event.motion.x * scaleX;
            const float fbCursorY = static_cast<float>(framebufferHeight) - event.motion.y * scaleY;

            // Map through the letterboxed viewport to game canvas coords.
            const glm::vec2 gamePosition = {
                (fbCursorX - static_cast<float>(viewport.x)) / static_cast<float>(viewport.width)  * static_cast<float>(screenSize.width),
                (fbCursorY - static_cast<float>(viewport.y)) / static_cast<float>(viewport.height) * static_cast<float>(screenSize.height)
            };
            controller.updateMousePosition(gamePosition);
            break;
        }

        case SDL_EVENT_MOUSE_WHEEL:
            if (!ImGui::GetIO().WantCaptureMouse)
                controller.scrolled({event.wheel.x, event.wheel.y});
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
        {
            SDL_JoystickID id = event.gdevice.which;
            SDL_Gamepad* gp = SDL_OpenGamepad(id);
            if (gp)
            {
                mOpenGamepads[id] = gp;
                controller.gamepadConnected(static_cast<int>(id));
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED:
        {
            SDL_JoystickID id = event.gdevice.which;
            auto it = mOpenGamepads.find(id);
            if (it != mOpenGamepads.end())
            {
                SDL_CloseGamepad(it->second);
                mOpenGamepads.erase(it);
            }
            controller.gamepadDisconnected(static_cast<int>(id));
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
            GamepadButton button = sdlButtonToGamepadButton(
                static_cast<SDL_GamepadButton>(event.gbutton.button));
            DiscreteTrigger trigger = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                ? DiscreteTrigger::Press : DiscreteTrigger::Release;
            controller.activateGamepadButton({static_cast<int>(event.gbutton.which), button, trigger});
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            GamepadAxis axis = sdlAxisToGamepadAxis(
                static_cast<SDL_GamepadAxis>(event.gaxis.axis));
            float value = static_cast<float>(event.gaxis.value) / 32767.0f;

            if (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger)
            {
                // SDL3 triggers are [0, 32767], already [0,1] after normalise
                if (value < GAMEPAD_AXIS_DEADZONE) value = 0.0f;
            }
            else
            {
                value = std::max(value, -1.0f);
                if (axis == GamepadAxis::LeftY || axis == GamepadAxis::RightY) value = -value;
                if (std::abs(value) < GAMEPAD_AXIS_DEADZONE) value = 0.0f;
            }
            controller.updateGamepadAxis(static_cast<int>(event.gaxis.which), axis, value);
            break;
        }
        } // switch
    } // SDL_PollEvent loop
}

std::pair<int, int> Sdl3Backend::getFramebufferSize() const
{
    int width, height;
    SDL_GetWindowSizeInPixels(mSdlWindow, &width, &height);
    return {width, height};
}

float Sdl3Backend::getTime() const
{
    return static_cast<float>(SDL_GetTicks()) / 1000.0f;
}

std::size_t Sdl3Backend::getCurrentMonitor() const
{
    SDL_DisplayID currentDisplay = SDL_GetDisplayForWindow(mSdlWindow);

    int count;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    if (!displays)
    {
        spdlog::error("No monitors were found");
        throw std::runtime_error("No monitors found");
    }

    for (int i = 0; i < count; ++i)
    {
        if (displays[i] == currentDisplay)
        {
            SDL_free(displays);
            return static_cast<std::size_t>(i);
        }
    }

    SDL_free(displays);
    spdlog::warn("Top left corner of the window is outside of all monitors. Returning primary monitor.");
    return 0;
}

bool Sdl3Backend::isFullscreen() const
{
    return (SDL_GetWindowFlags(mSdlWindow) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Sdl3Backend::setFullscreenOnMonitor(std::size_t monitorIndex)
{
    int count;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    debugCheck(static_cast<int>(monitorIndex) < count, "Monitor index out of range");
    SDL_DisplayID display = displays[monitorIndex];
    SDL_free(displays);

    SDL_Rect bounds;
    SDL_GetDisplayBounds(display, &bounds);
    SDL_SetWindowPosition(mSdlWindow, bounds.x, bounds.y);
    SDL_SetWindowFullscreen(mSdlWindow, true);
}

AABox Sdl3Backend::getWindowAABox() const
{
    AABox box;
    SDL_GetWindowPosition(mSdlWindow, &box.x, &box.y);
    SDL_GetWindowSize(mSdlWindow, &box.width, &box.height);
    return box;
}

void Sdl3Backend::setWindowed(const AABox& box)
{
    SDL_SetWindowFullscreen(mSdlWindow, false);
    SDL_SetWindowPosition(mSdlWindow, box.x, box.y);
    SDL_SetWindowSize(mSdlWindow, box.width, box.height);
}

ScreenSize Sdl3Backend::getWindowSize() const
{
    debugCheck(mSdlWindow != nullptr, "SDL Window has not been initialized.");
    int width, height;
    SDL_GetWindowSize(mSdlWindow, &width, &height);
    return {static_cast<unsigned int>(width), static_cast<unsigned int>(height)};
}

void Sdl3Backend::requestClose()
{
    mShouldClose = true;
}

void Sdl3Backend::setWindowTitle(const std::string& title)
{
    SDL_SetWindowTitle(mSdlWindow, title.c_str());
}

ScreenSize Sdl3Backend::getPrimaryMonitorSize()
{
    SDL_Init(SDL_INIT_VIDEO);  // idempotent — safe if Canvas has already initialised it
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(primary);
    if (!mode)
    {
        spdlog::warn("getPrimaryMonitorSize: could not query primary monitor, returning default 1920x1080");
        return {1920, 1080};
    }
    return {static_cast<unsigned int>(mode->w), static_cast<unsigned int>(mode->h)};
}

} // namespace Nothofagus
