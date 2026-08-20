#include "core/projection.hpp"

#include <charconv>
#include <iostream>
#include <optional>
#include <string_view>

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
        << "\n"
        << "Coordinates are normalized to [0, 1].\n";
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

    print_usage();
    return argc == 1 ? 0 : 1;
}
