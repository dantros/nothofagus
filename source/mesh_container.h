#pragma once

#include "mesh.h"
#include "dmesh.h"
#include "indexed_container.h"
#include <optional>

namespace Nothofagus
{

/**
 * @struct MeshPack
 * @brief Storage cell for a registered mesh asset: CPU data + GPU handle.
 *
 * The CPU mesh is always present (set at registration). The GPU `DMesh` is
 * lazy-uploaded on the next frame via the backend; `isDirty()` reports the
 * pre-upload state.
 *
 * `isAutoQuad` distinguishes engine-allocated auto-quads from user meshes.
 * Auto-quads are regenerated on `setTexture` swaps and cannot be removed
 * via `removeMesh`.
 */
struct MeshPack
{
    Mesh mesh;
    std::optional<DMesh> dmeshOpt;
    bool isAutoQuad = false;

    bool isDirty() const { return not dmeshOpt.has_value(); }

    void clear()
    {
        dmeshOpt = std::nullopt;
    }
};

using MeshContainer = IndexedContainer<MeshPack>;

}
