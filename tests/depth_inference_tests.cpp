#include "area51xr/depth_inference.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

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
        output = {1.0f, 2.0f, 3.0f, 4.0f};
        return true;
    }

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

    FakeBackend backend;
    area51xr::DepthAnythingV2Provider provider(backend, 14, calibration);
    area51xr::DepthEstimate depth{};
    assert(provider.estimate(frame, depth));
    assert(backend.saw_input);
    assert(depth.width == 2);
    assert(depth.height == 2);
    assert(depth.depth_m.size() == 4);
    assert(depth.depth_m.front() > depth.depth_m.back());
    assert(provider.last_stats().valid_samples == 4);

    FailingBackend failing;
    area51xr::DepthAnythingV2Provider failing_provider(failing, 14, calibration);
    assert(!failing_provider.estimate(frame, depth));
    assert(depth.depth_m.empty());
    return 0;
}
