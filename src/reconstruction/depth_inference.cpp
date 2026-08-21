#include "area51xr/depth_inference.h"

namespace area51xr {

DepthAnythingV2Provider::DepthAnythingV2Provider(
    TensorInferenceBackend& backend,
    std::uint32_t input_size,
    RelativeDepthCalibration calibration)
    : backend_(backend),
      input_size_(input_size),
      calibration_(calibration) {}

bool DepthAnythingV2Provider::estimate(const ColorFrameView& frame, DepthEstimate& output) {
    output = {};
    stats_ = {};
    if (!preprocess_depth_anything_v2(frame, input_, input_size_)) {
        return false;
    }

    std::uint32_t raw_width{};
    std::uint32_t raw_height{};
    raw_output_.clear();
    if (!backend_.run(
            input_.chw_rgb,
            input_.width,
            input_.height,
            raw_output_,
            raw_width,
            raw_height)) {
        return false;
    }

    output.width = frame.width;
    output.height = frame.height;
    if (!depth_anything_v2_output_to_metric(
            raw_output_,
            raw_width,
            raw_height,
            frame.width,
            frame.height,
            output.depth_m,
            stats_,
            calibration_)) {
        output = {};
        return false;
    }
    return true;
}

const RelativeDepthStats& DepthAnythingV2Provider::last_stats() const noexcept {
    return stats_;
}

} // namespace area51xr
