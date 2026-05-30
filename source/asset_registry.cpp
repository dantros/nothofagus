#include "asset_registry.h"
#include "check.h"
#include "bellota_to_mesh.h"
#include "texture_mode.h"

#include <utility>
#include <unordered_set>

namespace Nothofagus
{

AssetRegistry::AssetRegistry(ActiveBackend& backend)
    : mBackend(backend)
{
}

// ---------------------------------------------------------------------------
// Bellotas
// ---------------------------------------------------------------------------

BellotaId AssetRegistry::addBellota(const Bellota& bellota)
{
    BellotaId newBellotaId{mBellotas.add({bellota, std::nullopt})};
    Bellota& newBellota = this->bellota(newBellotaId);

    TextureId newTextureId = newBellota.texture();
    mTextureUsageMonitor.addEntry(newBellotaId, newTextureId);

    // Materialize an auto-quad if the user didn't supply a mesh explicitly.
    if (not newBellota.meshId().has_value())
    {
        const MeshId autoQuadId = materializeAutoQuad(newBellota);
        newBellota.meshId() = autoQuadId;
    }
    else
    {
        debugCheck(mMeshes.contains(newBellota.meshId().value().id),
                   "Bellota constructed with a MeshId that is not registered in this canvas");
        // Tile-map textures need the auto-quad's exact UV invariant (UVs in [0, 1]
        // over the full tile-map extent). Custom meshes can't satisfy that.
        debugCheck(mTextures.at(newTextureId.id).mode != TextureMode::TileMap,
                   "Bellota constructed with a custom MeshId on a tile-map texture — combination is unsupported");
    }
    mMeshUsageMonitor.addEntry(newBellotaId, newBellota.meshId().value());

    return newBellotaId;
}

void AssetRegistry::removeBellota(const BellotaId bellotaId)
{
    BellotaPack& bellotaPackToRemove = mBellotas.at(bellotaId.id);
    const TextureId textureId = bellotaPackToRemove.bellota.texture();
    const std::optional<MeshId>& meshIdOpt = bellotaPackToRemove.bellota.meshId();
    debugCheck(meshIdOpt.has_value(), "BellotaPack is missing a MeshId — invariant broken");
    const MeshId meshId = meshIdOpt.value();

    bellotaPackToRemove.clear();
    mBellotas.remove(bellotaId.id);
    mTextureUsageMonitor.removeEntry(bellotaId, textureId);
    mMeshUsageMonitor.removeEntry(bellotaId, meshId);
}

Bellota& AssetRegistry::bellota(BellotaId bellotaId)
{
    return mBellotas.at(bellotaId.id).bellota;
}

const Bellota& AssetRegistry::bellota(BellotaId bellotaId) const
{
    return mBellotas.at(bellotaId.id).bellota;
}

void AssetRegistry::setTint(const BellotaId bellotaId, const Tint& tint)
{
    debugCheck(mBellotas.contains(bellotaId.id), "There is no Bellota associated with the BellotaId provided");
    mBellotas.at(bellotaId.id).tintOpt = tint;
}

void AssetRegistry::removeTint(const BellotaId bellotaId)
{
    debugCheck(mBellotas.contains(bellotaId.id), "There is no Bellota associated with the BellotaId provided");
    mBellotas.at(bellotaId.id).tintOpt = std::nullopt;
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------

TextureId AssetRegistry::addTexture(const Texture& texture)
{
    glm::ivec2 textureSize = std::visit(GetTextureSizeVisitor(), texture);
    TexturePack texturePack{texture, std::nullopt, std::nullopt, std::nullopt, textureSize};
    texturePack.mode = textureModeOf(texture);
    TextureId newTextureId{mTextures.add(std::move(texturePack))};
    const bool textureWasAdded = mTextureUsageMonitor.addUnused(newTextureId);
    debugCheck(textureWasAdded, "Texture ID already present in usage monitor — duplicate addTexture call");
    return newTextureId;
}

void AssetRegistry::removeTexture(const TextureId textureId)
{
    const bool textureWasRemoved = mTextureUsageMonitor.removeUnused(textureId);
    debugCheck(textureWasRemoved, "Texture is not in the unused set — still referenced by a bellota or already removed");

    mTextures.at(textureId.id).freeGpuResources(mBackend);
    mTextures.remove(textureId.id);
}

void AssetRegistry::setTexture(const BellotaId bellotaId, const TextureId textureId)
{
    const Bellota& bellotaOriginal = bellota(bellotaId);
    const std::optional<MeshId>& meshIdOpt = bellotaOriginal.meshId();
    debugCheck(meshIdOpt.has_value(), "BellotaPack is missing a MeshId — invariant broken");

    const bool currentMeshIsAutoQuad = mMeshes.at(meshIdOpt.value().id).isAutoQuad;
    // Switching to a tile-map texture is only valid when the bellota uses an
    // auto-quad (which will be re-materialized below sized to the new texture).
    // A user mesh's UVs can't satisfy the tile-map shader's cell-lookup invariant.
    debugCheck(currentMeshIsAutoQuad or mTextures.at(textureId.id).mode != TextureMode::TileMap,
               "setTexture: cannot switch a bellota with a custom mesh onto a tile-map texture");

    Bellota bellotaWithNewTexture = bellotaOriginal.withTexture(textureId);
    if (currentMeshIsAutoQuad)
    {
        // Auto-quad needs to be regenerated for the new texture size — drop the MeshId
        // here; replaceBellota will re-materialize one sized to the new texture.
        bellotaWithNewTexture.meshId() = std::nullopt;
    }

    replaceBellota(bellotaId, bellotaWithNewTexture);
}

void AssetRegistry::markTextureAsDirty(const TextureId textureId)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    debugCheck(not texturePack.isProxy(), "markTextureAsDirty called on a render target proxy texture.");
    texturePack.freeGpuResources(mBackend);
}

void AssetRegistry::setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    texturePack.minFilter = mode;
    // Indirect index textures require GL_NEAREST — skip filter updates for them.
    if (texturePack.mode == TextureMode::Indirect) return;
    if (texturePack.dtextureOpt.has_value())
        mBackend.setTextureMinFilter(texturePack.dtextureOpt.value(), mode);
}

void AssetRegistry::setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    texturePack.magFilter = mode;
    // Indirect index textures require GL_NEAREST — skip filter updates for them.
    if (texturePack.mode == TextureMode::Indirect) return;
    if (texturePack.dtextureOpt.has_value())
        mBackend.setTextureMagFilter(texturePack.dtextureOpt.value(), mode);
}

Texture& AssetRegistry::texture(TextureId textureId)
{
    return mTextures.at(textureId.id).texture.value();
}

const Texture& AssetRegistry::texture(TextureId textureId) const
{
    return mTextures.at(textureId.id).texture.value();
}

void AssetRegistry::clearUnusedTextures()
{
    const std::unordered_set<TextureId> unusedTextureIdsCopy = mTextureUsageMonitor.getUnusedIds();
    for (TextureId textureId : unusedTextureIdsCopy)
    {
        // Proxy entries are owned by their RenderTargetPack — skip auto-cleanup.
        if (mTextures.at(textureId.id).isProxy())
            continue;
        removeTexture(textureId);
    }
    mTextureUsageMonitor.clearUnusedIds();
}

// ---------------------------------------------------------------------------
// Meshes
// ---------------------------------------------------------------------------

MeshId AssetRegistry::materializeAutoQuad(const Bellota& bellota)
{
    const TextureId textureId = bellota.texture();
    debugCheck(mTextures.contains(textureId.id),
               "materializeAutoQuad: bellota references an unknown texture");
    const glm::ivec2 textureSize = mTextures.at(textureId.id).mTextureSize;

    MeshPack pack;
    pack.mesh = generateQuadMesh(textureSize);
    pack.dmeshOpt = std::nullopt;
    pack.isAutoQuad = true;
    const MeshId newMeshId{mMeshes.add(std::move(pack))};
    const bool added = mMeshUsageMonitor.addUnused(newMeshId);
    debugCheck(added, "materializeAutoQuad: MeshId collision in usage monitor");
    return newMeshId;
}

MeshId AssetRegistry::addMesh(const Mesh& mesh)
{
    MeshPack pack;
    pack.mesh = mesh;
    pack.dmeshOpt = std::nullopt;
    pack.isAutoQuad = false;
    const MeshId newMeshId{mMeshes.add(std::move(pack))};
    const bool added = mMeshUsageMonitor.addUnused(newMeshId);
    debugCheck(added, "Mesh ID already present in usage monitor — duplicate addMesh call");
    return newMeshId;
}

MeshId AssetRegistry::addMesh(Mesh&& mesh)
{
    MeshPack pack;
    pack.mesh = std::move(mesh);
    pack.dmeshOpt = std::nullopt;
    pack.isAutoQuad = false;
    const MeshId newMeshId{mMeshes.add(std::move(pack))};
    const bool added = mMeshUsageMonitor.addUnused(newMeshId);
    debugCheck(added, "Mesh ID already present in usage monitor — duplicate addMesh call");
    return newMeshId;
}

void AssetRegistry::removeMesh(MeshId meshId)
{
    debugCheck(mMeshes.contains(meshId.id), "removeMesh: unknown MeshId");
    debugCheck(not mMeshes.at(meshId.id).isAutoQuad,
               "removeMesh: cannot remove an engine-allocated auto-quad — it is owned by the canvas");

    const bool wasRemoved = mMeshUsageMonitor.removeUnused(meshId);
    debugCheck(wasRemoved, "Mesh is not in the unused set — still referenced by a bellota or already removed");

    mMeshes.at(meshId.id).freeGpuResources(mBackend);
    mMeshes.remove(meshId.id);
}

void AssetRegistry::setMesh(const BellotaId bellotaId, const MeshId meshId)
{
    debugCheck(mMeshes.contains(meshId.id), "setMesh: unknown MeshId");
    const Bellota& bellotaOriginal = bellota(bellotaId);
    // Tile-map textures rely on the auto-quad UV invariant; custom meshes
    // would produce nonsense cell lookups in the tile-map shader.
    debugCheck(mTextures.at(bellotaOriginal.texture().id).mode != TextureMode::TileMap,
               "setMesh: cannot attach a custom mesh to a bellota whose texture is in tile-map mode");

    replaceBellota(bellotaId, bellotaOriginal.withMesh(meshId));
}

const Mesh& AssetRegistry::mesh(MeshId meshId) const
{
    debugCheck(mMeshes.contains(meshId.id), "mesh: unknown MeshId");
    return mMeshes.at(meshId.id).mesh;
}

const Mesh& AssetRegistry::mesh(BellotaId bellotaId) const
{
    const Bellota& target = bellota(bellotaId);
    debugCheck(target.meshId().has_value(), "mesh(BellotaId): bellota has no MeshId — invariant broken");
    return mesh(target.meshId().value());
}

void AssetRegistry::clearUnusedMeshes()
{
    const std::unordered_set<MeshId> unusedMeshIdsCopy = mMeshUsageMonitor.getUnusedIds();
    for (MeshId meshId : unusedMeshIdsCopy)
    {
        // Auto-quads use the same eligibility rules as user meshes; the isAutoQuad
        // flag exists only to gate the public `removeMesh` entry point.
        const bool wasRemoved = mMeshUsageMonitor.removeUnused(meshId);
        debugCheck(wasRemoved, "clearUnusedMeshes: mesh disappeared from unused set unexpectedly");

        mMeshes.at(meshId.id).freeGpuResources(mBackend);
        mMeshes.remove(meshId.id);
    }
}

// ---------------------------------------------------------------------------
// Render targets
// ---------------------------------------------------------------------------

RenderTargetId AssetRegistry::addRenderTarget(ScreenSize size)
{
    const glm::ivec2 texSize{static_cast<int>(size.width), static_cast<int>(size.height)};

    // Register a proxy TexturePack so bellotas can reference the render target like any other texture.
    TexturePack proxyPack;
    proxyPack.texture = std::nullopt;
    proxyPack.dtextureOpt = std::nullopt;
    proxyPack.mTextureSize = texSize;
    TextureId proxyTexId{mTextures.add(proxyPack)};
    mTextureUsageMonitor.addUnused(proxyTexId);

    RenderTargetPack renderTargetPack;
    renderTargetPack.renderTarget = RenderTarget{texSize, proxyTexId};
    renderTargetPack.dRenderTargetOpt = std::nullopt;
    RenderTargetId newId{mRenderTargets.add(renderTargetPack)};

    return newId;
}

void AssetRegistry::removeRenderTarget(RenderTargetId renderTargetId)
{
    RenderTargetPack& renderTargetPack = mRenderTargets.at(renderTargetId.id);
    const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
    renderTargetPack.freeGpuResources(mBackend, mTextures.at(proxyTexId.id));

    mTextures.remove(proxyTexId.id);

    // Remove from usage monitor if currently unused (i.e. no bellotas reference it).
    mTextureUsageMonitor.removeUnused(proxyTexId);

    mRenderTargets.remove(renderTargetId.id);
}

TextureId AssetRegistry::renderTargetTexture(RenderTargetId renderTargetId) const
{
    return mRenderTargets.at(renderTargetId.id).renderTarget.mProxyTextureId;
}

void AssetRegistry::setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor)
{
    mRenderTargets.at(renderTargetId.id).renderTarget.mClearColor = clearColor;
}

// ---------------------------------------------------------------------------
// Bulk teardown
// ---------------------------------------------------------------------------

void AssetRegistry::freeAllGpuResources()
{
    for (auto& [meshIndex, meshPack] : mMeshes)
        meshPack.freeGpuResources(mBackend);

    for (auto& [bellotaIndex, bellotaPack] : mBellotas)
        bellotaPack.clear();

    // Render targets must be processed before textures: freeRenderTarget frees
    // the FBO + the proxy's color attachment in one backend call, and nulls the
    // proxy's dtextureOpt so the subsequent texture loop's isProxy() guard is
    // hit on a clean slate.
    for (auto& [renderTargetIndex, renderTargetPack] : mRenderTargets)
    {
        const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
        if (mTextures.contains(proxyTexId.id))
            renderTargetPack.freeGpuResources(mBackend, mTextures.at(proxyTexId.id));
        else
            renderTargetPack.clear();
    }

    for (auto& [textureIndex, texturePack] : mTextures)
        texturePack.freeGpuResources(mBackend);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void AssetRegistry::replaceBellota(const BellotaId bellotaId, const Bellota& newBellota)
{
    BellotaPack& bellotaPack = mBellotas.at(bellotaId.id);

    TextureId textureIdToReplace = bellotaPack.bellota.texture();
    const bool oldTextureEntryRemoved = mTextureUsageMonitor.removeEntry(bellotaId, textureIdToReplace);
    debugCheck(oldTextureEntryRemoved, "Failed to remove old texture entry from usage monitor during bellota replacement");

    TextureId newTextureId = newBellota.texture();
    const bool newTextureEntryAdded = mTextureUsageMonitor.addEntry(bellotaId, newTextureId);
    debugCheck(newTextureEntryAdded, "Failed to add new texture entry to usage monitor during bellota replacement");

    // Mesh-usage shuffle. Two cases:
    //   * new bellota has explicit MeshId — drop the old MeshId, register the new one
    //   * new bellota has no MeshId — caller (setTexture) wants a fresh auto-quad
    //     sized to the new texture; materialise it now and stamp the bellota copy
    const std::optional<MeshId>& oldMeshIdOpt = bellotaPack.bellota.meshId();
    debugCheck(oldMeshIdOpt.has_value(), "BellotaPack is missing a MeshId — invariant broken");
    const MeshId oldMeshId = oldMeshIdOpt.value();
    const bool oldMeshEntryRemoved = mMeshUsageMonitor.removeEntry(bellotaId, oldMeshId);
    debugCheck(oldMeshEntryRemoved, "Failed to remove old mesh entry from usage monitor during bellota replacement");

    Bellota stampedNewBellota = newBellota;
    if (not stampedNewBellota.meshId().has_value())
    {
        const MeshId autoQuadId = materializeAutoQuad(stampedNewBellota);
        stampedNewBellota.meshId() = autoQuadId;
    }
    else
    {
        debugCheck(mMeshes.contains(stampedNewBellota.meshId().value().id),
                   "Replacement bellota references a MeshId that is not registered");
    }
    const bool newMeshEntryAdded = mMeshUsageMonitor.addEntry(bellotaId, stampedNewBellota.meshId().value());
    debugCheck(newMeshEntryAdded, "Failed to add new mesh entry to usage monitor during bellota replacement");

    bellotaPack.bellota = stampedNewBellota;
}

}
