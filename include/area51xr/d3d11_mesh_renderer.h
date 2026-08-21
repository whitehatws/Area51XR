#pragma once

#include "area51xr/xr_runtime.h"

#include <cstdint>
#include <memory>

namespace area51xr {

class D3D11MeshRenderer final {
public:
    D3D11MeshRenderer();
    ~D3D11MeshRenderer();

    D3D11MeshRenderer(const D3D11MeshRenderer&) = delete;
    D3D11MeshRenderer& operator=(const D3D11MeshRenderer&) = delete;

    bool initialize(void* d3d_device, void* d3d_context) noexcept;
    bool upload(const SpatialMeshView& mesh) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] std::uint64_t frame_number() const noexcept;
    [[nodiscard]] std::uint32_t vertex_count() const noexcept;
    [[nodiscard]] std::uint32_t triangle_count() const noexcept;
    [[nodiscard]] const char* last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace area51xr
