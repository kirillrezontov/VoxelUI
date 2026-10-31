#pragma once
#include "components.hpp"
#include "style.hpp"
namespace voxelui {
inline namespace v1 {
struct LayoutNode {
    Node node;
    Style style;
    Rect bounds, clip;
    Vec2 contentSize;
    std::vector<LayoutNode> children;
    // Measurement cache. Public computeLayout defaults to full recomputation.
    bool measureDirty{true};
    Vec2 measuredAvailable{}, measuredSize{};
};
struct LayoutStats {
    std::size_t passes{}, nodes{}, placed{};
};
void computeLayout(LayoutNode &, Vec2 viewport, TextSystem &, const Components &,
                   const std::map<std::string, Vec2> &scroll, LayoutStats &, bool incremental = false);
const LayoutNode *hitTest(const LayoutNode &, Vec2);
} // namespace v1
} // namespace voxelui
