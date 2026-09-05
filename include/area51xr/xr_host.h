#pragma once

#include "area51xr/bridge_host.h"
#include "area51xr/xr_aim.h"
#include "area51xr/xr_runtime.h"

#include <chrono>
#include <cstdint>
#include <vector>

namespace area51xr {

struct HostTickResult {
    bool runtime_ok{};
    bool session_running{};
    bool aim_valid{};
    bool offscreen{};
    NormalizedAim aim{};
    bool trigger_down{};
    bool coin_down{};
    bool start_down{};
    bool menu_open{};
    bool startup_active{};
    bool restart_requested{};
    bool quit_requested{};
    bool frame_presented{};
    FrameStatus frame{};
};

class XrHost {
public:
    XrHost(
        XrRuntime& runtime,
        MameSharedState& shared,
        AimPlane plane = {},
        bool show_startup = true);

    bool initialize();
    HostTickResult tick();
    void shutdown();

private:
    XrRuntime& runtime_;
    MameSharedState& shared_;
    AimPlane plane_{};
    NormalizedAim last_aim_{0.5f, 0.5f};
    std::vector<std::uint8_t> frame_buffer_;
    std::vector<std::uint8_t> ui_buffer_;
    std::vector<std::uint8_t> startup_buffer_;
    MameFrameHeader cached_frame_{};
    std::uint64_t last_copied_frame_{};
    std::chrono::steady_clock::time_point startup_started_at_{};
    bool show_startup_{true};
    bool startup_started_{};
    bool startup_rendered_{};
    bool has_frame_{};
    bool menu_open_{};
    bool last_menu_down_{};
    bool last_trigger_down_{};
    std::uint8_t restart_token_{};
    std::uint8_t quit_token_{};
    bool initialized_{};
};

} // namespace area51xr
