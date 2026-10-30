#include <functional>
#include <voxelui/layout.hpp>
namespace voxelui {
inline namespace v1 {
namespace {
float constrained(float value, Length minimum, Length maximum, float basis) {
    float low = minimum.resolve(basis, 0), high = std::max(low, maximum.resolve(basis, 100000));
    return std::clamp(value, low, high);
}
bool column(const LayoutNode &n) {
    return n.node.tag == "Column" || n.node.tag == "ScrollView" || n.node.tag == "Screen" ||
           n.node.tag == "Modal";
}
bool overlay(const LayoutNode &n) {
    return n.node.tag == "Stack" || n.node.tag == "Panel" || n.node.tag == "Tooltip";
}
Vec2 measure(LayoutNode &n, Vec2 available, TextSystem &text, const Components &components,
             LayoutStats &stats) {
    ++stats.nodes;
    auto &s = n.style;
    if (!s.visible) {
        n.bounds.w = n.bounds.h = 0;
        return {};
    }
    float p = s.padding + s.borderWidth;
    Vec2 inner{std::max(0.f, s.width.resolve(available.x, available.x) - 2 * p),
               std::max(0.f, s.height.resolve(available.y, available.y) - 2 * p)};
    Vec2 natural{};
    if (n.node.tag == "Text" || n.node.tag == "TextInput" || n.node.tag == "Button" ||
        n.node.tag == "Select") {
        auto value = n.node.attr("value", n.node.text);
        auto l = text.layout(value, s.font, s.fontSize, inner.x, "left", s.fontWeight);
        natural = l->size;
        if (n.node.tag == "TextInput")
            natural.x = std::max(natural.x, 160.f);
    }
    if (n.node.tag == "Image" || n.node.tag == "Vector")
        natural = {96, 96};
    if (n.node.tag == "Slider" || n.node.tag == "ProgressBar")
        natural = {160, 24};
    if (n.node.tag == "Toggle" || n.node.tag == "Checkbox")
        natural = {28, 28};
    if (auto *custom = components.get(n.node.tag))
        natural = custom->measure(n.node, inner);
    int index = 0;
    int cols = std::clamp(s.columns, 1, 1000);
    if (auto v = number(n.node.attr("columns")))
        cols = std::clamp(int(*v), 1, 1000);
    float gridRow = 0;
    Vec2 accumulated{};
    for (auto &c : n.children) {
        auto size = measure(
            c,
            {n.node.tag == "Grid" ? std::max(0.f, (inner.x - s.gap * (cols - 1)) / cols) : inner.x, inner.y},
            text, components, stats);
        if (!c.style.visible || !c.style.anchor.empty())
            continue;
        float m = 2 * c.style.margin;
        size.x += m;
        size.y += m;
        if (n.node.tag == "Row") {
            accumulated.x += size.x + (index ? s.gap : 0);
            accumulated.y = std::max(accumulated.y, size.y);
        } else if (n.node.tag == "Grid") {
            accumulated.x = inner.x;
            gridRow = std::max(gridRow, size.y);
            if ((index + 1) % cols == 0) {
                accumulated.y += gridRow + s.gap;
                gridRow = 0;
            }
        } else if (column(n)) {
            accumulated.x = std::max(accumulated.x, size.x);
            accumulated.y += size.y + (index ? s.gap : 0);
        } else {
            accumulated.x = std::max(accumulated.x, size.x);
            accumulated.y = std::max(accumulated.y, size.y);
        }
        ++index;
    }
    if (n.node.tag == "Grid") {
        if (index % cols)
            accumulated.y += gridRow;
        else if (index)
            accumulated.y -= s.gap;
    }
    natural = {std::max(natural.x, accumulated.x), std::max(natural.y, accumulated.y)};
    n.contentSize = natural;
    float w = s.width.resolve(available.x, natural.x + 2 * p),
          h = s.height.resolve(available.y, natural.y + 2 * p);
    if (n.node.tag == "Screen") {
        w = available.x;
        h = available.y;
    }
    n.bounds.w = constrained(w, s.minWidth, s.maxWidth, available.x);
    n.bounds.h = constrained(h, s.minHeight, s.maxHeight, available.y);
    return {n.bounds.w, n.bounds.h};
}
void place(LayoutNode &n, Vec2 pos, Rect clip, const std::map<std::string, Vec2> &scroll) {
    auto &s = n.style;
    n.bounds.x = pos.x;
    n.bounds.y = pos.y;
    n.clip = intersect(clip, n.bounds);
    float p = s.padding + s.borderWidth;
    Rect inner{pos.x + p, pos.y + p, std::max(0.f, n.bounds.w - 2 * p), std::max(0.f, n.bounds.h - 2 * p)};
    Rect childClip = s.clip ? intersect(clip, inner) : clip;
    Vec2 offset{};
    if (auto it = scroll.find(n.node.id); it != scroll.end())
        offset = {std::clamp(it->second.x, 0.f, std::max(0.f, n.contentSize.x - inner.w)),
                  std::clamp(it->second.y, 0.f, std::max(0.f, n.contentSize.y - inner.h))};
    float cursor = 0;
    float total = 0;
    int count = 0;
    for (auto &c : n.children)
        if (c.style.visible && c.style.anchor.empty()) {
            total += (n.node.tag == "Row" ? c.bounds.w : c.bounds.h) + 2 * c.style.margin;
            ++count;
        }
    total += std::max(0, count - 1) * s.gap;
    float extent = n.node.tag == "Row" ? inner.w : inner.h;
    if (s.justify == "center")
        cursor = std::max(0.f, (extent - total) * .5f);
    if (s.justify == "end")
        cursor = std::max(0.f, extent - total);
    int cols = std::clamp(s.columns, 1, 1000);
    if (auto v = number(n.node.attr("columns")))
        cols = std::clamp(int(*v), 1, 1000);
    int index = 0;
    float gridY = 0, rowH = 0;
    for (auto &c : n.children) {
        if (!c.style.visible)
            continue;
        float m = c.style.margin;
        Vec2 at{inner.x + m, inner.y + m};
        if (!c.style.anchor.empty()) {
            auto a = c.style.anchor;
            if (a == "center") {
                at.x = inner.x + (inner.w - c.bounds.w) * .5f;
                at.y = inner.y + (inner.h - c.bounds.h) * .5f;
            } else {
                if (a.ends_with("right"))
                    at.x = inner.x + inner.w - c.bounds.w - m;
                if (a.starts_with("bottom"))
                    at.y = inner.y + inner.h - c.bounds.h - m;
            }
        } else if (n.node.tag == "Grid") {
            float cell = std::max(0.f, (inner.w - s.gap * (cols - 1)) / cols);
            at.x += float(index % cols) * (cell + s.gap);
            at.y += gridY;
            if (c.style.width.unit == Unit::automatic)
                c.bounds.w = std::max(0.f, cell - 2 * m);
            rowH = std::max(rowH, c.bounds.h + 2 * m);
            if ((index + 1) % cols == 0) {
                gridY += rowH + s.gap;
                rowH = 0;
            }
            ++index;
        } else if (n.node.tag == "Row") {
            at.x += cursor;
            float free = inner.h - c.bounds.h - 2 * m;
            if (s.align == "center")
                at.y += std::max(0.f, free * .5f);
            if (s.align == "end")
                at.y += std::max(0.f, free);
            if (s.align == "stretch" && c.style.height.unit == Unit::automatic)
                c.bounds.h = constrained(std::max(0.f, inner.h - 2 * m), c.style.minHeight, c.style.maxHeight,
                                         inner.h);
            cursor += c.bounds.w + 2 * m + s.gap;
        } else if (column(n)) {
            at.y += cursor;
            float free = inner.w - c.bounds.w - 2 * m;
            if (s.align == "center")
                at.x += std::max(0.f, free * .5f);
            if (s.align == "end")
                at.x += std::max(0.f, free);
            if (s.align == "stretch" && c.style.width.unit == Unit::automatic)
                c.bounds.w =
                    constrained(std::max(0.f, inner.w - 2 * m), c.style.minWidth, c.style.maxWidth, inner.w);
            cursor += c.bounds.h + 2 * m + s.gap;
        } else if (overlay(n) && s.align == "stretch") {
            if (c.style.width.unit == Unit::automatic)
                c.bounds.w = std::max(0.f, inner.w - 2 * m);
            if (c.style.height.unit == Unit::automatic)
                c.bounds.h = std::max(0.f, inner.h - 2 * m);
        }
        at.x -= offset.x;
        at.y -= offset.y;
        place(c, at, childClip, scroll);
    }
}
} // namespace
void computeLayout(LayoutNode &root, Vec2 viewport, TextSystem &text, const Components &components,
                   const std::map<std::string, Vec2> &scroll, LayoutStats &stats) {
    ++stats.passes;
    stats.nodes = 0;
    measure(root, viewport, text, components, stats);
    place(root, {0, 0}, {0, 0, viewport.x, viewport.y}, scroll);
}
const LayoutNode *hitTest(const LayoutNode &n, Vec2 p) {
    if (!n.style.visible)
        return nullptr;
    if (n.style.clip && !n.clip.contains(p))
        return nullptr;
    for (auto i = n.children.rbegin(); i != n.children.rend(); ++i)
        if (auto *hit = hitTest(*i, p))
            return hit;
    return n.clip.contains(p) ? &n : nullptr;
}
} // namespace v1
} // namespace voxelui
