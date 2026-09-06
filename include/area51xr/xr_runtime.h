#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

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

enum class ControllerHand : std::uint8_t {
    left,
    right
};

struct XrInputState {
    bool session_running{};
    bool pose_valid{};
    Pose head{};
    Pose aim{};
    bool trigger_down{};
    bool coin_down{};
    bool start_down{};
    bool menu_down{};
    ControllerHand active_hand{ControllerHand::right};

    bool left_pose_valid{};
    Pose left_aim{};
    bool left_trigger_down{};
    bool left_coin_down{};
    bool left_start_down{};
    bool left_menu_down{};

    bool right_pose_valid{};
    Pose right_aim{};
    bool right_trigger_down{};
    bool right_coin_down{};
    bool right_start_down{};
    bool right_menu_down{};

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

enum class PassthroughState : std::uint8_t {
    unavailable,
    off,
    on
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
    [[nodiscard]] virtual PassthroughState passthrough_state() const noexcept = 0;
    [[nodiscard]] virtual const char* passthrough_extension() const noexcept = 0;
    virtual bool set_passthrough_enabled(bool enabled) = 0;
    virtual void shutdown() = 0;
};

class SimulatedXrRuntime final : public XrRuntime {
public:
    explicit SimulatedXrRuntime(XrInputState initial = {});

    bool initialize() override;
    bool poll(XrInputState& state) override;
    bool present(const VideoFrameView& frame) override;
    bool present_mesh(const SpatialMeshView& mesh) override;
    [[nodiscard]] PassthroughState passthrough_state() const noexcept override;
    [[nodiscard]] const char* passthrough_extension() const noexcept override;
    bool set_passthrough_enabled(bool enabled) override;
    void shutdown() override;

    void set_state(const XrInputState& state);
    void set_passthrough_supported(bool supported);
    void set_passthrough_enable_failure(bool fail);
    [[nodiscard]] std::uint64_t last_presented_frame() const noexcept;
    [[nodiscard]] std::uint64_t last_presented_mesh_frame() const noexcept;
    [[nodiscard]] std::size_t last_presented_mesh_vertices() const noexcept;
    [[nodiscard]] std::size_t last_presented_mesh_triangles() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> last_presented_pixels() const noexcept;

private:
    XrInputState state_{};
    std::uint64_t last_presented_frame_{};
    std::uint64_t last_presented_mesh_frame_{};
    std::size_t last_presented_mesh_vertices_{};
    std::size_t last_presented_mesh_triangles_{};
    std::vector<std::uint8_t> last_presented_pixels_{};
    PassthroughState passthrough_state_{PassthroughState::unavailable};
    bool passthrough_enable_failure_{};
    bool initialized_{};
};

Vec3 rotate_vector(const Quat& q, Vec3 v) noexcept;
Vec3 aim_forward(const Pose& pose) noexcept;

} // namespace area51xr
