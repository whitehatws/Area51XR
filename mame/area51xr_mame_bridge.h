#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif
#ifdef EXCEPTION_ILLEGAL_INSTRUCTION
#undef EXCEPTION_ILLEGAL_INSTRUCTION
#endif
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace area51xr_mame {

constexpr std::uint32_t kProtocolVersion = 2;
constexpr std::size_t kFrameBufferBytes = 760u * 512u * 4u;

struct GunState {
    std::uint32_t protocol_version;
    std::uint32_t padding0;
    std::uint64_t sequence;
    float aim_x;
    float aim_y;
    std::uint8_t trigger;
    std::uint8_t offscreen;
    std::uint8_t coin;
    std::uint8_t start;
    std::uint8_t reserved[4];
};

struct FrameHeader {
    std::uint32_t protocol_version;
    std::uint32_t padding0;
    std::uint64_t sequence;
    std::uint64_t frame_number;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride_bytes;
    std::uint32_t pixel_format;
    std::uint64_t presentation_time_ns;
    std::uint64_t payload_bytes;
};

struct SharedState {
    std::uint32_t protocol_version;
    std::uint32_t reserved0;
    GunState gun;
    FrameHeader frame;
    std::uint8_t frame_pixels[kFrameBufferBytes];
};

static_assert(sizeof(GunState) == 32);
static_assert(sizeof(FrameHeader) == 56);
static_assert(offsetof(SharedState, gun) == 8);
static_assert(offsetof(SharedState, frame) == 40);
static_assert(offsetof(SharedState, frame_pixels) == 96);

class Bridge {
public:
    ~Bridge() { close(); }

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    static Bridge& instance()
    {
        static Bridge bridge;
        return bridge;
    }

    const GunState* gun()
    {
        if (!ensure_open())
            return nullptr;
        if (state_->protocol_version != kProtocolVersion || state_->gun.protocol_version != kProtocolVersion)
            return nullptr;

#ifdef _WIN32
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            const LONG64 before = InterlockedCompareExchange64(
                reinterpret_cast<volatile LONG64*>(&state_->gun.sequence), 0, 0);
            if (before & 1)
                continue;

            gun_snapshot_ = state_->gun;

            const LONG64 after = InterlockedCompareExchange64(
                reinterpret_cast<volatile LONG64*>(&state_->gun.sequence), 0, 0);
            if (before == after && !(after & 1))
                return &gun_snapshot_;
        }
#endif
        return nullptr;
    }

    template <typename Bitmap, typename Rect>
    void publish_bitmap(const Bitmap& bitmap, const Rect& requested)
    {
        if (!ensure_open())
            return;

        if (bitmap.width() <= 0 || bitmap.height() <= 0)
            return;

        const int left = std::max(0, requested.left());
        const int top = std::max(0, requested.top());
        const int right = std::min(bitmap.width() - 1, requested.right());
        const int bottom = std::min(bitmap.height() - 1, requested.bottom());
        if (right < left || bottom < top)
            return;

        const std::uint32_t width = static_cast<std::uint32_t>(right - left + 1);
        const std::uint32_t height = static_cast<std::uint32_t>(bottom - top + 1);
        const std::size_t row_bytes = static_cast<std::size_t>(width) * sizeof(std::uint32_t);
        const std::size_t payload_bytes = row_bytes * height;
        if (payload_bytes == 0 || payload_bytes > kFrameBufferBytes)
            return;

#ifdef _WIN32
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&state_->frame.sequence));
#endif
        state_->frame.protocol_version = kProtocolVersion;
        state_->frame.frame_number = ++frame_number_;
        state_->frame.width = width;
        state_->frame.height = height;
        state_->frame.stride_bytes = static_cast<std::uint32_t>(row_bytes);
        state_->frame.pixel_format = 1;
        state_->frame.presentation_time_ns = 0;
        state_->frame.payload_bytes = payload_bytes;

        for (std::uint32_t row = 0; row < height; ++row)
        {
            const int y = top + static_cast<int>(row);
            std::memcpy(
                state_->frame_pixels + static_cast<std::size_t>(row) * row_bytes,
                &bitmap.pix(y, left),
                row_bytes);
        }

#ifdef _WIN32
        MemoryBarrier();
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&state_->frame.sequence));
#endif
    }

private:
    Bridge() = default;

    bool ensure_open()
    {
#ifdef _WIN32
        if (state_)
            return true;

        mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\Area51XR_MAME_v2");
        if (!mapping_)
            return false;

        void* view = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState));
        if (!view)
        {
            CloseHandle(mapping_);
            mapping_ = nullptr;
            return false;
        }

        state_ = static_cast<SharedState*>(view);
        if (state_->protocol_version != kProtocolVersion)
        {
            close();
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    void close()
    {
#ifdef _WIN32
        if (state_)
            UnmapViewOfFile(state_);
        if (mapping_)
            CloseHandle(mapping_);
#endif
        state_ = nullptr;
#ifdef _WIN32
        mapping_ = nullptr;
#endif
    }

#ifdef _WIN32
    HANDLE mapping_{};
#endif
    SharedState* state_{};
    GunState gun_snapshot_{};
    std::uint64_t frame_number_{};
};

inline const GunState* gun_state()
{
    return Bridge::instance().gun();
}

template <typename Bitmap, typename Rect>
inline void publish_bitmap(const Bitmap& bitmap, const Rect& visible)
{
    Bridge::instance().publish_bitmap(bitmap, visible);
}

inline std::uint8_t normalized_to_mame_axis(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
}

inline std::uint16_t apply_system_input(std::uint16_t input)
{
    if (const auto* const xr = gun_state(); xr && xr->coin)
        input &= ~std::uint16_t(0x0001);
    return input;
}

inline std::uint32_t apply_p1_p2_input(std::uint32_t input)
{
    if (const auto* const xr = gun_state(); xr && xr->start)
        input &= ~std::uint32_t(0x01000000);
    return input;
}

} // namespace area51xr_mame
