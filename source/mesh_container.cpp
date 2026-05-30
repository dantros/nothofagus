#include "mesh_container.h"

namespace Nothofagus
{

void MeshPack::freeGpuResources(ActiveBackend& backend)
{
    if (dmeshOpt.has_value())
        backend.freeMesh(*dmeshOpt);
    clear();
}

void MeshPack::syncToGpu(ActiveBackend& backend)
{
    if (isDirty())
        dmeshOpt = backend.uploadMesh(mesh);
}

}
