#include "imgui_image_manager.h"

#include "asset_registry.h"
#include "bellota_to_mesh.h"   // generateQuadMesh
#include "check.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Nothofagus
{

MeshId ImguiImageManager::ownedQuadFor(glm::ivec2 textureSize)
{
    const std::pair<int, int> sizeKey{textureSize.x, textureSize.y};
    auto it = mOwnedQuads.find(sizeKey);
    if (it != mOwnedQuads.end())
        return it->second;

    const MeshId meshId = mAssets.addMesh(generateQuadMesh(textureSize));
    mOwnedQuads.emplace(sizeKey, meshId);
    return meshId;
}

void ImguiImageManager::imguiVisual(const Visual& visual, glm::vec2 sizePx)
{
    const ImVec2 displaySize(sizePx.x, sizePx.y);

    if (not visual.visible())
    {
        ImGui::Dummy(displaySize);
        return;
    }

    const TextureId texId = visual.texture();
    debugCheck(mAssets.textures().contains(texId.id), "imguiVisual: unknown TextureId");

    // Resolve the mesh: the visual's own mesh (auto-quad or custom) when present,
    // else a manager-owned quad sized to the texture (a standalone Visual{texId}).
    MeshId meshId{0};
    bool ownedQuad = false;
    if (visual.meshId().has_value())
    {
        meshId = visual.meshId().value();
    }
    else
    {
        meshId = ownedQuadFor(mAssets.textures().at(texId.id).mTextureSize);
        ownedQuad = true;
    }

    const std::size_t layer = visual.currentLayer();
    const Key key{texId.id, meshId.id, layer};

    auto it = mEntries.find(key);
    if (it == mEntries.end())
    {
        // Size the internal RTT to the mesh's AABB (auto-quad → texture size; custom
        // mesh → its full extent, no clipping). Vertices are in pixel units.
        const Mesh& mesh = mAssets.mesh(meshId);
        glm::vec2 aabbMin{ std::numeric_limits<float>::max()};
        glm::vec2 aabbMax{-std::numeric_limits<float>::max()};
        for (const Vertex& vertex : mesh.vertices)
        {
            aabbMin = glm::min(aabbMin, vertex.position);
            aabbMax = glm::max(aabbMax, vertex.position);
        }
        if (mesh.vertices.empty())
        {
            aabbMin = glm::vec2(0.0f);
            aabbMax = glm::vec2(1.0f);
        }
        const glm::ivec2 rttSize{
            std::max(1, static_cast<int>(std::ceil(aabbMax.x - aabbMin.x))),
            std::max(1, static_cast<int>(std::ceil(aabbMax.y - aabbMin.y)))
        };

        const RenderTargetId rt = mAssets.addRenderTarget(
            ScreenSize{static_cast<unsigned int>(rttSize.x), static_cast<unsigned int>(rttSize.y)});
        mAssets.setRenderTargetClearColor(rt, glm::vec4(0.0f)); // transparent

        if (mTextureRefs[texId.id]++ == 0)  mAssets.retainTexture(texId);
        if (mMeshRefs[meshId.id]++  == 0)   mAssets.retainMesh(meshId);

        Entry entry;
        entry.rt        = rt;
        entry.texture   = texId;
        entry.mesh      = meshId;
        entry.layer     = static_cast<int>(layer);
        entry.rttSize   = rttSize;
        entry.aabbMin   = aabbMin;
        entry.ownedQuad = ownedQuad;
        it = mEntries.emplace(key, entry).first;
    }

    Entry& entry = it->second;
    entry.lastUsedFrame = mFrameCounter;

    if (entry.handle != 0)
    {
        // ImGui's image sampler is global-per-draw and defaults to LINEAR (it does not
        // read the texture's own filter), so an upscaled image blurs. Honor the visual's
        // texture magFilter via ImGui 1.92's standard sampler draw-callbacks (same path on
        // OpenGL + Vulkan). Default Nearest -> crisp pixel art; restore Linear afterward so
        // the window's text/widgets are unaffected.
        const bool nearest =
            mAssets.textures().at(entry.texture.id).magFilter == TextureSampleMode::Nearest;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
        if (nearest && platformIo.DrawCallback_SetSamplerNearest)
            drawList->AddCallback(platformIo.DrawCallback_SetSamplerNearest, nullptr);

        // Opacity from the Visual modulates the drawn image via the widget alpha, so the
        // RTT pixels themselves stay opacity-independent (one RTT serves any opacity).
        const bool fade = visual.opacity() < 0.999f;
        if (fade)
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * visual.opacity());
        ImGui::Image(static_cast<ImTextureID>(entry.handle), displaySize,
                     ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        if (fade)
            ImGui::PopStyleVar();

        if (nearest && platformIo.DrawCallback_SetSamplerLinear)
            drawList->AddCallback(platformIo.DrawCallback_SetSamplerLinear, nullptr);
    }
    else
    {
        // Warm-up: the handle is created render-side this frame; draw next frame.
        ImGui::Dummy(displaySize);
    }
}

void ImguiImageManager::appendInternalPasses(std::vector<RttPass>& out)
{
    for (auto& [key, entry] : mEntries)
    {
        if (entry.lastUsedFrame != mFrameCounter)
            continue;

        // Fit transform: place the mesh's AABB at the RTT origin. The render side then
        // applies rttNdc(rttSize), filling the target exactly.
        glm::mat3 fit(1.0f);
        fit[2][0] = -entry.aabbMin.x;
        fit[2][1] = -entry.aabbMin.y;

        DrawItem item{};
        item.bellotaTransform = fit;
        item.texture          = entry.texture;
        item.mesh             = entry.mesh;
        item.layer            = entry.layer;
        item.tintColor        = glm::vec3(1.0f);
        item.tintIntensity    = 0.0f;
        item.opacity          = 1.0f;          // opacity applied at ImGui::Image draw
        item.depthOffset      = 0;

        RttPass pass;
        pass.target = entry.rt;
        pass.draws.push_back(item);
        out.push_back(std::move(pass));
    }
}

void ImguiImageManager::resolveAndGarbageCollect()
{
    RenderTargetContainer& renderTargets = mAssets.renderTargets();

    // 1. For visuals drawn this frame, refresh the flat-2D and (lazily) create the
    //    ImGui handle. Acquire before resolve so the creation frame already has pixels.
    for (auto& [key, entry] : mEntries)
    {
        if (entry.lastUsedFrame != mFrameCounter)
            continue;
        if (not renderTargets.contains(entry.rt.id))
            continue;
        RenderTargetPack& pack = renderTargets.at(entry.rt.id);
        if (not pack.dRenderTargetOpt.has_value())
            continue;

        const DRenderTarget& dRenderTarget = pack.dRenderTargetOpt.value();
        if (entry.handle == 0)
            entry.handle = mBackend.acquireFlat2DImguiHandle(dRenderTarget);
        mBackend.resolveRenderTargetFlat2D(dRenderTarget);
    }

    // 2. Retire entries unused for a while (frees the internal RTT + ImGui handle).
    std::vector<Key> toRetire;
    for (auto& [key, entry] : mEntries)
        if (mFrameCounter - entry.lastUsedFrame > kRetireAfterFrames)
            toRetire.push_back(key);

    for (const Key& key : toRetire)
    {
        Entry& entry = mEntries.at(key);
        freeEntryGpu(entry);
        unpinEntry(entry);
        mEntries.erase(key);
    }
}

void ImguiImageManager::freeEntryGpu(Entry& entry)
{
    RenderTargetContainer& renderTargets = mAssets.renderTargets();
    if (entry.handle != 0 && renderTargets.contains(entry.rt.id))
    {
        RenderTargetPack& pack = renderTargets.at(entry.rt.id);
        if (pack.dRenderTargetOpt.has_value())
            mBackend.releaseFlat2DImguiHandle(pack.dRenderTargetOpt.value(), entry.handle);
    }
    entry.handle = 0;
    mAssets.removeRenderTarget(entry.rt);
}

void ImguiImageManager::unpinEntry(const Entry& entry)
{
    auto texIt = mTextureRefs.find(entry.texture.id);
    if (texIt != mTextureRefs.end() && --texIt->second == 0)
    {
        mAssets.releaseTexture(entry.texture);
        mTextureRefs.erase(texIt);
    }

    auto meshIt = mMeshRefs.find(entry.mesh.id);
    if (meshIt != mMeshRefs.end() && --meshIt->second == 0)
    {
        mAssets.releaseMesh(entry.mesh);
        mMeshRefs.erase(meshIt);
        // If this was a shared owned-quad, drop it from the cache so it can be GC'd.
        if (entry.ownedQuad)
            for (auto cacheIt = mOwnedQuads.begin(); cacheIt != mOwnedQuads.end(); ++cacheIt)
                if (cacheIt->second.id == entry.mesh.id) { mOwnedQuads.erase(cacheIt); break; }
    }
}

void ImguiImageManager::releaseAll()
{
    for (auto& [key, entry] : mEntries)
    {
        freeEntryGpu(entry);
        unpinEntry(entry);
    }
    mEntries.clear();
    mTextureRefs.clear();
    mMeshRefs.clear();
    mOwnedQuads.clear();
}

} // namespace Nothofagus
