#pragma once

#include "render_target.h"
#include "indexed_container.h"
#include "dframebuffer.h"
#include <optional>

namespace Nothofagus
{

struct RenderTargetPack
{
    RenderTarget renderTarget;
    std::optional<DFramebuffer> dframebufferOpt;

    bool isDirty() const { return not dframebufferOpt.has_value(); }

    void clear()
    {
        if (dframebufferOpt.has_value())
        {
            dframebufferOpt.value().clear();
            dframebufferOpt = std::nullopt;
        }
    }
};

using RenderTargetContainer = IndexedContainer<RenderTargetPack>;

}
