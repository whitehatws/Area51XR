#include "area51xr/mame_ipc.h"

#include <algorithm>
#include <atomic>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace area51xr {
namespace {

constexpr const wchar_t* kMappingName = L"Local\\Area51XR_MAME_v3";

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
    if (state_->protocol_version != kMameBridgeProtocolVersion) {
        close();
        return false;
    }
    return true;
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

    std::atomic_ref<std::uint64_t> sequence(shared.frame.sequence);
    sequence.fetch_add(1, std::memory_order_acq_rel);

    shared.frame.protocol_version = header.protocol_version;
    shared.frame.frame_number = header.frame_number;
    shared.frame.width = header.width;
    shared.frame.height = header.height;
    shared.frame.stride_bytes = header.stride_bytes;
    shared.frame.pixel_format = header.pixel_format;
    shared.frame.presentation_time_ns = header.presentation_time_ns;
    shared.frame.payload_bytes = header.payload_bytes;
    std::copy(pixels.begin(), pixels.end(), shared.frame_pixels);

    sequence.fetch_add(1, std::memory_order_release);
    return true;
}

bool copy_latest_frame(
    const MameSharedState& shared,
    MameFrameHeader& header,
    std::span<std::uint8_t> destination) noexcept {
    std::atomic_ref<std::uint64_t> sequence(const_cast<std::uint64_t&>(shared.frame.sequence));

    for (int attempt = 0; attempt < 4; ++attempt) {
        const std::uint64_t before = sequence.load(std::memory_order_acquire);
        if (before & 1u) {
            continue;
        }

        MameFrameHeader candidate = shared.frame;
        if (candidate.protocol_version != kMameBridgeProtocolVersion ||
            candidate.payload_bytes > destination.size() ||
            candidate.payload_bytes > kMameFrameBufferBytes) {
            return false;
        }

        std::copy_n(shared.frame_pixels, static_cast<std::size_t>(candidate.payload_bytes), destination.begin());
        const std::uint64_t after = sequence.load(std::memory_order_acquire);
        if (before == after && !(after & 1u)) {
            candidate.sequence = after;
            header = candidate;
            return true;
        }
    }
    return false;
}

}  // namespace area51xr
