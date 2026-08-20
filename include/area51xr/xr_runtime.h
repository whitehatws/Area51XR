#pragma once

#include <cstdint>

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

class XrRuntime {
public:
    virtual ~XrRuntime() = default;
    virtual bool initialize() = 0;
    virtual bool poll(XrInputState& state) = 0;
    virtual void shutdown() = 0;
};

class SimulatedXrRuntime final : public XrRuntime {
public:
    explicit SimulatedXrRuntime(XrInputState initial = {});

    bool initialize() override;
    bool poll(XrInputState& state) override;
    void shutdown() override;

    void set_state(const XrInputState& state);

private:
    XrInputState state_{};
    bool initialized_{};
};

Vec3 rotate_vector(const Quat& q, Vec3 v) noexcept;
Vec3 aim_forward(const Pose& pose) noexcept;

} // namespace area51xr
