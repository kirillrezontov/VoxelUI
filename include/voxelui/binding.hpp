#pragma once
#include "types.hpp"
#include <functional>
#include <variant>
namespace voxelui {
inline namespace v1 {
struct Value {
    using Object = std::map<std::string, Value>;
    using Array = std::vector<Value>;
    std::variant<std::monostate, bool, double, std::string, Object, Array> data;
    Value() = default;
    Value(bool v) : data(v) {}
    Value(double v) : data(v) {}
    Value(int v) : data(double(v)) {}
    Value(std::string v) : data(std::move(v)) {}
    Value(const char *v) : data(std::string(v)) {}
    Value(Object v) : data(std::move(v)) {}
    Value(Array v) : data(std::move(v)) {}
    bool operator==(const Value &) const = default;
    std::string string() const;
    std::optional<double> numeric() const;
    bool truth() const;
};
bool validBindingPath(std::string_view);
class DataModel {
    Value root_{Value::Object{}};
    std::uint64_t revision_{};
    std::vector<std::string> changed_;

  public:
    const Value *get(std::string_view path) const;
    bool set(std::string_view path, Value value);
    std::uint64_t revision() const {
        return revision_;
    }
    const std::vector<std::string> &changes() const {
        return changed_;
    }
    void acknowledge() {
        changed_.clear();
    }
};
std::optional<std::string> bindingPath(std::string_view);
} // namespace v1
} // namespace voxelui
