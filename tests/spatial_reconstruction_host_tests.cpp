#include "area51xr/spatial_reconstruction_host.h"

#include <array>
#include <cassert>

int main() {
    const std::array<std::uint8_t, 16> pixels{
        0, 0, 0, 255,
        32, 64, 128, 255,
        255, 128, 64, 255,
        255, 255, 255, 255
    };
    area51xr::ColorFrameView frame{};
    frame.frame_number = 4;
    frame.width = 2;
    frame.height = 2;
    frame.stride_bytes = 8;
    frame.pixels = pixels;

    area51xr::SyntheticDepthProvider provider(2.0f);
    area51xr::XrInputState state{};
    state.session_running = true;
    area51xr::SimulatedXrRuntime runtime(state);
    assert(runtime.initialize());

    area51xr::DepthReconstructionOptions options{};
    options.stabilizer.smoothing_alpha = 1.0f;
    options.stabilizer.confidence_rise = 1.0f;
    options.mesh.sample_step = 1;
    options.mesh.max_triangle_depth_delta_m = 0.5f;

    area51xr::ReconstructionQualityThresholds thresholds{};
    thresholds.min_valid_depth_ratio = 0.5f;
    thresholds.min_mesh_triangle_ratio = 0.5f;
    thresholds.min_mesh_triangles = 1;

    area51xr::SpatialReconstructionHost host(provider, runtime, options, thresholds);
    const auto tick = host.process(frame);
    assert(tick.depth_ok);
    assert(tick.quality_ok);
    assert(tick.presented);
    assert(tick.stats.mesh_triangles == 2);
    assert(runtime.last_presented_mesh_frame() == 1);
    assert(runtime.last_presented_mesh_vertices() == 4);
    assert(runtime.last_presented_mesh_triangles() == 2);

    thresholds.min_mesh_triangles = 99;
    area51xr::SpatialReconstructionHost strict_host(provider, runtime, options, thresholds);
    const auto rejected = strict_host.process(frame);
    assert(rejected.depth_ok);
    assert(!rejected.quality_ok);
    assert(!rejected.presented);

    return 0;
}
