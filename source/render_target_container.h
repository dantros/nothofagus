#pragma once

#include "render_target.h"
#include "indexed_container.h"
#include "drender_target.h"
#include "backends/render_backend_select.h"
#include <optional>

namespace Nothofagus
{

struct TexturePack;   // freeGpuResources needs to null the proxy pack's dtextureOpt

struct RenderTargetPack
{
    RenderTarget renderTarget;
    std::optional<DRenderTarget> dRenderTargetOpt;

    bool isDirty() const { return not dRenderTargetOpt.has_value(); }

    /// Reset the GPU-side optional to nullopt. Does NOT touch the backend or
    /// the proxy texture pack. Use `freeGpuResources` for full teardown.
    void clear() { dRenderTargetOpt = std::nullopt; }

    /// Free the backend render-target handle (FBO + color attachment) and
    /// also null the proxy texture's `dtextureOpt` (which the backend frees
    /// as part of `freeRenderTarget`). Resets this pack's optional.
    void freeGpuResources(ActiveBackend& backend, TexturePack& proxyPack);

    /// Per-frame GPU sync: lazy-create the render target on first use and
    /// stamp its color attachment as the proxy texture's `dtextureOpt` so
    /// bellotas that sample the proxy pick up a live handle.
    void syncToGpu(ActiveBackend& backend, TexturePack& proxyPack);
};

using RenderTargetContainer = IndexedContainer<RenderTargetPack>;

}
