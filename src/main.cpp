#include "area51xr/bridge_host.h"
#include "area51xr/xr_host.h"
#include "core/projection.hpp"

#ifdef A51XR_HAS_OPENXR
#include "area51xr/openxr_runtime.h"
#endif

#include <charconv>
#include <chrono>
#include <iostream>
#include <optional>
#include <string_view>
#include <thread>

namespace {

std::optional<double> parse_number(std::string_view value) {
    double result{};
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const auto parsed = std::from_chars(first, last, result);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
        return std::nullopt;
    }
    return result;
}

void print_usage() {
    std::cout
        << "Area51XR host\n"
        << "Usage:\n"
        << "  area51xr --version\n"
        << "  area51xr --simulate-gun <x> <y>\n"
        << "  area51xr --bridge <x> <y> [--fire]\n"
#ifdef A51XR_HAS_OPENXR
        << "  area51xr --xr-bridge\n"
#endif
        << "\n"
        << "Coordinates are normalized to [0, 1].\n";
}

int run_bridge(float x, float y, bool fire) {
    area51xr::MameIpc ipc;
    if (!ipc.create()) {
        std::cerr << "failed to create MAME bridge\n";
        return 4;
    }

    area51xr::write_gun_state(*ipc.state(), x, y, fire);
    std::cout << "MAME bridge ready: aim=" << x << ',' << y
              << " trigger=" << (fire ? "down" : "up") << std::endl;
    std::cout << "Waiting for MAME frames. Press Ctrl+C to stop." << std::endl;

    std::uint64_t last_frame = 0;
    for (;;) {
        const auto status = area51xr::read_frame_status(*ipc.state());
        if (status.frame_number != 0 && status.frame_number != last_frame) {
            last_frame = status.frame_number;
            std::cout << "frame=" << status.frame_number
                      << " size=" << status.width << 'x' << status.height
                      << " bytes=" << status.payload_bytes << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

#ifdef A51XR_HAS_OPENXR
int run_xr_bridge() {
    area51xr::MameIpc ipc;
    if (!ipc.create()) {
        std::cerr << "failed to create MAME bridge\n";
        return 4;
    }

    area51xr::OpenXrRuntime runtime;
    area51xr::XrHost host(runtime, *ipc.state());
    if (!host.initialize()) {
        std::cerr << "OpenXR initialization failed: " << runtime.last_error() << std::endl;
        return 5;
    }

    std::cout << "OpenXR bridge initialized. Waiting for runtime session and MAME frames." << std::endl;
    std::uint64_t last_frame = 0;
    bool last_running = false;
    bool logged_frame = false;
    bool logged_valid_aim = false;
    bool last_offscreen = false;
    bool last_coin = false;
    bool last_start = false;
    auto next_status_log = std::chrono::steady_clock::now();

    for (;;) {
        const auto tick = host.tick();
        if (!tick.runtime_ok) {
            std::cerr << "OpenXR runtime error: " << runtime.last_error() << std::endl;
            host.shutdown();
            return 6;
        }
        if (tick.session_running != last_running) {
            last_running = tick.session_running;
            std::cout << "xr_session=" << (last_running ? "running" : "waiting") << std::endl;
        }

        const bool new_frame = tick.frame.frame_number != 0 && tick.frame.frame_number != last_frame;
        if (new_frame) {
            last_frame = tick.frame.frame_number;
            const auto now = std::chrono::steady_clock::now();
            const bool aim_became_valid = tick.aim_valid && !logged_valid_aim;
            const bool offscreen_changed = tick.offscreen != last_offscreen;
            const bool controls_changed = tick.coin_down != last_coin || tick.start_down != last_start;
            const bool periodic_status = now >= next_status_log;

            if (!logged_frame || aim_became_valid || offscreen_changed || controls_changed || periodic_status) {
                std::cout << "frame=" << tick.frame.frame_number
                          << " size=" << tick.frame.width << 'x' << tick.frame.height
                          << " aim=" << tick.aim.x << ',' << tick.aim.y
                          << " aim_valid=" << (tick.aim_valid ? 1 : 0)
                          << " offscreen=" << (tick.offscreen ? 1 : 0)
                          << " trigger=" << (tick.trigger_down ? "down" : "up")
                          << " coin=" << (tick.coin_down ? "down" : "up")
                          << " start=" << (tick.start_down ? "down" : "up")
                          << std::endl;
                logged_frame = true;
                logged_valid_aim = logged_valid_aim || tick.aim_valid;
                last_offscreen = tick.offscreen;
                last_coin = tick.coin_down;
                last_start = tick.start_down;
                next_status_log = now + std::chrono::seconds(1);
            }
        }

        if (!tick.session_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}
#endif

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "Area51XR 0.2.0\n";
        return 0;
    }

#ifdef A51XR_HAS_OPENXR
    if (argc == 2 && std::string_view{argv[1]} == "--xr-bridge") {
        return run_xr_bridge();
    }
#endif

    if (argc == 4 && std::string_view{argv[1]} == "--simulate-gun") {
        const auto x = parse_number(argv[2]);
        const auto y = parse_number(argv[3]);
        if (!x || !y) {
            std::cerr << "invalid coordinate\n";
            return 2;
        }

        const auto projected = a51xr::normalized_to_gun({*x, *y}, {640, 480});
        if (!projected) {
            std::cerr << "coordinate outside normalized range\n";
            return 3;
        }

        std::cout << "gun_x=" << projected->x << " gun_y=" << projected->y << '\n';
        return 0;
    }

    if ((argc == 4 || argc == 5) && std::string_view{argv[1]} == "--bridge") {
        const auto x = parse_number(argv[2]);
        const auto y = parse_number(argv[3]);
        if (!x || !y || *x < 0.0 || *x > 1.0 || *y < 0.0 || *y > 1.0) {
            std::cerr << "invalid bridge coordinate\n";
            return 2;
        }
        const bool fire = argc == 5 && std::string_view{argv[4]} == "--fire";
        if (argc == 5 && !fire) {
            print_usage();
            return 1;
        }
        return run_bridge(static_cast<float>(*x), static_cast<float>(*y), fire);
    }

    print_usage();
    return argc == 1 ? 0 : 1;
}
