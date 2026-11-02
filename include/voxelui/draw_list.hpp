#pragma once
#include "svg.hpp"
#include "text.hpp"
#include <variant>
namespace voxelui {
inline namespace v1 {
struct DrawRectangle {
    bool operator==(const DrawRectangle &) const = default;
    Rect bounds;
    Color fill, border;
    float borderWidth{}, radius{};
};
struct DrawImage {
    bool operator==(const DrawImage &) const = default;
    Rect bounds;
    std::shared_ptr<const Image> image;
    float opacity{1};
};
struct DrawText {
    bool operator==(const DrawText &) const = default;
    Vec2 origin;
    std::shared_ptr<const TextLayout> layout;
    Color color;
};
struct DrawVectorGeometry {
    bool operator==(const DrawVectorGeometry &) const = default;
    Rect bounds;
    std::shared_ptr<const VectorDocument> geometry;
    float opacity{1};
};
struct PushClip {
    bool operator==(const PushClip &) const = default;
    Rect bounds;
};
struct PopClip { bool operator==(const PopClip &) const = default; };
struct PushTransform {
    bool operator==(const PushTransform &) const = default;
    Transform transform;
};
struct PopTransform { bool operator==(const PopTransform &) const = default; };
using DrawCommand = std::variant<DrawRectangle, DrawImage, DrawText, DrawVectorGeometry, PushClip, PopClip,
                                 PushTransform, PopTransform>;
struct DrawBatch {
    bool operator==(const DrawBatch &) const = default;
    std::size_t first{}, count{}, commandType{};
};
struct DrawList {
    bool operator==(const DrawList &) const = default;
    std::vector<DrawCommand> commands;
    Vec2 logicalSize;
    float framebufferScale{1};
    std::vector<DrawBatch> batches;
    void rebuildBatches() {
        batches.clear();
        auto compatible = [](const DrawCommand &a, const DrawCommand &b) {
            if (a.index() != b.index() || a.index() > 3)
                return false;
            if (auto *image = std::get_if<DrawImage>(&a))
                return image->image == std::get<DrawImage>(b).image;
            if (auto *text = std::get_if<DrawText>(&a))
                return text->layout == std::get<DrawText>(b).layout;
            if (auto *vector = std::get_if<DrawVectorGeometry>(&a))
                return vector->geometry == std::get<DrawVectorGeometry>(b).geometry;
            return true;
        };
        for (std::size_t i = 0; i < commands.size(); ++i) {
            if (!batches.empty() && compatible(commands[i - 1], commands[i]))
                ++batches.back().count;
            else
                batches.push_back({i, 1, commands[i].index()});
        }
    }
};
} // namespace v1
} // namespace voxelui
