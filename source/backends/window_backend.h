#pragma once

#include "canvas.h"
#include "controller.h"
#include "aa_box.h"
#include <concepts>
#include <utility>
#include <cstddef>

namespace Nothofagus
{

/**
 * @concept WindowBackend
 * @brief C++20 concept constraining window + input backend types.
 *
 * Any type satisfying this concept can be used as the compile-time backend
 * for Canvas. Backends are selected via the NOTHOFAGUS_WINDOW_BACKEND CMake
 * option (GLFW or SDL3), which sets/unsets the NOTHOFAGUS_BACKEND_SDL3 define.
 */
template<typename T>
concept WindowBackend = requires(
    T& backend,
    Controller& controller,
    const ViewportRect& viewport,
    const ScreenSize& screenSize,
    std::size_t monitorIndex,
    const AABox& windowedBox,
    const std::string& title)
{
    // Session lifecycle
    { backend.beginSession(controller) } -> std::same_as<void>;
    { backend.isRunning()              } -> std::same_as<bool>;

    // Per-frame
    { backend.newImGuiFrame()                               } -> std::same_as<void>;
    { backend.endFrame(controller, viewport, screenSize)    } -> std::same_as<void>;
    { backend.getFramebufferSize()                          } -> std::same_as<std::pair<int, int>>;
    { backend.getTime()                                     } -> std::convertible_to<float>;

    // ImGui platform init (called once; fonts and renderer init are handled separately)
    { backend.initImGuiPlatform() } -> std::same_as<void>;
    { backend.contentScale()      } -> std::convertible_to<float>;
    // Returns the OS-level window handle (GLFWwindow* or SDL_Window*) as void*.
    { backend.nativeHandle()      } -> std::same_as<void*>;

    // Window management
    { backend.getCurrentMonitor()              } -> std::same_as<std::size_t>;
    { backend.isFullscreen()                   } -> std::same_as<bool>;
    { backend.setFullscreenOnMonitor(monitorIndex) } -> std::same_as<void>;
    { backend.getWindowAABox()                 } -> std::same_as<AABox>;
    { backend.setWindowed(windowedBox)         } -> std::same_as<void>;
    { backend.getWindowSize()                  } -> std::same_as<ScreenSize>;
    { backend.requestClose()                   } -> std::same_as<void>;
    { backend.setWindowTitle(title)            } -> std::same_as<void>;
};

} // namespace Nothofagus

// Include and alias the selected backend
#ifdef NOTHOFAGUS_BACKEND_SDL3
    #include "sdl3_backend.h"
    namespace Nothofagus { using SelectedWindowBackend = Sdl3Backend; }
#else
    #include "glfw_backend.h"
    namespace Nothofagus { using SelectedWindowBackend = GlfwBackend; }
#endif

static_assert(
    Nothofagus::WindowBackend<Nothofagus::SelectedWindowBackend>,
    "Selected window backend does not satisfy the WindowBackend concept");
