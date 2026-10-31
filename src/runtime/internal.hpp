#pragma once
#include <functional>
#include <sstream>
#include <voxelui/runtime.hpp>
namespace voxelui::v1::runtime_detail {

inline std::string bytes(const Resource &r) {
    return {reinterpret_cast<const char *>(r.bytes.data()), r.bytes.size()};
}
inline bool focusable(const LayoutNode &n) {
    auto &t = n.node.tag;
    return n.style.visible && n.node.attr("disabled") != "true" &&
           (t == "Button" || t == "Toggle" || t == "Checkbox" || t == "Slider" || t == "TextInput" ||
            t == "Select");
}
inline const LayoutNode *search(const LayoutNode &n, std::string_view id) {
    if (n.node.id == id)
        return &n;
    for (auto &c : n.children)
        if (auto *result = search(c, id))
            return result;
    return nullptr;
}
inline bool pathTo(const LayoutNode &n, std::string_view id, std::vector<const LayoutNode *> &path) {
    path.push_back(&n);
    if (n.node.id == id)
        return true;
    for (auto &c : n.children)
        if (pathTo(c, id, path))
            return true;
    path.pop_back();
    return false;
}
inline const LayoutNode *modal(const LayoutNode &n) {
    const LayoutNode *result = nullptr;
    if (n.style.visible) {
        if (n.node.tag == "Modal")
            result = &n;
        for (auto &c : n.children)
            if (auto *m = modal(c))
                result = m;
    }
    return result;
}
inline const Value *property(const Value *value, std::string_view path) {
    if (!value)
        return nullptr;
    while (!path.empty()) {
        auto dot = path.find('.');
        auto *obj = std::get_if<Value::Object>(&value->data);
        if (!obj)
            return nullptr;
        auto it = obj->find(std::string(path.substr(0, dot)));
        if (it == obj->end())
            return nullptr;
        value = &it->second;
        if (dot == path.npos)
            break;
        path.remove_prefix(dot + 1);
    }
    return value;
}
inline std::size_t payloadSize(const Node &n) {
    std::size_t bytes = n.id.size() + n.tag.size() + n.text.size();
    for (auto &[key, value] : n.attributes)
        bytes += key.size() + value.size();
    return bytes;
}
inline bool layoutEquivalent(const Style &a, const Style &b) {
    return a.width == b.width && a.height == b.height && a.minWidth == b.minWidth &&
           a.maxWidth == b.maxWidth && a.minHeight == b.minHeight && a.maxHeight == b.maxHeight &&
           a.padding == b.padding && a.margin == b.margin && a.gap == b.gap &&
           a.borderWidth == b.borderWidth && a.font == b.font && a.fontSize == b.fontSize &&
           a.fontWeight == b.fontWeight && a.align == b.align && a.justify == b.justify &&
           a.anchor == b.anchor && a.columns == b.columns && a.visible == b.visible && a.clip == b.clip;
}
inline Color faded(Color c, float opacity) {
    c.a = std::uint8_t(c.a * std::clamp(opacity, 0.f, 1.f));
    return c;
}
inline float numeric(const Node &n, std::string_view key, float fallback = 0) {
    return float(number(n.attr(key)).value_or(fallback));
}
} // namespace voxelui::v1::runtime_detail
