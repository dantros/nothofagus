#include "headless_backend.h"
#include <imgui.h>

namespace Nothofagus
{

HeadlessBackend::HeadlessBackend(const std::string& /*title*/, int width, int height, bool /*visible*/)
    : mWidth(width)
    , mHeight(height)
    , mStartTime(std::chrono::steady_clock::now())
{
}

void HeadlessBackend::initImGuiPlatform()
{
    // No platform backend — just configure ImGui display size so NewFrame() works.
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(mWidth), static_cast<float>(mHeight));
}

void HeadlessBackend::beginSession(Controller& /*controller*/)
{
}

bool HeadlessBackend::isRunning() const
{
    return mRunning;
}

void HeadlessBackend::newImGuiFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(mWidth), static_cast<float>(mHeight));
    io.DeltaTime   = 1.0f / 60.0f;
}

void HeadlessBackend::endFrame(Controller& /*controller*/, const ViewportRect& /*viewport*/, const ScreenSize& /*screenSize*/)
{
    // No buffer swap or event polling in headless mode.
}

std::pair<int, int> HeadlessBackend::getFramebufferSize() const
{
    return {mWidth, mHeight};
}

float HeadlessBackend::getTime() const
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - mStartTime).count();
}

std::size_t HeadlessBackend::getCurrentMonitor() const
{
    return 0;
}

bool HeadlessBackend::isFullscreen() const
{
    return false;
}

void HeadlessBackend::setFullscreenOnMonitor(std::size_t /*monitorIndex*/)
{
}

AABox HeadlessBackend::getWindowAABox() const
{
    return {0, 0, mWidth, mHeight};
}

void HeadlessBackend::setWindowed(const AABox& /*box*/)
{
}

ScreenSize HeadlessBackend::getWindowSize() const
{
    return {static_cast<unsigned int>(mWidth), static_cast<unsigned int>(mHeight)};
}

void HeadlessBackend::requestClose()
{
    mRunning = false;
}

void HeadlessBackend::setWindowTitle(const std::string& /*title*/)
{
}

ScreenSize HeadlessBackend::getPrimaryMonitorSize()
{
    return {1920u, 1080u};
}

} // namespace Nothofagus
