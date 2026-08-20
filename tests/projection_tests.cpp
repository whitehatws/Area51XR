#include "core/projection.hpp"

#include <cmath>
#include <iostream>

namespace {

bool near(double a, double b) {
    return std::abs(a - b) < 0.0001;
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    const auto center = a51xr::normalized_to_gun({0.5, 0.5}, {640, 480});
    if (!center || !near(center->x, 319.5) || !near(center->y, 239.5)) {
        return fail("center projection failed");
    }

    const auto upper_left = a51xr::normalized_to_gun({0.0, 1.0}, {640, 480});
    if (!upper_left || !near(upper_left->x, 0.0) || !near(upper_left->y, 0.0)) {
        return fail("upper-left projection failed");
    }

    if (a51xr::normalized_to_gun({-0.01, 0.5}, {640, 480})) {
        return fail("out-of-range input was accepted");
    }

    if (a51xr::normalized_to_gun({0.5, 0.5}, {0, 480})) {
        return fail("invalid bounds were accepted");
    }

    return 0;
}
