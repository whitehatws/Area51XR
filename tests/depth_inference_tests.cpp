#include "area51xr/depth_inference.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace {

bool near(float a, float b, float eps = 1.0e-4f) { return std::abs(a - b) < eps; }

class FakeBackend final : public area51xr::TensorInferenceBackend {
public:
    bool run(
        std::span<const float> input,
        std::uint32_t input_width,
        std::uint32_t input_height,
        std::vector<float>& output,
        std::uint32_t& output_width,
        std::uint32_t& output_height) override {
        saw_input = !input.empty() && input_width == 14 && input_height == 14;
        output_width = 2;
        output_height = 2;
        output = next_output;
        return true;
    }

    std::vector<float> next_output{1.0f, 2.0f, 3.0f, 4.0f};
    bool saw_input{};
};

class FailingBackend final : public area51xr::TensorInferenceBackend {
public:
    bool run(
        std::span<const float>,
        std::uint32_t,
        std::uint32_t,
        std::vector<float>&,
        std::uint32_t&,
        std::uint32_t&) override {
        return false;
    }
};

} // namespace

int main() {
    const std::array<std::uint8_t, 16> pixels{
        0, 0, 0, 255,
        32, 64, 128, 255,
        255, 128, 64, 255,
        255, 255, 255, 255
    };
    area51xr::ColorFrameView frame{};
    frame.width = 2;
    frame.height = 2;
    frame.stride_bytes = 8;
    frame.pixels = pixels;

    area51xr::RelativeDepthCalibration calibration{};
    calibration.near_distance_m = 1.0f;
    calibration.far_distance_m = 4.0f;
    calibration.far_percentile = 0.0f;
    calibration.near_percentile = 1.0f;

    area51xr::RelativeDepthAnchorOptions anchor_options{};
    anchor_options.smoothing_alpha = 0.5f;
    anchor_options.reset_span_multiple = 10.0f;

    FakeBackend backend;
    area51xr::DepthAnythingV2Provider provider(backend, 14, calibration, anchor_options);
    area51xr::DepthEstimate depth{};
    assert(provider.estimate(frame, depth));
    assert(backend.saw_input);
    assert(depth.width == 2);
    assert(depth.height == 2);
    assert(depth.depth_m.size() == 4);
    assert(depth.depth_m.front() > depth.depth_m.back());
    assert(provider.last_stats().valid_samples == 4);
    assert(near(provider.anchors().far_disparity, 1.0f));
    assert(near(provider.anchors().near_disparity, 4.0f));

    backend.next_output = {2.0f, 3.0f, 4.0f, 5.0f};
    assert(provider.estimate(frame, depth));
    assert(near(provider.anchors().far_disparity, 1.5f));
    assert(near(provider.anchors().near_disparity, 4.5f));

    provider.reset_anchors();
    assert(!provider.anchors().valid_samples);
    assert(provider.estimate(frame, depth));
    assert(near(provider.anchors().far_disparity, 2.0f));
    assert(near(provider.anchors().near_disparity, 5.0f));

    FailingBackend failing;
    area51xr::DepthAnythingV2Provider failing_provider(failing, 14, calibration);
    assert(!failing_provider.estimate(frame, depth));
    assert(depth.depth_m.empty());
    return 0;
}
