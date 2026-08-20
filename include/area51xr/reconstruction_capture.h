#pragma once

#include "area51xr/depth_provider.h"

#include <cstdint>
#include <span>
#include <vector>

namespace area51xr {

constexpr std::uint32_t kReconstructionCaptureVersion = 1;

struct ReconstructionCapture {
    std::uint64_t frame_number{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride_bytes{};
    std::vector<std::uint8_t> pixels;
    std::vector<float> depth_m;
};

std::vector<std::uint8_t> encode_reconstruction_capture(
    const ColorFrameView& frame,
    std::span<const float> depth_m);

bool decode_reconstruction_capture(
    std::span<const std::uint8_t> bytes,
    ReconstructionCapture& output);

} // namespace area51xr
