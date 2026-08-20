#pragma once

#include "area51xr/depth_mesh.h"
#include "area51xr/depth_stabilizer.h"

#include <cstdint>
#include <span>
#include <vector>

namespace area51xr {

struct DepthReconstructionOptions {
    DepthStabilizerOptions stabilizer{};
    DepthMeshOptions mesh{};
    float mesh_min_confidence{0.35f};
    float scene_cut_depth_delta_m{1.5f};
    float scene_cut_fraction{0.45f};
};

struct DepthReconstructionStats {
    std::uint64_t frame_number{};
    std::size_t valid_depth_samples{};
    std::size_t mesh_triangles{};
    bool scene_cut_reset{};
};

struct DepthReconstructionResult {
    DepthMesh mesh;
    DepthReconstructionStats stats{};
};

class DepthReconstructor {
public:
    explicit DepthReconstructor(DepthReconstructionOptions options = {});

    const DepthReconstructionResult& update(
        std::span<const float> depth_m,
        std::uint32_t width,
        std::uint32_t height,
        const CameraIntrinsics& intrinsics);

    void reset();

private:
    bool detect_scene_cut(std::span<const float> incoming) const noexcept;

    DepthReconstructionOptions options_{};
    DepthStabilizer stabilizer_;
    std::vector<float> previous_input_;
    std::vector<float> filtered_depth_;
    DepthReconstructionResult result_{};
    std::uint64_t frame_number_{};
};

} // namespace area51xr
