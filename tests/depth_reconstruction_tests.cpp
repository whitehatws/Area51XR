#include "area51xr/depth_reconstruction.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {
bool near(float a, float b, float eps = 1.0e-5f) { return std::abs(a - b) < eps; }
}

int main() {
    area51xr::DepthReconstructionOptions options{};
    options.stabilizer.smoothing_alpha = 1.0f;
    options.stabilizer.confidence_rise = 0.5f;
    options.stabilizer.confidence_decay = 0.5f;
    options.stabilizer.usable_confidence = 0.25f;
    options.mesh_min_confidence = 0.5f;
    options.mesh.max_triangle_depth_delta_m = 0.5f;
    options.scene_cut_depth_delta_m = 1.0f;
    options.scene_cut_fraction = 0.5f;

    area51xr::DepthReconstructor reconstructor(options);
    const auto camera = area51xr::intrinsics_from_vertical_fov(2, 2, 1.0f);

    const std::array<float, 4> stable{2.0f, 2.0f, 2.0f, 2.0f};
    const auto& first = reconstructor.update(stable, 2, 2, camera);
    assert(first.stats.frame_number == 1);
    assert(first.stats.total_samples == 4);
    assert(first.stats.confident_samples == 4);
    assert(first.stats.valid_depth_samples == 4);
    assert(first.stats.mesh_triangles == 2);
    assert(first.stats.mesh_max_triangles == 2);
    assert(first.stats.rejected_triangles == 0);
    assert(near(first.stats.confident_sample_ratio, 1.0f));
    assert(near(first.stats.valid_depth_ratio, 1.0f));
    assert(near(first.stats.mesh_triangle_ratio, 1.0f));
    assert(!first.stats.scene_cut_reset);

    const std::array<float, 4> one_missing{0.0f, 2.0f, 2.0f, 2.0f};
    const auto& missing = reconstructor.update(one_missing, 2, 2, camera);
    assert(missing.stats.confident_samples == 4);
    assert(missing.stats.valid_depth_samples == 3);
    assert(missing.stats.mesh_triangles == 1);
    assert(missing.stats.rejected_triangles == 1);
    assert(near(missing.stats.valid_depth_ratio, 0.75f));
    assert(near(missing.stats.mesh_triangle_ratio, 0.5f));

    // All four samples move by more than the cut threshold; temporal history
    // must reset instead of blending the old shot into the new one.
    const std::array<float, 4> cut{5.0f, 5.0f, 5.0f, 5.0f};
    const auto& scene_cut = reconstructor.update(cut, 2, 2, camera);
    assert(scene_cut.stats.scene_cut_reset);
    assert(scene_cut.stats.valid_depth_samples == 4);
    assert(scene_cut.stats.mesh_triangles == 2);

    const std::array<float, 3> bad_size{1.0f, 1.0f, 1.0f};
    const auto& invalid = reconstructor.update(bad_size, 2, 2, camera);
    assert(invalid.mesh.indices.empty());
    assert(invalid.stats.total_samples == 4);
    assert(invalid.stats.valid_depth_samples == 0);
    assert(invalid.stats.mesh_max_triangles == 0);

    reconstructor.reset();
    const auto& after_reset = reconstructor.update(stable, 2, 2, camera);
    assert(after_reset.stats.frame_number == 1);
    return 0;
}
