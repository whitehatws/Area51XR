#pragma once

#include "area51xr/reconstruction_engine.h"
#include "area51xr/reconstruction_presenter.h"
#include "area51xr/reconstruction_quality.h"

namespace area51xr {

struct SpatialReconstructionTick {
    bool depth_ok{};
    bool quality_ok{};
    bool presented{};
    ReconstructionQualityResult quality{};
    DepthReconstructionStats stats{};
};

class SpatialReconstructionHost {
public:
    SpatialReconstructionHost(
        DepthProvider& provider,
        XrRuntime& runtime,
        DepthReconstructionOptions reconstruction_options = {},
        ReconstructionQualityThresholds quality_thresholds = {},
        float vertical_fov_radians = 1.04719755f);

    SpatialReconstructionTick process(const ColorFrameView& frame);
    void reset();

private:
    ReconstructionEngine engine_;
    ReconstructionPresenter presenter_;
    ReconstructionQualityThresholds quality_thresholds_{};
};

} // namespace area51xr
