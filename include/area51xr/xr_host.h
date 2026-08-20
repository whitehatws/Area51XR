#pragma once

#include "area51xr/bridge_host.h"
#include "area51xr/xr_aim.h"
#include "area51xr/xr_runtime.h"

#include <cstdint>
#include <vector>

namespace area51xr {

struct HostTickResult {
    bool runtime_ok{};
    bool session_running{};
    bool aim_valid{};
    NormalizedAim aim{};
    bool trigger_down{};
    bool frame_presented{};
    FrameStatus frame{};
};

class XrHost {
public:
    XrHost(XrRuntime& runtime, MameSharedState& shared, AimPlane plane = {});

    bool initialize();
    HostTickResult tick();
    void shutdown();

private:
    XrRuntime& runtime_;
    MameSharedState& shared_;
    AimPlane plane_{};
    NormalizedAim last_aim_{0.5f, 0.5f};
    std::vector<std::uint8_t> frame_buffer_;
    std::uint64_t last_presented_frame_{};
    bool initialized_{};
};

} // namespace area51xr
