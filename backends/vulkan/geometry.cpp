#include "geometry.hpp"
#include <array>
#include <limits>

namespace voxelui::vulkan {
namespace {
bool finite(Transform t) {
    return std::isfinite(t.a) && std::isfinite(t.b) && std::isfinite(t.c) && std::isfinite(t.d) &&
           std::isfinite(t.e) && std::isfinite(t.f);
}
bool finite(Rect r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h) &&
           std::isfinite(r.x + r.w) && std::isfinite(r.y + r.h);
}
std::vector<Vec2> corners(Rect r, Transform t) {
    return {t.apply({r.x, r.y}), t.apply({r.x + r.w, r.y}), t.apply({r.x + r.w, r.y + r.h}),
            t.apply({r.x, r.y + r.h})};
}
bool finite(const std::vector<Vec2> &points) {
    return std::ranges::all_of(points, [](Vec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); });
}
// Sutherland-Hodgman against framebuffer clip edges. Input is a convex quad;
// the resulting polygon remains convex, so triangulating it as a fan is valid.
void clip(std::vector<Vec2> &points, Rect r) {
    for (int edge = 0; edge < 4 && !points.empty(); ++edge) {
        auto distance = [&](Vec2 p) -> double {
            switch (edge) {
            case 0:
                return double(p.x) - r.x;
            case 1:
                return double(r.x) + r.w - p.x;
            case 2:
                return double(p.y) - r.y;
            default:
                return double(r.y) + r.h - p.y;
            }
        };
        std::vector<Vec2> output;
        auto a = points.back();
        double da = distance(a);
        for (auto b : points) {
            double db = distance(b);
            if ((da >= 0) != (db >= 0)) {
                double t = da / (da - db);
                output.push_back({float(double(a.x) + (double(b.x) - a.x) * t),
                                  float(double(a.y) + (double(b.y) - a.y) * t)});
            }
            if (db >= 0)
                output.push_back(b);
            a = b;
            da = db;
        }
        points = std::move(output);
    }
}
} // namespace
std::expected<Mesh, GeometryError> compileGeometry(const DrawList &list, GeometryLimits limits) {
    std::size_t command = list.commands.size();
    auto fail = [&](std::string message) -> std::expected<Mesh, GeometryError> {
        return std::unexpected(GeometryError{command, std::move(message)});
    };
    if (list.commands.size() > limits.commands)
        return fail("command budget exceeded");
    if (!std::isfinite(list.framebufferScale) || !std::isfinite(list.logicalSize.x) ||
        !std::isfinite(list.logicalSize.y) || list.logicalSize.x <= 0 || list.logicalSize.y <= 0)
        return fail("invalid viewport or framebuffer scale");
    const float dpi = std::clamp(list.framebufferScale, .25f, 8.f);
    const double width = std::ceil(double(list.logicalSize.x) * dpi);
    const double height = std::ceil(double(list.logicalSize.y) * dpi);
    if (width > 8192 || height > 8192 || width * height > 16000000)
        return fail("viewport exceeds reference framebuffer limits");
    std::vector<Transform> transforms{{dpi, 0, 0, dpi, 0, 0}};
    std::vector<Rect> clips{{0, 0, float(width), float(height)}};
    Mesh mesh;
    // Return a diagnostic even for malformed geometry outside the current clip.
    auto emit = [&](Rect r, Color color) -> std::string {
        if (!finite(r))
            return "nonfinite rectangle";
        auto points = corners(r, transforms.back());
        if (!finite(points))
            return "transformed rectangle overflow";
        if (r.w <= 0 || r.h <= 0 || !color.a || clips.back().w <= 0 || clips.back().h <= 0)
            return {};
        auto t = transforms.back();
        double det = double(t.a) * t.d - double(t.b) * t.c;
        if (std::abs(det) < 1e-8)
            return {}; // reference's singular-transform policy
        clip(points, clips.back());
        if (points.size() < 3)
            return {};
        const auto count = (points.size() - 2) * 3;
        if (points.size() > limits.vertices - std::min(limits.vertices, mesh.vertices.size()) ||
            count > limits.indices - std::min(limits.indices, mesh.indices.size()) ||
            mesh.vertices.size() + points.size() > std::numeric_limits<std::uint32_t>::max())
            return "geometry budget exceeded";
        auto base = std::uint32_t(mesh.vertices.size());
        for (auto p : points)
            mesh.vertices.push_back({p, color});
        for (std::uint32_t i = 1; i + 1 < points.size(); ++i)
            mesh.indices.insert(mesh.indices.end(), {base, base + i, base + i + 1});
        return {};
    };
    for (command = 0; command < list.commands.size(); ++command) {
        auto error = std::visit(
            [&](const auto &c) -> std::string {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::same_as<T, DrawRectangle>) {
                    if (!finite(c.bounds) || !std::isfinite(c.borderWidth) || !std::isfinite(c.radius) ||
                        c.borderWidth < 0 || c.radius < 0)
                        return "invalid rectangle parameters";
                    if (c.radius != 0) {
                        const auto first = mesh.vertices.size();
                        // Emit conservative geometry. Fragment coverage evaluates the
                        // same local-space rounded bounds as the software reference.
                        if (auto error = emit(c.bounds, {255,255,255,255}); !error.empty())
                            return error;
                        const auto t = transforms.back();
                        const float det=t.a*t.d-t.b*t.c;
                        if (!std::isfinite(det)) return "rounded transform determinant overflow";
                        for (auto i=first; i<mesh.vertices.size(); ++i) {
                            auto& v=mesh.vertices[i];
                            v.inverseX={t.d/det,-t.c/det,(t.c*t.f-t.d*t.e)/det};
                            v.inverseY={-t.b/det,t.a/det,(t.b*t.e-t.a*t.f)/det};
                            for(auto value:v.inverseX) if(!std::isfinite(value)) return "rounded inverse overflow";
                            for(auto value:v.inverseY) if(!std::isfinite(value)) return "rounded inverse overflow";
                            v.bounds=c.bounds;v.rounding={c.radius,c.borderWidth};
                            v.color=c.fill;v.border=c.border;
                        }
                        return {};
                    }
                    auto r = c.bounds;
                    if (r.w <= 0 || r.h <= 0)
                        return {};
                    float b = std::min({c.borderWidth, r.w * .5f, r.h * .5f});
                    if (c.borderWidth >= std::min(r.w, r.h) * .5f)
                        return emit(r, c.border);
                    if (b == 0)
                        return emit(r, c.fill);
                    // Disjoint strips avoid double blending transparent borders/corners.
                    for (auto strip : std::array<Rect, 4>{{{r.x, r.y, r.w, b},
                                                           {r.x, r.y + r.h - b, r.w, b},
                                                           {r.x, r.y + b, b, r.h - 2 * b},
                                                           {r.x + r.w - b, r.y + b, b, r.h - 2 * b}}})
                        if (auto e = emit(strip, c.border); !e.empty())
                            return e;
                    return emit({r.x + b, r.y + b, r.w - 2 * b, r.h - 2 * b}, c.fill);
                } else if constexpr (std::same_as<T, PushTransform>) {
                    if (transforms.size() - 1 >= limits.depth)
                        return "transform depth exceeded";
                    auto t = transforms.back() * c.transform;
                    if (!finite(c.transform) || !finite(t))
                        return "nonfinite transform";
                    transforms.push_back(t);
                } else if constexpr (std::same_as<T, PopTransform>) {
                    if (transforms.size() == 1)
                        return "transform stack underflow";
                    transforms.pop_back();
                } else if constexpr (std::same_as<T, PushClip>) {
                    if (clips.size() - 1 >= limits.depth)
                        return "clip depth exceeded";
                    if (!finite(c.bounds))
                        return "nonfinite clip";
                    auto points = corners(c.bounds, transforms.back());
                    if (!finite(points))
                        return "transformed clip overflow";
                    if (c.bounds.w <= 0 || c.bounds.h <= 0) {
                        clips.push_back({});
                        return {};
                    }
                    float x = points[0].x, y = points[0].y, right = x, bottom = y;
                    for (auto p : points) {
                        x = std::min(x, p.x);
                        y = std::min(y, p.y);
                        right = std::max(right, p.x);
                        bottom = std::max(bottom, p.y);
                    }
                    // v1 software clips use transformed axis-aligned bounds, including rotations.
                    Rect mapped{x, y, right - x, bottom - y};
                    if (!finite(mapped))
                        return "transformed clip overflow";
                    clips.push_back(intersect(clips.back(), mapped));
                } else if constexpr (std::same_as<T, PopClip>) {
                    if (clips.size() == 1)
                        return "clip stack underflow";
                    clips.pop_back();
                } else if constexpr (std::same_as<T, DrawImage>)
                    return "images are not implemented";
                else if constexpr (std::same_as<T, DrawText>)
                    return "glyph atlas is not implemented";
                else if constexpr (std::same_as<T, DrawVectorGeometry>)
                    return "SVG tessellation is not implemented";
                return {};
            },
            list.commands[command]);
        if (!error.empty())
            return fail(std::move(error));
    }
    if (transforms.size() != 1 || clips.size() != 1)
        return fail("unbalanced draw list stacks");
    return mesh;
}
} // namespace voxelui::vulkan
