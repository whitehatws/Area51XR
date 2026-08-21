#include "area51xr/reconstruction_quality.h"

#include <cassert>

int main() {
    area51xr::ReconstructionQualityThresholds thresholds{};
    thresholds.min_valid_depth_ratio = 0.5f;
    thresholds.min_mesh_triangle_ratio = 0.25f;
    thresholds.min_mesh_triangles = 2;

    area51xr::DepthReconstructionStats good{};
    good.valid_depth_ratio = 0.8f;
    good.mesh_triangle_ratio = 0.5f;
    good.mesh_triangles = 4;
    assert(area51xr::evaluate_reconstruction_quality(good, thresholds).passed);

    auto low_depth = good;
    low_depth.valid_depth_ratio = 0.1f;
    assert(!area51xr::evaluate_reconstruction_quality(low_depth, thresholds).passed);

    auto low_count = good;
    low_count.mesh_triangles = 1;
    assert(!area51xr::evaluate_reconstruction_quality(low_count, thresholds).passed);

    auto low_coverage = good;
    low_coverage.mesh_triangle_ratio = 0.1f;
    assert(!area51xr::evaluate_reconstruction_quality(low_coverage, thresholds).passed);

    return 0;
}
