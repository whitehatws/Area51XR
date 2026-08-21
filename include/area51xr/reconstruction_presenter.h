#pragma once

#include "area51xr/depth_reconstruction.h"
#include "area51xr/xr_runtime.h"

#include <cstdint>
#include <vector>

namespace area51xr {

class ReconstructionPresenter {
public:
    explicit ReconstructionPresenter(XrRuntime& runtime);

    bool present(const DepthReconstructionResult& reconstruction);

    [[nodiscard]] std::uint64_t last_presented_frame() const noexcept;
    [[nodiscard]] std::size_t last_presented_vertices() const noexcept;
    [[nodiscard]] std::size_t last_presented_triangles() const noexcept;

private:
    XrRuntime& runtime_;
    std::vector<SpatialMeshVertexView> vertices_;
    std::uint64_t last_presented_frame_{};
    std::size_t last_presented_vertices_{};
    std::size_t last_presented_triangles_{};
};

} // namespace area51xr
