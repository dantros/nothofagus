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

namespace
{
    // Overload-set helper for std::visit: one operator() per variant alternative, so a
    // missing case is a compile error (compile-time exhaustiveness over the variant).
    template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    // AABB of a mesh's vertex positions (pixel units); {0,0}-{1,1} for an empty mesh.
    void meshAabb(const Mesh& mesh, glm::vec2& outMin, glm::vec2& outExtent)
    {
        glm::vec2 lo{ std::numeric_limits<float>::max()};
        glm::vec2 hi{-std::numeric_limits<float>::max()};
        for (const Vertex& vertex : mesh.vertices)
        {
            lo = glm::min(lo, vertex.position);
            hi = glm::max(hi, vertex.position);
        }
        if (mesh.vertices.empty()) { lo = glm::vec2(0.0f); hi = glm::vec2(1.0f); }
        outMin = lo;
        outExtent = glm::max(hi - lo, glm::vec2(1e-3f));
    }

    glm::ivec2 toPhysical(glm::vec2 sizePx)
    {
        return glm::ivec2{
            std::max(1, static_cast<int>(std::ceil(sizePx.x))),
            std::max(1, static_cast<int>(std::ceil(sizePx.y))),
        };
    }

    // Resolved geometry for a drawn image, independent of texture/handle bookkeeping.
    struct ImageLayout
    {
        glm::ivec2 rttPhys{1, 1};       ///< physical px the off-screen target is rasterized at.
        glm::vec2  naturalPhys{1.0f};   ///< the visual's natural extent in those same RTT px.
        glm::vec2  displaySize{1.0f};   ///< logical points handed to ImGui (Image / layout size).
        bool       fitCentered = false; ///< uniform-fit + center (else fill the box).
    };

    // Logical modes (LogicalPixels / Scaled / Custom) size in logical px and rasterize at
    // size × contentScale, so the image is DPI-scaled like the rest of the UI; ImGui then
    // re-applies DPI to displaySize, giving a 1:1 RTT->screen mapping. DevicePixels sizes in
    // physical px directly (1 unit = 1 display pixel): the RTT is rasterized at that exact
    // size and displaySize divides DPI back out, bypassing OS content scaling.
    ImageLayout resolveLayout(const ImguiImageSize::Spec& sizeSpec, glm::vec2 naturalLogical, float scale)
    {
        const auto logical = [&](glm::vec2 targetLogical, bool fit) {
            targetLogical = glm::max(targetLogical, glm::vec2(1.0f));
            return ImageLayout{toPhysical(targetLogical * scale), naturalLogical * scale, targetLogical, fit};
        };
        return std::visit(overloaded{
            [&](const ImguiImageSize::LogicalPixels&) { return logical(naturalLogical, false); },
            [&](const ImguiImageSize::Scaled& s)      { return logical(naturalLogical * s.factor, false); },
            [&](const ImguiImageSize::Custom& c)      { return logical(c.size, c.fit == ImguiImageFit::Fit); },
            [&](const ImguiImageSize::DevicePixels& d) {
                const glm::vec2 targetDevice = glm::max(d.size, glm::vec2(1.0f));
                // naturalPhys is the natural extent in device px (1 texel -> 1 device px);
                // displaySize divides DPI out so ImGui's re-multiply lands on targetDevice.
                return ImageLayout{toPhysical(targetDevice), naturalLogical,
                                   targetDevice / scale, d.fit == ImguiImageFit::Fit};
            },
        }, sizeSpec);
    }
}

void ImguiImageManager::imguiVisual(const Visual& visual, const ImguiImageSize::Spec& sizeSpec, float contentScale)
{
    const TextureId texId = visual.texture();
    debugCheck(mAssets.textures().contains(texId.id), "imguiVisual: unknown TextureId");

    // DPI density to rasterize the off-screen target at (the same value the font atlas
    // uses). DevicePixels deliberately bypasses it; see resolveLayout.
    const float scale = std::max(contentScale, 1e-3f);

    // Natural size = the visual's real on-screen footprint (mesh AABB extent, logical px).
    // For an invisible visual, reserve the layout without touching GPU/mesh resources:
    // derive the size from the mesh (if any) or the texture, then bail.
    if (not visual.visible())
    {
        glm::vec2 natural = visual.meshId().has_value()
            ? [&]{ glm::vec2 mn, ext; meshAabb(mAssets.mesh(visual.meshId().value()), mn, ext); return ext; }()
            : glm::max(glm::vec2(mAssets.textures().at(texId.id).mTextureSize), glm::vec2(1.0f));
        const glm::vec2 display = resolveLayout(sizeSpec, natural, scale).displaySize;
        ImGui::Dummy(ImVec2(display.x, display.y));
        return;
    }

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

    glm::vec2 aabbMin, naturalLogical;
    meshAabb(mAssets.mesh(meshId), aabbMin, naturalLogical);

    // Resolve the rasterization size, the natural extent in those RTT px, the ImGui layout
    // size, and the fit mode — all per the chosen size spec (logical vs device pixels).
    const ImageLayout layout = resolveLayout(sizeSpec, naturalLogical, scale);
    const glm::ivec2 rttPhys = layout.rttPhys;
    const glm::vec2 naturalPhys = layout.naturalPhys;
    const bool fitCentered = layout.fitCentered;

    // Content placement within the target (physical px): fill, or — for Fit —
    // uniform-scaled and centered with transparent margins.
    glm::vec2 contentPhys(rttPhys);
    glm::vec2 offset(0.0f);
    if (fitCentered)
    {
        const float f = std::min(static_cast<float>(rttPhys.x) / naturalPhys.x,
                                 static_cast<float>(rttPhys.y) / naturalPhys.y);
        contentPhys = naturalPhys * f;
        offset = (glm::vec2(rttPhys) - contentPhys) * 0.5f;
    }

    // T maps the mesh AABB onto [offset, offset+contentPhys] in the RTT's pixel space:
    //   q = (contentPhys / naturalLogical) * (p - aabbMin) + offset
    // (column-major mat3; render side then applies rttNdc(rttPhys)).
    const glm::vec2 sv = contentPhys / naturalLogical;
    glm::mat3 transform(1.0f);
    transform[0][0] = sv.x;
    transform[1][1] = sv.y;
    transform[2][0] = offset.x - sv.x * aabbMin.x;
    transform[2][1] = offset.y - sv.y * aabbMin.y;

    const std::size_t layer = visual.currentLayer();
    const Key key{texId.id, meshId.id, layer, rttPhys.x, rttPhys.y, fitCentered ? 1 : 0};

    auto it = mEntries.find(key);
    if (it == mEntries.end())
    {
        const RenderTargetId rt = mAssets.addRenderTarget(
            ScreenSize{static_cast<unsigned int>(rttPhys.x), static_cast<unsigned int>(rttPhys.y)});
        mAssets.setRenderTargetClearColor(rt, glm::vec4(0.0f)); // transparent

        mTexturePins.retain(texId,  [&](TextureId id) { mAssets.retainTexture(id); });
        mMeshPins.retain(meshId,    [&](MeshId id)    { mAssets.retainMesh(id); });

        Entry entry;
        entry.rt        = rt;
        entry.texture   = texId;
        entry.mesh      = meshId;
        entry.layer     = static_cast<int>(layer);
        entry.rttSize   = rttPhys;
        entry.transform = transform;
        entry.ownedQuad = ownedQuad;
        it = mEntries.emplace(key, entry).first;
    }

    Entry& entry = it->second;
    entry.lastUsedFrame = mFrameCounter;

    const ImVec2 displaySize(layout.displaySize.x, layout.displaySize.y);

    // Pick the handle to draw: this entry's own once it is ready, otherwise the best
    // ready handle for the SAME sprite (same texture/mesh) — see findReadyFallback for
    // the preference order. Without this, the per-frame warm-up flashes an empty cell on
    // every animation step (layer changes) and on every size-slider tick (size changes),
    // since each is a fresh Key. Only a genuinely first-seen sprite (nothing ready yet)
    // still reserves a blank cell.
    std::uint64_t drawHandle = entry.handle;
    if (drawHandle == 0)
    {
        if (Entry* fallback = findReadyFallback(key, entry))
        {
            // Keep the borrowed entry alive and re-rendered this frame so its handle
            // stays valid through the main ImGui pass (GC runs before it).
            fallback->lastUsedFrame = mFrameCounter;
            drawHandle = fallback->handle;
        }
    }

    if (drawHandle != 0)
    {
        // ImGui's image sampler is global-per-draw and defaults to LINEAR (it does not
        // read the texture's own filter), so an upscaled image blurs. Honor the visual's
        // texture magFilter via ImGui 1.92's standard sampler draw-callbacks (same path on
        // OpenGL + Vulkan). Default Nearest -> crisp pixel art; restore Linear afterward so
        // the window's text/widgets are unaffected. (A fallback handle is the same texture,
        // so its magFilter matches.)
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
        ImGui::Image(static_cast<ImTextureID>(drawHandle), displaySize,
                     ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        if (fade)
            ImGui::PopStyleVar();

        if (nearest && platformIo.DrawCallback_SetSamplerLinear)
            drawList->AddCallback(platformIo.DrawCallback_SetSamplerLinear, nullptr);
    }
    else
    {
        // First-ever frame for this visual: nothing ready to show yet; reserve layout.
        ImGui::Dummy(displaySize);
    }
}

ImguiImageManager::Entry* ImguiImageManager::findReadyFallback(const Key& key, const Entry& want)
{
    // Any ready handle for the same sprite (same texture + mesh) beats a blank cell
    // during warm-up. Prefer, in order: the same animation layer (correct frame), then
    // the same rasterized size + fit (crisp, no stretch), then the most recently
    // rendered. This bridges two warm-up cases: an animation revisiting a retired layer
    // (same size, different layer), and a size-slider drag churning a new RTT every
    // frame (same layer, different size) — the differently-sized handle is just drawn
    // stretched to the target for the one bridge frame. nullptr if nothing is ready.
    const int wantFit = std::get<5>(key);
    Entry* best = nullptr;
    int bestScore = -1;
    for (auto& [candidateKey, candidate] : mEntries)
    {
        if (candidate.handle == 0)                   continue; // not yet ready
        if (candidate.texture.id != want.texture.id) continue;
        if (candidate.mesh.id    != want.mesh.id)    continue;

        int score = 0;
        if (candidate.layer   == want.layer)      score += 4; // correct frame content
        if (candidate.rttSize == want.rttSize)    score += 2; // crisp, no stretch
        if (std::get<5>(candidateKey) == wantFit) score += 1; // same fill/fit placement

        if (score > bestScore ||
            (score == bestScore && best != nullptr && candidate.lastUsedFrame > best->lastUsedFrame))
        {
            best = &candidate;
            bestScore = score;
        }
    }
    return best;
}

void ImguiImageManager::appendInternalPasses(std::vector<RttPass>& out)
{
    for (auto& [key, entry] : mEntries)
    {
        if (entry.lastUsedFrame != mFrameCounter)
            continue;

        // entry.transform maps the mesh AABB into the RTT's pixel space (fill or fit-
        // centered); the render side then applies rttNdc(rttSize) over it.
        DrawItem item{};
        item.bellotaTransform = entry.transform;
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
    mTexturePins.release(entry.texture, [&](TextureId id) { mAssets.releaseTexture(id); });

    mMeshPins.release(entry.mesh, [&](MeshId id)
    {
        mAssets.releaseMesh(id);
        // If this was a shared owned-quad, drop it from the cache so it can be GC'd.
        if (entry.ownedQuad)
            for (auto cacheIt = mOwnedQuads.begin(); cacheIt != mOwnedQuads.end(); ++cacheIt)
                if (cacheIt->second.id == id.id) { mOwnedQuads.erase(cacheIt); break; }
    });
}

void ImguiImageManager::releaseAll()
{
    for (auto& [key, entry] : mEntries)
    {
        freeEntryGpu(entry);
        unpinEntry(entry);
    }
    mEntries.clear();
    mTexturePins.clear();
    mMeshPins.clear();
    mOwnedQuads.clear();
}

} // namespace Nothofagus
