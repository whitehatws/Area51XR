#include "area51xr/mame_ipc.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace area51xr {
namespace {

constexpr const wchar_t* kMappingName = L"Local\\Area51XR_MAME_v1";

}  // namespace

MameIpc::~MameIpc() {
    close();
}

bool MameIpc::create() {
    close();
#ifdef _WIN32
    HANDLE mapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(sizeof(MameSharedState)),
        kMappingName);
    if (!mapping) {
        return false;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MameSharedState));
    if (!view) {
        CloseHandle(mapping);
        return false;
    }

    mapping_ = mapping;
    state_ = static_cast<MameSharedState*>(view);
    std::memset(state_, 0, sizeof(MameSharedState));
    state_->protocol_version = kMameBridgeProtocolVersion;
    state_->gun.protocol_version = kMameBridgeProtocolVersion;
    state_->frame.protocol_version = kMameBridgeProtocolVersion;
    return true;
#else
    return false;
#endif
}

bool MameIpc::open() {
    close();
#ifdef _WIN32
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, kMappingName);
    if (!mapping) {
        return false;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MameSharedState));
    if (!view) {
        CloseHandle(mapping);
        return false;
    }

    mapping_ = mapping;
    state_ = static_cast<MameSharedState*>(view);
    return state_->protocol_version == kMameBridgeProtocolVersion;
#else
    return false;
#endif
}

void MameIpc::close() {
#ifdef _WIN32
    if (state_) {
        UnmapViewOfFile(state_);
    }
    if (mapping_) {
        CloseHandle(static_cast<HANDLE>(mapping_));
    }
#endif
    state_ = nullptr;
    mapping_ = nullptr;
}

bool MameIpc::valid() const noexcept {
    return state_ != nullptr;
}

MameSharedState* MameIpc::state() noexcept {
    return state_;
}

const MameSharedState* MameIpc::state() const noexcept {
    return state_;
}

bool publish_frame(
    MameSharedState& shared,
    const MameFrameHeader& header,
    std::span<const std::uint8_t> pixels) {
    if (header.protocol_version != kMameBridgeProtocolVersion) {
        return false;
    }
    if (header.payload_bytes != pixels.size() || pixels.size() > kMameFrameBufferBytes) {
        return false;
    }

    std::copy(pixels.begin(), pixels.end(), shared.frame_pixels);
    shared.frame = header;
    return true;
}

}  // namespace area51xr
