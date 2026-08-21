#ifdef _WIN32

#include "area51xr/openxr_runtime.h"

namespace area51xr {

bool OpenXrRuntime::present_mesh(const SpatialMeshView&) {
    // The reconstruction pipeline can now hand spatial meshes to the XR layer.
    // The first hardware milestone still presents the live game as a quad; the
    // D3D11 mesh renderer will replace this no-op behind the same interface.
    return true;
}

} // namespace area51xr

#endif // _WIN32
