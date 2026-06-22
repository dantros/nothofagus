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

    // Resolve the two orthogonal axes (size source + units) into rasterization geometry.
    // The size source picks the target extent (in its own units) and the fit; the units pick
    // how that maps to the RTT and ImGui:
    //  - Logical: rasterize at target × contentScale (DPI-scaled, crisp like the UI); ImGui
    //    re-applies DPI to displaySize == target, giving a 1:1 RTT->screen mapping.
    //  - Device: rasterize at the target px directly (1 texel -> 1 display pixel); displaySize
    //    divides DPI back out so ImGui's re-multiply lands on the device size, bypassing DPI.
    ImageLayout resolveLayout(const ImguiImageSize::Spec& sizeSpec, glm::vec2 naturalLogical, float scale)
    {
        // (target extent in its own units, fit-centered, units) per size source.
        struct Resolved { glm::vec2 target; bool fitCentered; ImguiImageUnits units; };
        const Resolved r = std::visit(overloaded{
            [&](const ImguiImageSize::Natural& n)  { return Resolved{naturalLogical, false, n.units}; },
            [&](const ImguiImageSize::Scaled& s)   { return Resolved{naturalLogical * s.factor, false, s.units}; },
            [&](const ImguiImageSize::Explicit& e) { return Resolved{e.size, e.fit == ImguiImageFit::Fit, e.units}; },
        }, sizeSpec);

        const glm::vec2 target = glm::max(r.target, glm::vec2(1.0f));
        if (r.units == ImguiImageUnits::Device)
            return ImageLayout{toPhysical(target), naturalLogical, target / scale, r.fitCentered};
        return ImageLayout{toPhysical(target * scale), naturalLogical * scale, target, r.fitCentered};
    }
}

ImguiImageManager::ResolvedGeom ImguiImageManager::resolveGeom(
    const Visual& visual, const ImguiImageSize::Spec& sizeSpec, float scale)
{
    ResolvedGeom geom;
    geom.texId = visual.texture();

    // Resolve the mesh: the visual's own mesh (auto-quad or custom) when present,
    // else a manager-owned quad sized to the texture (a standalone Visual{texId}).
    if (visual.meshId().has_value())
    {
        geom.meshId = visual.meshId().value();
    }
    else
    {
        geom.meshId = ownedQuadFor(mAssets.textures().at(geom.texId.id).mTextureSize);
        geom.ownedQuad = true;
    }

    glm::vec2 aabbMin, naturalLogical;
    meshAabb(mAssets.mesh(geom.meshId), aabbMin, naturalLogical);

    // Resolve the rasterization size, the natural extent in those RTT px, the ImGui layout
    // size, and the fit mode — all per the chosen size spec (logical vs device pixels).
    const ImageLayout layout = resolveLayout(sizeSpec, naturalLogical, scale);
    geom.rttPhys     = layout.rttPhys;
    geom.fitCentered = layout.fitCentered;
    geom.displaySize = layout.displaySize;
    geom.layer       = static_cast<int>(visual.currentLayer());

    // Content placement within the target (physical px): fill, or — for Fit —
    // uniform-scaled and centered with transparent margins.
    glm::vec2 contentPhys(geom.rttPhys);
    glm::vec2 offset(0.0f);
    if (geom.fitCentered)
    {
        const float f = std::min(static_cast<float>(geom.rttPhys.x) / layout.naturalPhys.x,
                                 static_cast<float>(geom.rttPhys.y) / layout.naturalPhys.y);
        contentPhys = layout.naturalPhys * f;
        offset = (glm::vec2(geom.rttPhys) - contentPhys) * 0.5f;
    }

    // T maps the mesh AABB onto [offset, offset+contentPhys] in the RTT's pixel space:
    //   q = (contentPhys / naturalLogical) * (p - aabbMin) + offset
    // (column-major mat3; render side then applies rttNdc(rttPhys)).
    const glm::vec2 sv = contentPhys / naturalLogical;
    geom.transform = glm::mat3(1.0f);
    geom.transform[0][0] = sv.x;
    geom.transform[1][1] = sv.y;
    geom.transform[2][0] = offset.x - sv.x * aabbMin.x;
    geom.transform[2][1] = offset.y - sv.y * aabbMin.y;
    return geom;
}

void ImguiImageManager::allocateEntry(Entry& entry, const ResolvedGeom& geom)
{
    const RenderTargetId rt = mAssets.addRenderTarget(
        ScreenSize{static_cast<unsigned int>(geom.rttPhys.x), static_cast<unsigned int>(geom.rttPhys.y)});
    mAssets.setRenderTargetClearColor(rt, glm::vec4(0.0f)); // transparent

    mTexturePins.retain(geom.texId, [&](TextureId id) { mAssets.retainTexture(id); });
    mMeshPins.retain(geom.meshId,   [&](MeshId id)    { mAssets.retainMesh(id); });

    entry.rt        = rt;
    entry.texture   = geom.texId;
    entry.mesh      = geom.meshId;
    entry.layer     = geom.layer;
    entry.rttSize   = geom.rttPhys;
    entry.transform = geom.transform;
    entry.ownedQuad = geom.ownedQuad;
    entry.displaySize = geom.displaySize;
}

void ImguiImageManager::drawResolvedImage(
    std::uint64_t handle, glm::vec2 displaySize, TextureId texForFilter, float opacity)
{
    // ImGui's image sampler is global-per-draw and defaults to LINEAR (it does not
    // read the texture's own filter), so an upscaled image blurs. Honor the visual's
    // texture magFilter via ImGui 1.92's standard sampler draw-callbacks (same path on
    // OpenGL + Vulkan). Default Nearest -> crisp pixel art; restore Linear afterward so
    // the window's text/widgets are unaffected.
    const bool nearest =
        mAssets.textures().at(texForFilter.id).magFilter == TextureSampleMode::Nearest;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
    if (nearest && platformIo.DrawCallback_SetSamplerNearest)
        drawList->AddCallback(platformIo.DrawCallback_SetSamplerNearest, nullptr);

    // Opacity modulates the drawn image via the widget alpha, so the RTT pixels themselves
    // stay opacity-independent (one RTT serves any opacity).
    const bool fade = opacity < 0.999f;
    if (fade)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * opacity);
    ImGui::Image(static_cast<ImTextureID>(handle), ImVec2(displaySize.x, displaySize.y),
                 ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
    if (fade)
        ImGui::PopStyleVar();

    if (nearest && platformIo.DrawCallback_SetSamplerLinear)
        drawList->AddCallback(platformIo.DrawCallback_SetSamplerLinear, nullptr);
}

void ImguiImageManager::imguiVisual(const Visual& visual, const ImguiImageSize::Spec& sizeSpec, float contentScale)
{
    const TextureId texId = visual.texture();
    debugCheck(mAssets.textures().contains(texId.id), "imguiVisual: unknown TextureId");

    // DPI density to rasterize the off-screen target at (the same value the font atlas
    // uses). Device units deliberately bypass it; see resolveLayout.
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

    const ResolvedGeom geom = resolveGeom(visual, sizeSpec, scale);
    const Key key{geom.texId.id, geom.meshId.id, static_cast<std::size_t>(geom.layer),
                  geom.rttPhys.x, geom.rttPhys.y, geom.fitCentered ? 1 : 0};

    auto it = mEntries.find(key);
    if (it == mEntries.end())
    {
        Entry entry;
        allocateEntry(entry, geom);
        it = mEntries.emplace(key, entry).first;
    }

    Entry& entry = it->second;
    entry.lastUsedFrame = mFrameCounter;

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
        drawResolvedImage(drawHandle, geom.displaySize, geom.texId, visual.opacity());
    else
        ImGui::Dummy(ImVec2(geom.displaySize.x, geom.displaySize.y)); // first-ever frame: reserve layout
}

// --- Registration ----------------------------------------------------------------------

ImguiImageId ImguiImageManager::registerImage(
    const Visual& visual, const ImguiImageSize::Spec& sizeSpec, float contentScale)
{
    const TextureId texId = visual.texture();
    debugCheck(mAssets.textures().contains(texId.id), "registerImguiImage: unknown TextureId");

    const float scale = std::max(contentScale, 1e-3f);
    const ResolvedGeom geom = resolveGeom(visual, sizeSpec, scale);

    Entry entry;
    allocateEntry(entry, geom);
    entry.opacity = visual.opacity();
    entry.visible = visual.visible();
    entry.spec    = sizeSpec;

    const std::size_t id = mNextImageId++;
    mRegistered.emplace(id, entry);
    return ImguiImageId{id};
}

void ImguiImageManager::updateImage(ImguiImageId id, const Visual& visual, float contentScale)
{
    auto it = mRegistered.find(id.id);
    if (it == mRegistered.end())
        return;
    Entry& entry = it->second;

    const TextureId texId = visual.texture();
    debugCheck(mAssets.textures().contains(texId.id), "updateImguiImage: unknown TextureId");

    const float scale = std::max(contentScale, 1e-3f);
    const ResolvedGeom geom = resolveGeom(visual, entry.spec, scale);

    entry.opacity = visual.opacity();
    entry.visible = visual.visible();

    if (geom.rttPhys != entry.rttSize || geom.texId.id != entry.texture.id ||
        geom.meshId.id != entry.mesh.id)
    {
        // Geometry/source changed enough to need a fresh RTT (this re-warms the handle).
        freeEntryGpu(entry);
        unpinEntry(entry);
        Entry fresh;
        allocateEntry(fresh, geom);
        fresh.opacity = entry.opacity;
        fresh.visible = entry.visible;
        fresh.spec    = entry.spec;
        entry = fresh;
    }
    else
    {
        // Same target size: keep the RTT + handle, just re-render with the new content.
        entry.layer       = geom.layer;
        entry.transform   = geom.transform;
        entry.displaySize = geom.displaySize;
        entry.dirty       = true;
    }
}

void ImguiImageManager::unregisterImage(ImguiImageId id)
{
    auto it = mRegistered.find(id.id);
    if (it == mRegistered.end())
        return;
    freeEntryGpu(it->second);
    unpinEntry(it->second);
    mRegistered.erase(it);
}

void ImguiImageManager::drawImage(ImguiImageId id, std::optional<glm::vec2> drawSize)
{
    auto it = mRegistered.find(id.id);
    if (it == mRegistered.end())
        return;
    Entry& entry = it->second;
    entry.lastUsedFrame = mFrameCounter;

    const glm::vec2 size = drawSize.value_or(entry.displaySize);
    if (entry.visible && entry.handle != 0)
        drawResolvedImage(entry.handle, size, entry.texture, entry.opacity);
    else
        ImGui::Dummy(ImVec2(size.x, size.y)); // invisible, or handle not yet ready: reserve layout
}

std::uint64_t ImguiImageManager::handleOf(ImguiImageId id) const
{
    auto it = mRegistered.find(id.id);
    return it == mRegistered.end() ? 0 : it->second.handle;
}

glm::vec2 ImguiImageManager::sizeOf(ImguiImageId id) const
{
    auto it = mRegistered.find(id.id);
    return it == mRegistered.end() ? glm::vec2(0.0f) : it->second.displaySize;
}

bool ImguiImageManager::isReady(ImguiImageId id) const
{
    auto it = mRegistered.find(id.id);
    return it != mRegistered.end() && it->second.handle != 0;
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

void ImguiImageManager::pushPass(std::vector<RttPass>& out, const Entry& entry)
{
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

void ImguiImageManager::appendInternalPasses(std::vector<RttPass>& out)
{
    // Lazy entries: re-render every entry drawn this frame.
    for (auto& [key, entry] : mEntries)
        if (entry.lastUsedFrame == mFrameCounter)
            pushPass(out, entry);

    // Registered entries: re-render when displayed this frame, when their source changed
    // (dirty), or once to populate a freshly registered RTT (everRendered). Off-screen
    // registered images cost nothing but keep their last-rendered content + valid handle.
    for (auto& [id, entry] : mRegistered)
    {
        const bool displayed = entry.lastUsedFrame == mFrameCounter;
        const bool render = entry.visible && (displayed || entry.dirty || not entry.everRendered);
        entry.renderedThisFrame = render;
        if (render)
        {
            pushPass(out, entry);
            entry.everRendered = true;
            entry.dirty = false;
        }
    }
}

void ImguiImageManager::resolveAndGarbageCollect()
{
    RenderTargetContainer& renderTargets = mAssets.renderTargets();

    // 1. For images rendered this frame, refresh the flat-2D and (lazily) create the
    //    ImGui handle. Acquire before resolve so the creation frame already has pixels.
    auto resolveEntry = [&](Entry& entry)
    {
        if (not renderTargets.contains(entry.rt.id))
            return;
        RenderTargetPack& pack = renderTargets.at(entry.rt.id);
        if (not pack.dRenderTargetOpt.has_value())
            return;

        const DRenderTarget& dRenderTarget = pack.dRenderTargetOpt.value();
        if (entry.handle == 0)
            entry.handle = mBackend.acquireFlat2DImguiHandle(dRenderTarget);
        mBackend.resolveRenderTargetFlat2D(dRenderTarget);
    };

    for (auto& [key, entry] : mEntries)
        if (entry.lastUsedFrame == mFrameCounter)
            resolveEntry(entry);

    // Registered entries: resolve any that emitted a pass this frame (display, dirty, or
    // initial populate). The handle, once created, persists for the registration lifetime.
    for (auto& [id, entry] : mRegistered)
        if (entry.renderedThisFrame)
            resolveEntry(entry);

    // 2. Retire lazy entries unused for a while (frees the internal RTT + ImGui handle).
    //    Registered entries are never garbage-collected — only unregisterImage frees them.
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
    for (auto& [id, entry] : mRegistered)
    {
        freeEntryGpu(entry);
        unpinEntry(entry);
    }
    mEntries.clear();
    mRegistered.clear();
    mTexturePins.clear();
    mMeshPins.clear();
    mOwnedQuads.clear();
}

} // namespace Nothofagus
