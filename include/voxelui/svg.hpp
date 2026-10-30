#pragma once
#include "document.hpp"
namespace voxelui {
inline namespace v1 {
struct Gradient {
    Vec2 from{}, to{1, 0};
    Color first{}, last{};
    bool enabled{};
};
struct VectorShape {
    std::vector<std::vector<Vec2>> contours;
    Color fill{0, 0, 0, 255}, stroke{0, 0, 0, 0};
    float strokeWidth{1};
    Gradient gradient;
    std::optional<Rect> clip;
    bool closed{true};
    std::shared_ptr<const Image> image;
    Rect imageBounds;
    Transform imageTransform;
};
struct VectorSlot {
    std::string name, id;
    Transform transform;
    Rect bounds;
};
struct VectorDocument {
    Rect viewBox;
    std::vector<VectorShape> shapes;
    std::vector<VectorSlot> slots;
};
std::shared_ptr<const VectorDocument> parseSvg(const Resource &, Diagnostics &, Limits limits = {},
                                               Resources *resources = nullptr);
class VectorCache {
    std::map<std::string, std::pair<std::uint64_t, std::shared_ptr<const VectorDocument>>> entries_;

  public:
    std::shared_ptr<const VectorDocument> get(const Resource &, Diagnostics &,
                                              Resources *resources = nullptr);
    void clear() {
        entries_.clear();
    }
};
} // namespace v1
} // namespace voxelui
