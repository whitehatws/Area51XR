#include "area51xr/depth_reconstruction.h"

#include <algorithm>
#include <cmath>

namespace area51xr {
namespace {

std::size_t max_triangle_count(std::uint32_t width, std::uint32_t height, const DepthMeshOptions& options) noexcept {
    if (width == 0 || height == 0 || options.sample_step == 0) {
        return 0;
    }
    const std::uint32_t columns = (width - 1) / options.sample_step + 1;
    const std::uint32_t rows = (height - 1) / options.sample_step + 1;
    if (columns < 2 || rows < 2) {
        return 0;
    }
    return static_cast<std::size_t>(columns - 1) * static_cast<std::size_t>(rows - 1) * 2u;
}

float ratio(std::size_t value, std::size_t total) noexcept {
    return total == 0 ? 0.0f : static_cast<float>(value) / static_cast<float>(total);
}

} // namespace

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
    result_.stats.total_samples = expected;
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

    std::size_t confident{};
    std::size_t valid{};
    for (std::size_t i = 0; i < stable.depth_m.size(); ++i) {
        if (stable.confidence[i] >= min_confidence) {
            ++confident;
            filtered_depth_[i] = stable.depth_m[i];
            if (filtered_depth_[i] > 0.0f) {
                ++valid;
            }
        } else {
            filtered_depth_[i] = 0.0f;
        }
    }

    result_.mesh = build_depth_mesh(filtered_depth_, width, height, intrinsics, options_.mesh);
    result_.stats.confident_samples = confident;
    result_.stats.valid_depth_samples = valid;
    result_.stats.mesh_triangles = result_.mesh.indices.size() / 3;
    result_.stats.mesh_max_triangles = max_triangle_count(width, height, options_.mesh);
    result_.stats.rejected_triangles = result_.stats.mesh_max_triangles > result_.stats.mesh_triangles
        ? result_.stats.mesh_max_triangles - result_.stats.mesh_triangles
        : 0;
    result_.stats.confident_sample_ratio = ratio(confident, expected);
    result_.stats.valid_depth_ratio = ratio(valid, expected);
    result_.stats.mesh_triangle_ratio = ratio(result_.stats.mesh_triangles, result_.stats.mesh_max_triangles);
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
