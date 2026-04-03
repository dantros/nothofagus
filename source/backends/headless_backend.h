#pragma once

#include "canvas.h"
#include "controller.h"
#include "aa_box.h"
#include <string>
#include <utility>
#include <chrono>

namespace Nothofagus
{

/// Minimal no-op window backend for headless Vulkan rendering.
/// Satisfies the WindowBackend concept without creating any window or
/// requiring a display server.  Used when NOTHOFAGUS_HEADLESS_VULKAN is set.
class HeadlessBackend
{
public:
    HeadlessBackend(const std::string& title, int width, int height, bool visible = true);

    void initImGuiPlatform();

    float contentScale() const { return 1.0f; }
    void* nativeHandle() const { return nullptr; }

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

    void setWindowTitle(const std::string& title);

    static ScreenSize getPrimaryMonitorSize();

private:
    int mWidth;
    int mHeight;
    std::chrono::steady_clock::time_point mStartTime;
    bool mRunning = true;
};

} // namespace Nothofagus
