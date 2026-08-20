#pragma once

#include "area51xr/depth_provider.h"
#include "area51xr/depth_reconstruction.h"

namespace area51xr {

struct ReconstructionEngineResult {
    bool depth_ok{};
    DepthReconstructionResult reconstruction;
};

class ReconstructionEngine {
public:
    ReconstructionEngine(
        DepthProvider& provider,
        DepthReconstructionOptions options = {},
        float vertical_fov_radians = 1.04719755f);

    const ReconstructionEngineResult& process(const ColorFrameView& frame);
    void reset();

private:
    DepthProvider& provider_;
    DepthReconstructor reconstructor_;
    DepthEstimate estimate_{};
    ReconstructionEngineResult result_{};
    float vertical_fov_radians_{};
};

} // namespace area51xr
