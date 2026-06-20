#pragma once

#include "bellota.h"
#include "mesh.h"
#include "texture.h"
#include "render_target.h"
#include "screen_size.h"
#include "tint.h"
#include "bellota_container.h"
#include "texture_container.h"
#include "mesh_container.h"
#include "render_target_container.h"
#include "resource_usage_monitor.h"
#include "backends/render_backend_select.h"

#include <glm/glm.hpp>
#include <vector>

namespace Nothofagus
{

using TextureUsageMonitor = ResourceUsageMonitor<TextureId>;
using MeshUsageMonitor    = ResourceUsageMonitor<MeshId>;

/// Owns the CPU-side asset containers (bellotas, textures, meshes, render
/// targets) and their two usage monitors. Encapsulates monitor book-keeping
/// so callers can't forget to register a mesh entry when adding a bellota,
/// register a texture entry when replacing one, or skip the auto-quad
/// materialization step.
///
/// Holds a reference to the active GPU backend so that per-asset remove /
/// dirty paths can free GPU resources eagerly. Whole-registry teardown
/// (destructor or canvas shutdown) goes through `freeAllGpuResources()` so
/// the caller can sequence it against the backend's own shutdown.
///
/// Does NOT enforce explorer-pool-ownership gates or tear down ImGui RTT
/// secondary contexts — both are FrameRunner-level concerns layered on top
/// of this class.
class AssetRegistry
{
public:
    explicit AssetRegistry(ActiveBackend& backend);

    // ---------- Bellotas ----------
    BellotaId addBellota(const Bellota& bellota);
    void removeBellota(BellotaId bellotaId);
    Bellota& bellota(BellotaId bellotaId);
    const Bellota& bellota(BellotaId bellotaId) const;
    void setTint(BellotaId bellotaId, const Tint& tint);
    void removeTint(BellotaId bellotaId);

    // ---------- Textures ----------
    TextureId addTexture(const Texture& texture);
    void removeTexture(TextureId textureId);
    void setTexture(BellotaId bellotaId, TextureId textureId);
    void markTextureAsDirty(TextureId textureId);
    void setTextureMinFilter(TextureId textureId, TextureSampleMode mode);
    void setTextureMagFilter(TextureId textureId, TextureSampleMode mode);
    Texture& texture(TextureId textureId);
    const Texture& texture(TextureId textureId) const;
    void clearUnusedTextures();

    /// Detect-only counterpart to clearUnusedTextures(): returns the non-proxy
    /// textures currently unreferenced by any bellota and removes them from the
    /// usage monitor, WITHOUT freeing GPU resources. The caller (the deferred-
    /// free path) frees each later via freeRetiredTexture() once no in-flight
    /// render snapshot still references it. Proxy textures are skipped (they are
    /// RT-owned) and the unused set is cleared afterwards — identical monitor
    /// side effects to clearUnusedTextures(), minus the immediate free.
    std::vector<TextureId> collectUnusedTextures();

    /// Free GPU resources for a texture returned by collectUnusedTextures() and
    /// erase it from the container. Does not touch the usage monitor — that was
    /// already done by collectUnusedTextures().
    void freeRetiredTexture(TextureId textureId);

    // ---------- Meshes ----------
    MeshId addMesh(const Mesh& mesh);
    MeshId addMesh(Mesh&& mesh);
    void removeMesh(MeshId meshId);
    void setMesh(BellotaId bellotaId, MeshId meshId);
    const Mesh& mesh(MeshId meshId) const;
    const Mesh& mesh(BellotaId bellotaId) const;
    void clearUnusedMeshes();

    /// Detect-only counterpart to clearUnusedMeshes(): returns the meshes
    /// currently unreferenced by any bellota and removes them from the usage
    /// monitor, WITHOUT freeing GPU resources. Auto-quads and user meshes are
    /// treated alike (same eligibility as clearUnusedMeshes). Free each later
    /// via freeRetiredMesh().
    std::vector<MeshId> collectUnusedMeshes();

    /// Free GPU resources for a mesh returned by collectUnusedMeshes() and erase
    /// it from the container. Does not touch the usage monitor.
    void freeRetiredMesh(MeshId meshId);

    // ---------- Render targets ----------
    RenderTargetId addRenderTarget(ScreenSize size);
    void removeRenderTarget(RenderTargetId renderTargetId);
    TextureId renderTargetTexture(RenderTargetId renderTargetId) const;
    void setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor);

    // ---------- Container accessors (frame loop / destructor / managers) ----------
    BellotaContainer& bellotas() { return mBellotas; }
    const BellotaContainer& bellotas() const { return mBellotas; }
    TextureContainer& textures() { return mTextures; }
    const TextureContainer& textures() const { return mTextures; }
    MeshContainer& meshes() { return mMeshes; }
    const MeshContainer& meshes() const { return mMeshes; }
    RenderTargetContainer& renderTargets() { return mRenderTargets; }
    const RenderTargetContainer& renderTargets() const { return mRenderTargets; }

    /// Free every GPU handle held by any container. Call from FrameRunner's
    /// destructor before mBackend.shutdown(). After this call the registry
    /// is logically empty.
    void freeAllGpuResources();

private:
    void replaceBellota(BellotaId bellotaId, const Bellota& bellota);
    MeshId materializeAutoQuad(const Bellota& bellota);

    ActiveBackend& mBackend;

    BellotaContainer mBellotas;
    TextureContainer mTextures;
    MeshContainer mMeshes;
    RenderTargetContainer mRenderTargets;

    TextureUsageMonitor mTextureUsageMonitor;
    MeshUsageMonitor mMeshUsageMonitor;
};

}
