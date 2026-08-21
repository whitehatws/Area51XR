#include "area51xr/reconstruction_quality.h"

namespace area51xr {

ReconstructionQualityResult evaluate_reconstruction_quality(
    const DepthReconstructionStats& stats,
    const ReconstructionQualityThresholds& thresholds) noexcept {
    if (stats.valid_depth_ratio < thresholds.min_valid_depth_ratio) {
        return {false, "valid depth coverage below threshold"};
    }
    if (stats.mesh_triangles < thresholds.min_mesh_triangles) {
        return {false, "mesh triangle count below threshold"};
    }
    if (stats.mesh_triangle_ratio < thresholds.min_mesh_triangle_ratio) {
        return {false, "mesh coverage below threshold"};
    }
    return {true, "ok"};
}

} // namespace area51xr
