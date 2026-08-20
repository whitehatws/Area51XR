#include "area51xr/depth_reconstruction.h"

#include <algorithm>
#include <cmath>

namespace area51xr {

DepthReconstructor::DepthReconstructor(DepthReconstructionOptions options)
    : options_(options), stabilizer_(options.stabilizer) {}

bool DepthReconstructor::detect_scene_cut(std::span<const float> incoming) const noexcept {
    if (previous_input_.size() != incoming.size() || incoming.empty()) {
        return false;
    }

    std::size_t comparable{};
    std::size_t changed{};
    for (std::size_t i = 0; i < incoming.size(); ++i) {
        const float a = previous_input_[i];
        const float b = incoming[i];
        if (!std::isfinite(a) || !std::isfinite(b) ||
            a < options_.stabilizer.min_depth_m || b < options_.stabilizer.min_depth_m ||
            a > options_.stabilizer.max_depth_m || b > options_.stabilizer.max_depth_m) {
            continue;
        }
        ++comparable;
        if (std::abs(a - b) >= options_.scene_cut_depth_delta_m) {
            ++changed;
        }
    }

    if (comparable == 0) {
        return false;
    }
    const float fraction = static_cast<float>(changed) / static_cast<float>(comparable);
    return fraction >= std::clamp(options_.scene_cut_fraction, 0.0f, 1.0f);
}

const DepthReconstructionResult& DepthReconstructor::update(
    std::span<const float> depth_m,
    std::uint32_t width,
    std::uint32_t height,
    const CameraIntrinsics& intrinsics) {
    const std::size_t expected = static_cast<std::size_t>(width) * height;
    result_ = {};
    result_.stats.frame_number = ++frame_number_;
    if (depth_m.size() != expected || expected == 0) {
        previous_input_.assign(depth_m.begin(), depth_m.end());
        return result_;
    }

    const bool scene_cut = detect_scene_cut(depth_m);
    if (scene_cut) {
        stabilizer_.reset();
    }

    const auto stable = stabilizer_.update(depth_m);
    filtered_depth_.resize(stable.depth_m.size());
    const float min_confidence = std::clamp(options_.mesh_min_confidence, 0.0f, 1.0f);

    std::size_t valid{};
    for (std::size_t i = 0; i < stable.depth_m.size(); ++i) {
        if (stable.confidence[i] >= min_confidence) {
            filtered_depth_[i] = stable.depth_m[i];
            if (filtered_depth_[i] > 0.0f) {
                ++valid;
            }
        } else {
            filtered_depth_[i] = 0.0f;
        }
    }

    result_.mesh = build_depth_mesh(filtered_depth_, width, height, intrinsics, options_.mesh);
    result_.stats.valid_depth_samples = valid;
    result_.stats.mesh_triangles = result_.mesh.indices.size() / 3;
    result_.stats.scene_cut_reset = scene_cut;

    previous_input_.assign(depth_m.begin(), depth_m.end());
    return result_;
}

void DepthReconstructor::reset() {
    stabilizer_.reset();
    previous_input_.clear();
    filtered_depth_.clear();
    result_ = {};
    frame_number_ = 0;
}

} // namespace area51xr
