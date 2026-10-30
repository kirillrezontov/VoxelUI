#pragma once
#include "draw_list.hpp"
#include "events.hpp"
#include <functional>
#include <set>
namespace voxelui {
inline namespace v1 {
struct WidgetContext {
    const Node &node;
    Rect bounds;
};
struct WidgetDefinition {
    std::function<Vec2(const Node &, Vec2)> measure;
    std::function<void(const WidgetContext &, DrawList &)> draw;
    std::function<bool(const WidgetContext &, const Event &)> event;
};
template <class T>
concept UiWidgetImplementation =
    requires(T &w, const Node &n, Vec2 available, const WidgetContext &c, DrawList &list) {
        { w.measure(n, available) } -> std::same_as<Vec2>;
        w.draw(c, list);
    };
class Components {
    std::map<std::string, WidgetDefinition> entries_;

  public:
    bool registerWidget(std::string tag, WidgetDefinition definition);
    template <UiWidgetImplementation T> bool registerWidget(std::string tag, std::shared_ptr<T> w) {
        return registerWidget(std::move(tag), {[w](auto &n, auto a) { return w->measure(n, a); },
                                               [w](auto &c, auto &l) { w->draw(c, l); },
                                               {}});
    }
    const WidgetDefinition *get(std::string_view tag) const {
        auto i = entries_.find(std::string(tag));
        return i == entries_.end() ? nullptr : &i->second;
    }
    std::set<std::string> tags() const {
        std::set<std::string> s;
        for (auto &[k, v] : entries_)
            s.insert(k);
        return s;
    }
};
} // namespace v1
} // namespace voxelui
