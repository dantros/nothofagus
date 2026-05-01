#pragma once

#include <cstddef>

namespace Nothofagus
{

/// Stable handle to a font baked through Canvas::bakeImguiFont(). The handle
/// stays valid across atlas rebuilds (the rebuild patches the underlying
/// ImFont* in place); only Canvas::removeImguiFont(thisId) invalidates it.
struct ImguiFontId
{
    std::size_t id;

    bool operator==(const ImguiFontId& rhs) const { return id == rhs.id; }
};

}
