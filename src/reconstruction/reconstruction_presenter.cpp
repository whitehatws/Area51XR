#include "area51xr/reconstruction_presenter.h"

namespace area51xr {

ReconstructionPresenter::ReconstructionPresenter(XrRuntime& runtime)
    : runtime_(runtime) {}

bool ReconstructionPresenter::present(const DepthReconstructionResult& reconstruction) {
    vertices_.clear();
    vertices_.reserve(reconstruction.mesh.vertices.size());
    for (const auto& vertex : reconstruction.mesh.vertices) {
        vertices_.push_back({vertex.position, vertex.u, vertex.v});
    }

    SpatialMeshView view{};
    view.frame_number = reconstruction.stats.frame_number;
    view.vertices = vertices_;
    view.indices = reconstruction.mesh.indices;

    if (!runtime_.present_mesh(view)) {
        return false;
    }

    last_presented_frame_ = view.frame_number;
    last_presented_vertices_ = view.vertices.size();
    last_presented_triangles_ = view.indices.size() / 3;
    return true;
}

std::uint64_t ReconstructionPresenter::last_presented_frame() const noexcept {
    return last_presented_frame_;
}

std::size_t ReconstructionPresenter::last_presented_vertices() const noexcept {
    return last_presented_vertices_;
}

std::size_t ReconstructionPresenter::last_presented_triangles() const noexcept {
    return last_presented_triangles_;
}

} // namespace area51xr
