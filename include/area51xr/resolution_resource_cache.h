#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace area51xr {

template <typename Resource>
class ResolutionResourceCache {
public:
    struct Entry {
        std::uint32_t width{};
        std::uint32_t height{};
        Resource resource{};
    };

    template <typename Factory>
    Resource* get_or_create(std::uint32_t width, std::uint32_t height, Factory&& factory) {
        for (auto& entry : entries_) {
            if (entry.width == width && entry.height == height) {
                return &entry.resource;
            }
        }

        std::optional<Resource> created = factory();
        if (!created) return nullptr;
        entries_.push_back(Entry{width, height, std::move(*created)});
        return &entries_.back().resource;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] std::vector<Entry>& entries() noexcept { return entries_; }
    void clear() noexcept { entries_.clear(); }

private:
    std::vector<Entry> entries_;
};

} // namespace area51xr
