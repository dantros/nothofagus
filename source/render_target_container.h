#pragma once

#include "render_target.h"
#include "indexed_container.h"
#include "drender_target.h"
#include <optional>

namespace Nothofagus
{

struct RenderTargetPack
{
    RenderTarget renderTarget;
    std::optional<DRenderTarget> dRenderTargetOpt;

    bool isDirty() const { return not dRenderTargetOpt.has_value(); }

    void clear()
    {
        if (dRenderTargetOpt.has_value())
        {
            dRenderTargetOpt.value().clear();
            dRenderTargetOpt = std::nullopt;
        }
    }
};

using RenderTargetContainer = IndexedContainer<RenderTargetPack>;

}
