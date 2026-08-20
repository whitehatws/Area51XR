#include "area51xr/bridge_host.h"
#include "core/projection.hpp"

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
        << "Area51XR diagnostic host\n"
        << "Usage:\n"
        << "  area51xr --version\n"
        << "  area51xr --simulate-gun <x> <y>\n"
        << "  area51xr --bridge <x> <y> [--fire]\n"
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

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "Area51XR 0.1.0\n";
        return 0;
    }

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
