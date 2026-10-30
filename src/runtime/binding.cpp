#include <charconv>
#include <voxelui/actions.hpp>
#include <voxelui/components.hpp>
namespace voxelui {
inline namespace v1 {
std::string Value::string() const {
    if (auto v = std::get_if<std::string>(&data))
        return *v;
    if (auto v = std::get_if<bool>(&data))
        return *v ? "true" : "false";
    if (auto v = std::get_if<double>(&data)) {
        char b[64];
        auto [p, e] = std::to_chars(b, b + 64, *v);
        if (e == std::errc{})
            return {b, p};
    }
    return {};
}
std::optional<double> Value::numeric() const {
    if (auto p = std::get_if<double>(&data))
        return *p;
    return {};
}
bool Value::truth() const {
    if (auto p = std::get_if<bool>(&data))
        return *p;
    if (auto p = numeric())
        return *p != 0;
    return !string().empty();
}
bool validBindingPath(std::string_view p) {
    if (p.empty() || p.size() > 256 || p.front() == '.' || p.back() == '.' || p.find("..") != p.npos)
        return false;
    for (char c : p)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '.')
            return false;
    return true;
}
std::optional<std::string> bindingPath(std::string_view s) {
    if (s.size() >= 3 && s.front() == '{' && s.back() == '}') {
        auto p = trim(s.substr(1, s.size() - 2));
        if (validBindingPath(p))
            return p;
    }
    return {};
}
const Value *DataModel::get(std::string_view path) const {
    if (!validBindingPath(path))
        return nullptr;
    auto *v = &root_;
    while (!path.empty()) {
        auto dot = path.find('.');
        auto *obj = std::get_if<Value::Object>(&v->data);
        if (!obj)
            return nullptr;
        auto it = obj->find(std::string(path.substr(0, dot)));
        if (it == obj->end())
            return nullptr;
        v = &it->second;
        if (dot == path.npos)
            break;
        path.remove_prefix(dot + 1);
    }
    return v;
}
bool DataModel::set(std::string_view path, Value value) {
    if (!validBindingPath(path))
        return false;
    if (auto old = get(path); old && *old == value)
        return true;
    auto original = std::string(path);
    Value *v = &root_;
    while (!path.empty()) {
        auto dot = path.find('.');
        auto *obj = std::get_if<Value::Object>(&v->data);
        if (!obj)
            return false;
        auto key = std::string(path.substr(0, dot));
        if (dot == path.npos) {
            (*obj)[key] = std::move(value);
            break;
        }
        auto [it, added] = obj->try_emplace(key, Value::Object{});
        (void)added;
        v = &it->second;
        path.remove_prefix(dot + 1);
    }
    ++revision_;
    changed_.push_back(original);
    return true;
}
bool matchesType(const Value &v, ValueType t) {
    switch (t) {
    case ValueType::boolean:
        return std::holds_alternative<bool>(v.data);
    case ValueType::number:
        return std::holds_alternative<double>(v.data);
    case ValueType::string:
        return std::holds_alternative<std::string>(v.data);
    case ValueType::object:
        return std::holds_alternative<Value::Object>(v.data);
    case ValueType::array:
        return std::holds_alternative<Value::Array>(v.data);
    }
    return false;
}
bool Actions::registerAction(std::string id, ActionDefinition a) {
    if (!validResourceId(id) || !a.callback)
        return false;
    return entries_.emplace(std::move(id), std::move(a)).second;
}
std::optional<std::string> Actions::validate(const ActionContext &c, bool capabilities) const {
    auto it = entries_.find(c.action);
    if (it == entries_.end())
        return "Unknown action: " + c.action;
    auto &definition = it->second;
    if (capabilities && !definition.capability.empty() && !grants_.contains(definition.capability))
        return "Missing capability: " + definition.capability;
    for (auto &[name, type] : definition.parameters) {
        auto v = c.parameters.find(name);
        if (v == c.parameters.end() || !matchesType(v->second, type))
            return "Missing or incorrectly typed action parameter: " + name;
    }
    return {};
}
std::optional<std::string> Actions::invoke(const ActionContext &c) const {
    if (auto error = validate(c, true))
        return error;
    auto definition = entries_.at(c.action);
    try {
        return definition.callback(c);
    } catch (const std::exception &e) {
        return std::string("Action failed: ") + e.what();
    } catch (...) {
        return "Action failed with unknown exception";
    }
}
bool Components::registerWidget(std::string tag, WidgetDefinition d) {
    if (tag.empty() || builtinTag(tag) || !d.measure || !d.draw)
        return false;
    return entries_.emplace(std::move(tag), std::move(d)).second;
}
} // namespace v1
} // namespace voxelui
