#include "area51xr/mame_bridge.h"

#include <cassert>

int main() {
    using area51xr::encode_cojag_gun;

    const auto top_left = encode_cojag_gun(0.0f, 0.0f);
    assert(top_left.x == 0);
    assert(top_left.y == 0);
    assert(top_left.packed == 0x000001ffu);

    const auto bottom_right = encode_cojag_gun(1.0f, 1.0f);
    assert(bottom_right.x == 511);
    assert(bottom_right.y == 511);
    assert(bottom_right.packed == 0x01ff0000u);

    const auto center = encode_cojag_gun(0.5f, 0.5f);
    assert(center.x == 256);
    assert(center.y == 256);
    assert(center.packed == 0x010000ffu);

    const auto clamped = encode_cojag_gun(-1.0f, 2.0f);
    assert(clamped.x == 0);
    assert(clamped.y == 511);
    assert(clamped.packed == 0x01ff01ffu);

    return 0;
}
