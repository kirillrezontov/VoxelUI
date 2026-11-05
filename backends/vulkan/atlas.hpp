#pragma once
#include <unordered_map>
#include <voxelui/draw_list.hpp>
namespace voxelui::vulkan {
// Bounded owning cache. Published pages are immutable; edits use copy-on-write.
// A compiler can grow/repack between frames without invalidating submitted meshes.
class Atlas {
    struct Entry {
        std::shared_ptr<const void> owner;
        Rect region;
    };
    std::unordered_map<const void *, Entry> entries_;
    std::shared_ptr<Image> page_;
    int x_{}, y_{}, row_{};

  public:
    int size{256};
    std::shared_ptr<const Image> page() const {
        return page_;
    }
    void clear() {
        entries_.clear();
        page_.reset();
        x_ = y_ = row_ = 0;
    }
    std::optional<Rect> insert(std::shared_ptr<const void> owner, int w, int h,
                               std::span<const std::uint8_t> pixels, bool mask) {
        if (!owner || w <= 0 || h <= 0 || w > 2048 || h > 2048 || size < 1 || size > 2048 ||
            pixels.size() != std::size_t(w) * std::size_t(h) * (mask ? 1u : 4u))
            return {};
        if (auto it = entries_.find(owner.get()); it != entries_.end())
            return it->second.region;
        if (entries_.size() >= 16384)
            return {};
        if (w <= 0 || h <= 0 || w > size || h > size)
            return {};
        int x = x_, y = y_, row = row_;
        if (x + w > size) {
            x = 0;
            y += row;
            row = 0;
        }
        if (y + h > size)
            return {};
        if (!page_) {
            page_ = std::make_shared<Image>();
            page_->width = page_->height = size;
            page_->rgba.resize(std::size_t(size) * std::size_t(size) * 4);
        } else if (!page_.unique())
            page_ = std::make_shared<Image>(*page_);
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) {
                auto dst = (std::size_t(y + j) * std::size_t(size) + std::size_t(x + i)) * 4,
                     src = std::size_t(j) * std::size_t(w) + std::size_t(i);
                if (mask) {
                    page_->rgba[dst] = page_->rgba[dst + 1] = page_->rgba[dst + 2] = 255;
                    page_->rgba[dst + 3] = pixels[src];
                } else
                    std::copy_n(pixels.data() + src * 4, 4, page_->rgba.data() + dst);
            }
        Rect region{float(x), float(y), float(w), float(h)};
        auto key = owner.get();
        entries_.emplace(key, Entry{std::move(owner), region});
        x_ = x + w;
        y_ = y;
        row_ = std::max(row, h);
        return region;
    }
};
} // namespace voxelui::vulkan
