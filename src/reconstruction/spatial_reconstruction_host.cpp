#include "area51xr/spatial_reconstruction_host.h"

namespace area51xr {

SpatialReconstructionHost::SpatialReconstructionHost(
    DepthProvider& provider,
    XrRuntime& runtime,
    DepthReconstructionOptions reconstruction_options,
    ReconstructionQualityThresholds quality_thresholds,
    float vertical_fov_radians)
    : engine_(provider, reconstruction_options, vertical_fov_radians),
      presenter_(runtime),
      quality_thresholds_(quality_thresholds) {}

SpatialReconstructionTick SpatialReconstructionHost::process(const ColorFrameView& frame) {
    SpatialReconstructionTick tick{};
    const auto& result = engine_.process(frame);
    tick.depth_ok = result.depth_ok;
    tick.stats = result.reconstruction.stats;
    tick.quality = evaluate_reconstruction_quality(tick.stats, quality_thresholds_);
    tick.quality_ok = tick.quality.passed;
    if (tick.depth_ok && tick.quality_ok) {
        tick.presented = presenter_.present(result.reconstruction);
    }
    return tick;
}

void SpatialReconstructionHost::reset() {
    engine_.reset();
}

} // namespace area51xr
