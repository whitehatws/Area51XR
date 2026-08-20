#pragma once

#include "area51xr/xr_runtime.h"

#include <memory>

namespace area51xr {

class OpenXrRuntime final : public XrRuntime {
public:
    OpenXrRuntime();
    ~OpenXrRuntime() override;

    OpenXrRuntime(const OpenXrRuntime&) = delete;
    OpenXrRuntime& operator=(const OpenXrRuntime&) = delete;

    bool initialize() override;
    bool poll(XrInputState& state) override;
    bool present(const VideoFrameView& frame) override;
    void shutdown() override;

    [[nodiscard]] const char* last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace area51xr
