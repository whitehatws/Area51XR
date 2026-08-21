#pragma once

#include "area51xr/depth_inference.h"

#include <filesystem>
#include <memory>

namespace area51xr {

class OnnxRuntimeBackend final : public TensorInferenceBackend {
public:
    explicit OnnxRuntimeBackend(const std::filesystem::path& model_path);
    ~OnnxRuntimeBackend() override;

    OnnxRuntimeBackend(const OnnxRuntimeBackend&) = delete;
    OnnxRuntimeBackend& operator=(const OnnxRuntimeBackend&) = delete;

    bool run(
        std::span<const float> chw_rgb,
        std::uint32_t input_width,
        std::uint32_t input_height,
        std::vector<float>& output,
        std::uint32_t& output_width,
        std::uint32_t& output_height) override;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] const char* last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace area51xr
