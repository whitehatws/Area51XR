#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace area51xr {

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

struct Quat {
    float x{};
    float y{};
    float z{};
    float w{1.0f};
};

struct Pose {
    Vec3 position{};
    Quat orientation{};
};

struct XrInputState {
    bool session_running{};
    bool pose_valid{};
    Pose head{};
    Pose aim{};
    bool trigger_down{};
    bool coin_down{};
    bool start_down{};
    std::uint64_t sample_number{};
};

struct VideoFrameView {
    std::uint64_t frame_number{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride_bytes{};
    std::uint32_t pixel_format{};
    std::span<const std::uint8_t> pixels{};
};

struct SpatialMeshVertexView {
    Vec3 position{};
    float u{};
    float v{};
};

struct SpatialMeshView {
    std::uint64_t frame_number{};
    std::span<const SpatialMeshVertexView> vertices{};
    std::span<const std::uint32_t> indices{};
};

class XrRuntime {
public:
    virtual ~XrRuntime() = default;
    virtual bool initialize() = 0;
    virtual bool poll(XrInputState& state) = 0;
    virtual bool present(const VideoFrameView& frame) = 0;
    virtual bool present_mesh(const SpatialMeshView& mesh) = 0;
    virtual void shutdown() = 0;
};

class SimulatedXrRuntime final : public XrRuntime {
public:
    explicit SimulatedXrRuntime(XrInputState initial = {});

    bool initialize() override;
    bool poll(XrInputState& state) override;
    bool present(const VideoFrameView& frame) override;
    bool present_mesh(const SpatialMeshView& mesh) override;
    void shutdown() override;

    void set_state(const XrInputState& state);
    [[nodiscard]] std::uint64_t last_presented_frame() const noexcept;
    [[nodiscard]] std::uint64_t last_presented_mesh_frame() const noexcept;
    [[nodiscard]] std::size_t last_presented_mesh_vertices() const noexcept;
    [[nodiscard]] std::size_t last_presented_mesh_triangles() const noexcept;

private:
    XrInputState state_{};
    std::uint64_t last_presented_frame_{};
    std::uint64_t last_presented_mesh_frame_{};
    std::size_t last_presented_mesh_vertices_{};
    std::size_t last_presented_mesh_triangles_{};
    bool initialized_{};
};

Vec3 rotate_vector(const Quat& q, Vec3 v) noexcept;
Vec3 aim_forward(const Pose& pose) noexcept;

} // namespace area51xr
