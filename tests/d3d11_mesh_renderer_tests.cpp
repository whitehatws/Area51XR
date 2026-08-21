#include "area51xr/d3d11_mesh_renderer.h"

#include <cassert>

int main() {
    area51xr::D3D11MeshRenderer renderer;
    assert(!renderer.initialized());
    assert(!renderer.upload({}));
    assert(renderer.last_error()[0] != '\0');

#ifndef _WIN32
    assert(!renderer.initialize(nullptr, nullptr));
    assert(!renderer.initialized());
#endif

    return 0;
}
