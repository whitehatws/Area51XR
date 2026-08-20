#pragma once

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

class XrRuntime {
public:
    virtual ~XrRuntime() = default;
    virtual bool initialize() = 0;
    virtual bool poll(XrInputState& state) = 0;
    virtual bool present(const VideoFrameView& frame) = 0;
    virtual void shutdown() = 0;
};

class SimulatedXrRuntime final : public XrRuntime {
public:
    explicit SimulatedXrRuntime(XrInputState initial = {});

    bool initialize() override;
    bool poll(XrInputState& state) override;
    bool present(const VideoFrameView& frame) override;
    void shutdown() override;

    void set_state(const XrInputState& state);
    [[nodiscard]] std::uint64_t last_presented_frame() const noexcept;

private:
    XrInputState state_{};
    std::uint64_t last_presented_frame_{};
    bool initialized_{};
};

Vec3 rotate_vector(const Quat& q, Vec3 v) noexcept;
Vec3 aim_forward(const Pose& pose) noexcept;

} // namespace area51xr
