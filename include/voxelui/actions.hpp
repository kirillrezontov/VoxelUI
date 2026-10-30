#pragma once
#include "binding.hpp"
#include <set>
namespace voxelui {
inline namespace v1 {
enum class ValueType { boolean, number, string, object, array };
bool matchesType(const Value &, ValueType);
struct ActionContext {
    std::string elementId, action;
    std::map<std::string, Value> parameters;
};
struct ActionDefinition {
    std::map<std::string, ValueType> parameters;
    std::string capability;
    std::function<std::optional<std::string>(const ActionContext &)> callback;
};
class Actions {
    std::map<std::string, ActionDefinition> entries_;
    std::set<std::string> grants_;

  public:
    bool registerAction(std::string id, ActionDefinition action);
    void unregisterAction(std::string_view id) {
        entries_.erase(std::string(id));
    }
    void grant(std::string capability) {
        grants_.insert(std::move(capability));
    }
    bool contains(std::string_view id) const {
        return entries_.contains(std::string(id));
    }
    std::optional<std::string> validate(const ActionContext &, bool capabilities = false) const;
    std::optional<std::string> invoke(const ActionContext &) const;
};
} // namespace v1
} // namespace voxelui
