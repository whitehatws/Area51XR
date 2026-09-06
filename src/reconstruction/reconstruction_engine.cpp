#include "area51xr/reconstruction_engine.h"

namespace area51xr {

ReconstructionEngine::ReconstructionEngine(
    DepthProvider& provider,
    DepthReconstructionOptions options,
    float vertical_fov_radians)
    : provider_(provider),
      reconstructor_(options),
      vertical_fov_radians_(vertical_fov_radians) {}

const ReconstructionEngineResult& ReconstructionEngine::process(const ColorFrameView& frame) {
    result_ = {};
    if (!provider_.estimate(frame, estimate_)) {
        return result_;
    }
    if (estimate_.width != frame.width || estimate_.height != frame.height ||
        estimate_.depth_m.size() != static_cast<std::size_t>(frame.width) * frame.height) {
        return result_;
    }

    const auto intrinsics = intrinsics_from_vertical_fov(
        frame.width, frame.height, vertical_fov_radians_);
    result_.depth_ok = intrinsics.fx > 0.0f && intrinsics.fy > 0.0f;
    if (!result_.depth_ok) {
        return result_;
    }

    result_.reconstruction = reconstructor_.update(
        estimate_.depth_m, frame.width, frame.height, intrinsics);
    return result_;
}

void ReconstructionEngine::reset() {
    reconstructor_.reset();
    estimate_ = {};
    result_ = {};
}

} // namespace area51xr
