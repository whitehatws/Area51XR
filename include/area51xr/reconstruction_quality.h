#pragma once

#include "area51xr/depth_reconstruction.h"

namespace area51xr {

struct ReconstructionQualityThresholds {
    float min_valid_depth_ratio{0.55f};
    float min_mesh_triangle_ratio{0.30f};
    std::size_t min_mesh_triangles{16};
};

struct ReconstructionQualityResult {
    bool passed{};
    const char* reason{"not evaluated"};
};

ReconstructionQualityResult evaluate_reconstruction_quality(
    const DepthReconstructionStats& stats,
    const ReconstructionQualityThresholds& thresholds = {}) noexcept;

} // namespace area51xr
