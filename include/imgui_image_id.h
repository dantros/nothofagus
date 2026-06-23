#pragma once

#include <cstddef>

namespace Nothofagus
{

/// Stable handle to an image registered through Canvas::registerImguiImage(). The
/// handle, the internal render target backing it, and its ImGui-bindable
/// `ImTextureID` all persist until Canvas::unregisterImguiImage(thisId) — so a
/// registered image is warm-up-free once its first render tick has run, and never
/// churns on size changes or per-frame garbage collection.
struct ImguiImageId
{
    std::size_t id;

    bool operator==(const ImguiImageId& rhs) const { return id == rhs.id; }
};

}
