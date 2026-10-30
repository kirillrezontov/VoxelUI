#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace voxelui {
inline namespace v1 {
inline constexpr unsigned api_version = 1;
struct Vec2 {
    float x{}, y{};
};
struct Rect {
    float x{}, y{}, w{}, h{};
    bool contains(Vec2 p) const {
        return w > 0 && h > 0 && p.x >= x && p.y >= y && p.x < x + w && p.y < y + h;
    }
};
inline Rect intersect(Rect a, Rect b) {
    float x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0.f, std::min(a.x + a.w, b.x + b.w) - x),
            std::max(0.f, std::min(a.y + a.h, b.y + b.h) - y)};
}
struct Color {
    std::uint8_t r{}, g{}, b{}, a{255};
    bool operator==(const Color &) const = default;
};
struct Transform {
    float a{1}, b{}, c{}, d{1}, e{}, f{};
    Vec2 apply(Vec2 p) const {
        return {a * p.x + c * p.y + e, b * p.x + d * p.y + f};
    }
    Transform operator*(const Transform &t) const {
        return {a * t.a + c * t.b, b * t.a + d * t.b,     a * t.c + c * t.d,
                b * t.c + d * t.d, a * t.e + c * t.f + e, b * t.e + d * t.f + f};
    }
};
enum class Severity { warning, error };
struct Diagnostic {
    Severity severity{Severity::error};
    std::string source;
    int line{1}, column{1};
    std::string message;
};
using Diagnostics = std::vector<Diagnostic>;
inline bool hasErrors(const Diagnostics &d) {
    return std::ranges::any_of(d, [](auto &x) { return x.severity == Severity::error; });
}
struct Limits {
    std::size_t bytes = 4 * 1024 * 1024, elements = 10000, attributes = 64, depth = 64, geometry = 100000;
};
std::optional<double> number(std::string_view text);
std::string trim(std::string_view text);
} // namespace v1
} // namespace voxelui
