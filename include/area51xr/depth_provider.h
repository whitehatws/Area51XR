#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace area51xr {

struct ColorFrameView {
    std::uint64_t frame_number{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride_bytes{};
    std::span<const std::uint8_t> pixels;
};

struct DepthEstimate {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<float> depth_m;
};

class DepthProvider {
public:
    virtual ~DepthProvider() = default;
    virtual bool estimate(const ColorFrameView& frame, DepthEstimate& output) = 0;
};

class SyntheticDepthProvider final : public DepthProvider {
public:
    explicit SyntheticDepthProvider(float base_depth_m = 2.0f, float horizontal_slope_m = 0.0f);

    bool estimate(const ColorFrameView& frame, DepthEstimate& output) override;

    void set_base_depth(float depth_m) noexcept;
    void set_horizontal_slope(float slope_m) noexcept;

private:
    float base_depth_m_{2.0f};
    float horizontal_slope_m_{};
};

} // namespace area51xr
