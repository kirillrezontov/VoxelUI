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
};
struct LayoutStats {
    std::size_t passes{}, nodes{};
};
void computeLayout(LayoutNode &, Vec2 viewport, TextSystem &, const Components &,
                   const std::map<std::string, Vec2> &scroll, LayoutStats &);
const LayoutNode *hitTest(const LayoutNode &, Vec2);
} // namespace v1
} // namespace voxelui
