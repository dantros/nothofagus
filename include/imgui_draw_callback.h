#pragma once

#include <functional>

namespace Nothofagus
{

/// Callback type for ImGui draws targeted at a render target via Canvas::renderImguiTo.
/// The callback runs on the render target's secondary ImGuiContext.
using ImguiDrawCallback = std::function<void()>;

}
