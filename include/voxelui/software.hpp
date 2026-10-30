#pragma once
#include "draw_list.hpp"
#include <filesystem>
namespace voxelui {
inline namespace v1 {
class SoftwareRenderer {
    int width_{}, height_{};
    std::vector<std::uint8_t> pixels_;

  public:
    void render(const DrawList &);
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }
    const std::vector<std::uint8_t> &pixels() const {
        return pixels_;
    }
    bool savePng(const std::filesystem::path &) const;
};
} // namespace v1
} // namespace voxelui
