#include "render_target_container.h"
#include "texture_container.h"
#include "check.h"

namespace Nothofagus
{

void RenderTargetPack::freeGpuResources(ActiveBackend& backend, TexturePack& proxyPack)
{
    if (dRenderTargetOpt.has_value())
    {
        debugCheck(proxyPack.dtextureOpt.has_value(),
            "RenderTargetPack::freeGpuResources: render target initialized but proxy texture is not");
        backend.freeRenderTarget(*dRenderTargetOpt, *proxyPack.dtextureOpt);
        proxyPack.dtextureOpt = std::nullopt;
    }
    clear();
}

void RenderTargetPack::syncToGpu(ActiveBackend& backend, TexturePack& proxyPack)
{
    if (isDirty())
    {
        dRenderTargetOpt = backend.createRenderTarget(renderTarget.mSize);
        proxyPack.dtextureOpt = backend.getRenderTargetTexture(*dRenderTargetOpt);
    }
}

}
