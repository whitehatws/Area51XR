#include "area51xr/reconstruction_capture.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

int main() {
    const std::array<std::uint8_t, 16> pixels{
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };
    const std::array<float, 4> depth{1.0f, 2.0f, 3.0f, 4.0f};

    area51xr::ColorFrameView frame{};
    frame.frame_number = 123;
    frame.width = 2;
    frame.height = 2;
    frame.stride_bytes = 8;
    frame.pixels = pixels;

    const auto encoded = area51xr::encode_reconstruction_capture(frame, depth);
    assert(!encoded.empty());

    area51xr::ReconstructionCapture decoded{};
    assert(area51xr::decode_reconstruction_capture(encoded, decoded));
    assert(decoded.frame_number == 123);
    assert(decoded.width == 2);
    assert(decoded.height == 2);
    assert(decoded.stride_bytes == 8);
    assert(decoded.pixels.size() == pixels.size());
    assert(decoded.depth_m.size() == depth.size());
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        assert(decoded.pixels[i] == pixels[i]);
    }
    for (std::size_t i = 0; i < depth.size(); ++i) {
        assert(std::abs(decoded.depth_m[i] - depth[i]) < 1.0e-6f);
    }

    auto corrupted = encoded;
    corrupted[corrupted.size() / 2] ^= 0x80u;
    assert(!area51xr::decode_reconstruction_capture(corrupted, decoded));

    auto truncated = encoded;
    truncated.resize(truncated.size() - 3);
    assert(!area51xr::decode_reconstruction_capture(truncated, decoded));

    area51xr::ColorFrameView invalid = frame;
    invalid.stride_bytes = 4;
    assert(area51xr::encode_reconstruction_capture(invalid, depth).empty());
    return 0;
}
