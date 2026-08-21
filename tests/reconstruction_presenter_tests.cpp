#include "area51xr/reconstruction_presenter.h"

#include <array>
#include <cassert>

int main() {
    area51xr::XrInputState state{};
    state.session_running = true;
    area51xr::SimulatedXrRuntime runtime(state);
    assert(runtime.initialize());

    area51xr::DepthReconstructionResult reconstruction{};
    reconstruction.stats.frame_number = 12;
    reconstruction.mesh.vertices.push_back({{0.0f, 0.0f, -1.0f}, 0.0f, 0.0f});
    reconstruction.mesh.vertices.push_back({{1.0f, 0.0f, -1.0f}, 1.0f, 0.0f});
    reconstruction.mesh.vertices.push_back({{0.0f, 1.0f, -1.0f}, 0.0f, 1.0f});
    reconstruction.mesh.indices = {0, 1, 2};

    area51xr::ReconstructionPresenter presenter(runtime);
    assert(presenter.present(reconstruction));
    assert(presenter.last_presented_frame() == 12);
    assert(presenter.last_presented_vertices() == 3);
    assert(presenter.last_presented_triangles() == 1);
    assert(runtime.last_presented_mesh_frame() == 12);
    assert(runtime.last_presented_mesh_vertices() == 3);
    assert(runtime.last_presented_mesh_triangles() == 1);

    runtime.shutdown();
    assert(!presenter.present(reconstruction));
    return 0;
}
