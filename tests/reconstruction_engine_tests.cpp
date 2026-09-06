#include "area51xr/reconstruction_engine.h"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    const std::array<std::uint8_t, 16> pixels{
        0, 0, 0, 255,
        0, 0, 0, 255,
        0, 0, 0, 255,
        0, 0, 0, 255
    };

    area51xr::ColorFrameView frame{};
    frame.frame_number = 7;
    frame.width = 2;
    frame.height = 2;
    frame.stride_bytes = 8;
    frame.pixels = pixels;

    area51xr::SyntheticDepthProvider provider(2.0f, 0.0f);
    area51xr::DepthReconstructionOptions options{};
    options.stabilizer.confidence_rise = 1.0f;
    options.mesh_min_confidence = 1.0f;

    area51xr::ReconstructionEngine engine(provider, options, 1.0f);
    const auto& first = engine.process(frame);
    assert(first.depth_ok);
    assert(first.reconstruction.stats.valid_depth_samples == 4);
    assert(first.reconstruction.stats.mesh_triangles == 2);

    provider.set_horizontal_slope(0.2f);
    const auto& sloped = engine.process(frame);
    assert(sloped.depth_ok);
    assert(sloped.reconstruction.mesh.vertices.size() == 4);
    assert(sloped.reconstruction.mesh.vertices[0].position.z !=
           sloped.reconstruction.mesh.vertices[1].position.z);

    area51xr::ColorFrameView bad = frame;
    bad.stride_bytes = 4;
    const auto& invalid = engine.process(bad);
    assert(!invalid.depth_ok);

    engine.reset();
    const auto& after_reset = engine.process(frame);
    assert(after_reset.reconstruction.stats.frame_number == 1);
    return 0;
}
