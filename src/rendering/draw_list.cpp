#include "../runtime/internal.hpp"
namespace voxelui {
inline namespace v1 {
using namespace runtime_detail;
void Runtime::paint(const LayoutNode &n, DrawList &list) {
    if (!n.style.visible)
        return;
    auto &s = n.style;
    auto &b = n.bounds;
    auto fill = faded(s.background, s.opacity), border = faded(s.border, s.opacity);
    if (fill.a || s.borderWidth > 0)
        list.commands.emplace_back(DrawRectangle{b, fill, border, s.borderWidth, s.radius});
    if (s.clip)
        list.commands.emplace_back(PushClip{n.clip});
    auto *state = this->state(n.node.id);
    float p = s.padding + s.borderWidth;
    Rect content{b.x + p, b.y + p, std::max(0.f, b.w - 2 * p), std::max(0.f, b.h - 2 * p)};
    if (n.node.tag == "ProgressBar" || n.node.tag == "Slider") {
        list.commands.emplace_back(DrawRectangle{b, {40, 49, 62, 255}, {}, 0, 4});
        float min = numeric(n.node, "min"), max = std::max(min + .001f, numeric(n.node, "max", 1));
        float value = n.node.tag == "Slider" && state ? float(state->value) : numeric(n.node, "value");
        float ratio = std::clamp((value - min) / (max - min), 0.f, 1.f);
        list.commands.emplace_back(
            DrawRectangle{{b.x, b.y, b.w * ratio, b.h}, faded(s.foreground, s.opacity), {}, 0, 4});
    }
    if (n.node.tag == "Checkbox" || n.node.tag == "Toggle") {
        list.commands.emplace_back(
            DrawRectangle{b, {36, 44, 60, 255}, s.foreground, 1, n.node.tag == "Toggle" ? b.h * .5f : 4});
        if (state && state->checked)
            list.commands.emplace_back(
                DrawRectangle{{b.x + 5, b.y + 5, std::max(0.f, b.w - 10), std::max(0.f, b.h - 10)},
                              s.foreground,
                              {},
                              0,
                              3});
    }
    if (n.node.tag == "Image") {
        auto it = images_.find(n.node.attr("source"));
        if (it != images_.end()) {
            Rect target = content;
            auto &img = *it->second;
            if (s.fit != "stretch") {
                float k = s.fit == "cover" ? std::max(content.w / img.width, content.h / img.height)
                                           : std::min(content.w / img.width, content.h / img.height);
                target = {content.x + (content.w - img.width * k) * .5f,
                          content.y + (content.h - img.height * k) * .5f, img.width * k, img.height * k};
            }
            list.commands.emplace_back(PushClip{content});
            list.commands.emplace_back(DrawImage{target, it->second, s.opacity});
            list.commands.emplace_back(PopClip{});
        }
    }
    if (n.node.tag == "Vector") {
        if (auto it = vectorDocuments_.find(n.node.attr("source")); it != vectorDocuments_.end()) {
            auto vector = it->second;
            if (vector) {
                list.commands.emplace_back(DrawVectorGeometry{content, vector, s.opacity});
                auto vb = vector->viewBox;
                float k = std::min(content.w / vb.w, content.h / vb.h);
                Transform fit{k,
                              0,
                              0,
                              k,
                              content.x + (content.w - vb.w * k) * .5f - vb.x * k,
                              content.y + (content.h - vb.h * k) * .5f - vb.y * k};
                for (auto &slot : vector->slots)
                    if (auto it = slots_.find(slot.name); it != slots_.end()) {
                        list.commands.emplace_back(PushClip{content});
                        list.commands.emplace_back(PushTransform{fit * slot.transform});
                        Node slotNode;
                        slotNode.id = n.node.id + "/" + slot.id;
                        slotNode.tag = "Slot";
                        list.commands.emplace_back(PushClip{slot.bounds});
                        auto definition = it->second;
                        if (definition.draw)
                            definition.draw({slotNode, slot.bounds}, list);
                        list.commands.emplace_back(PopClip{});
                        list.commands.emplace_back(PopTransform{});
                        list.commands.emplace_back(PopClip{});
                    }
            }
        }
    }
    if (n.node.tag == "Text" || n.node.tag == "TextInput" || n.node.tag == "Select" ||
        (n.node.tag == "Button" && n.children.empty())) {
        bool input = n.node.tag == "TextInput";
        std::string value = input && state                    ? state->text
                            : n.node.tag == "Select" && state ? state->text
                                                              : n.node.attr("value", n.node.text);
        if (input && state)
            value += state->composition;
        if (value.empty() && input)
            value = n.node.attr("placeholder");
        auto layout = text_.layout(value, s.font, s.fontSize, content.w, s.textAlign, s.fontWeight);
        float y = content.y;
        if (s.verticalAlign == "center")
            y += std::max(0.f, (content.h - layout->size.y) * .5f);
        if (s.verticalAlign == "bottom")
            y += std::max(0.f, content.h - layout->size.y);
        list.commands.emplace_back(PushClip{content});
        if (input && state && focus_ == n.node.id) {
            float x1 = text_
                           .layout(
                               std::string_view(state->text).substr(0, std::min(state->caret, state->anchor)),
                               s.font, s.fontSize)
                           ->size.x,
                  x2 = text_
                           .layout(
                               std::string_view(state->text).substr(0, std::max(state->caret, state->anchor)),
                               s.font, s.fontSize)
                           ->size.x;
            if (x2 > x1)
                list.commands.emplace_back(DrawRectangle{
                    {content.x + x1, y, x2 - x1, s.fontSize * 1.3f}, {63, 109, 170, 170}, {}, 0, 0});
            float cx =
                text_.layout(std::string_view(state->text).substr(0, state->caret), s.font, s.fontSize)
                    ->size.x;
            list.commands.emplace_back(
                DrawRectangle{{content.x + cx, y, 1, s.fontSize * 1.3f}, s.foreground, {}, 0, 0});
        }
        list.commands.emplace_back(DrawText{{content.x, y}, layout, faded(s.foreground, s.opacity)});
        list.commands.emplace_back(PopClip{});
    }
    if (auto *custom = components_.get(n.node.tag)) {
        auto definition = *custom;
        definition.draw({n.node, b}, list);
    }
    for (auto &c : n.children)
        paint(c, list);
    if (s.clip)
        list.commands.emplace_back(PopClip{});
    if (focus_ == n.node.id)
        list.commands.emplace_back(DrawRectangle{b, {0, 0, 0, 0}, {103, 188, 255, 255}, 2, s.radius});
}
DrawList Runtime::buildDrawList(bool debug) {
    auto currentFrame = frame_;
    currentFrame.deltaSeconds = 0;
    update(currentFrame);
    auto treeLifetime = tree_;
    DrawList list;
    list.logicalSize = frame_.viewport;
    list.framebufferScale = frame_.dpiScale;
    list.commands.emplace_back(PushTransform{{frame_.uiScale, 0, 0, frame_.uiScale, 0, 0}});
    if (treeLifetime)
        paint(*treeLifetime, list);
    if (debug && tree_) {
        std::function<void(const LayoutNode &)> outline = [&](const LayoutNode &n) {
            list.commands.emplace_back(DrawRectangle{n.bounds, {0, 0, 0, 0}, {255, 80, 120, 180}, 1, 0});
            auto label = n.node.id + " [" + std::to_string(int(n.bounds.x)) + "," +
                         std::to_string(int(n.bounds.y)) + " " + std::to_string(int(n.bounds.w)) + "x" +
                         std::to_string(int(n.bounds.h)) + "]";
            list.commands.emplace_back(
                DrawText{{n.bounds.x, n.bounds.y}, text_.layout(label, "ui:font", 10), {255, 210, 100, 255}});
            for (auto &c : n.children)
                outline(c);
        };
        outline(*tree_);
    }
    if (!hover_.empty())
        if (auto *hovered = find(hover_)) {
            auto tip = hovered->node.attr("tooltip");
            if (!tip.empty()) {
                auto layout = text_.layout(tip, "ui:font", 14, 300);
                Rect b{hovered->bounds.x, hovered->bounds.y + hovered->bounds.h + 4, layout->size.x + 16,
                       layout->size.y + 12};
                list.commands.emplace_back(DrawRectangle{b, {12, 17, 26, 245}, {100, 120, 145, 255}, 1, 4});
                list.commands.emplace_back(DrawText{{b.x + 8, b.y + 6}, layout, {255, 255, 255, 255}});
            }
        }
    list.commands.emplace_back(PopTransform{});
    list.rebuildBatches();
    return list;
}
} // namespace v1
} // namespace voxelui
