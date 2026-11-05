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
static std::expected<Mesh, GeometryError> compileWithAtlas(const DrawList &list, GeometryLimits limits,
                                                           Atlas &atlas, VectorMeshesCache &vectors,
                                                           std::uint64_t &tessellations) {
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
    auto polygon = [&](std::vector<Vec2> points, Color color) -> std::string {
        if (!finite(points))
            return "transformed SVG overflow";
        clip(points, clips.back());
        if (points.size() < 3)
            return {};
        if (mesh.vertices.size() + points.size() > limits.vertices ||
            mesh.indices.size() + (points.size() - 2) * 3 > limits.indices)
            return "geometry budget exceeded";
        auto base = std::uint32_t(mesh.vertices.size());
        for (auto point : points)
            mesh.vertices.push_back({point, color});
        for (std::uint32_t i = 1; i + 1 < points.size(); ++i)
            mesh.indices.insert(mesh.indices.end(), {base, base + i, base + i + 1});
        return {};
    };
    auto mappedClip = [&](Rect r, Transform t) {
        auto p = corners(r, t);
        float x = p[0].x, y = p[0].y, right = x, bottom = y;
        for (auto point : p) {
            x = std::min(x, point.x);
            y = std::min(y, point.y);
            right = std::max(right, point.x);
            bottom = std::max(bottom, point.y);
        }
        return intersect(clips.back(), {x, y, right - x, bottom - y});
    };
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
    auto sampled = [&](Rect bounds, Rect region, Color tint) -> std::string {
        const auto first = mesh.vertices.size();
        if (auto error = emit(bounds, {255, 255, 255, 255}); !error.empty())
            return error;
        auto t = transforms.back();
        const float det = t.a * t.d - t.b * t.c;
        if (!std::isfinite(det))
            return "sampled transform overflow";
        for (auto i = first; i < mesh.vertices.size(); ++i) {
            auto &v = mesh.vertices[i];
            v.color = tint;
            v.bounds = bounds;
            v.sample = region;
            v.rounding = {-2, 0};
            v.inverseX = {t.d / det, -t.c / det, (t.c * t.f - t.d * t.e) / det};
            v.inverseY = {-t.b / det, t.a / det, (t.b * t.e - t.a * t.f) / det};
            for (float x : v.inverseX)
                if (!std::isfinite(x))
                    return "sampled inverse overflow";
            for (float x : v.inverseY)
                if (!std::isfinite(x))
                    return "sampled inverse overflow";
        }
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
                        if (auto error = emit(c.bounds, {255, 255, 255, 255}); !error.empty())
                            return error;
                        const auto t = transforms.back();
                        const float det = t.a * t.d - t.b * t.c;
                        if (!std::isfinite(det))
                            return "rounded transform determinant overflow";
                        for (auto i = first; i < mesh.vertices.size(); ++i) {
                            auto &v = mesh.vertices[i];
                            v.inverseX = {t.d / det, -t.c / det, (t.c * t.f - t.d * t.e) / det};
                            v.inverseY = {-t.b / det, t.a / det, (t.b * t.e - t.a * t.f) / det};
                            for (auto value : v.inverseX)
                                if (!std::isfinite(value))
                                    return "rounded inverse overflow";
                            for (auto value : v.inverseY)
                                if (!std::isfinite(value))
                                    return "rounded inverse overflow";
                            v.bounds = c.bounds;
                            v.rounding = {c.radius, c.borderWidth};
                            v.color = c.fill;
                            v.border = c.border;
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
                } else if constexpr (std::same_as<T, DrawImage>) {
                    if (!finite(c.bounds) || !std::isfinite(c.opacity))
                        return "invalid image parameters";
                    if (!c.image)
                        return {};
                    auto &image = *c.image;
                    if (image.width <= 0 || image.height <= 0 || image.width > 2048 || image.height > 2048 ||
                        image.rgba.size() != std::size_t(image.width) * image.height * 4)
                        return "invalid or oversized image";
                    auto region = atlas.insert(c.image, image.width, image.height, image.rgba, false);
                    if (!region)
                        return "atlas capacity exceeded";
                    return sampled(c.bounds, *region,
                                   {255, 255, 255, std::uint8_t(std::clamp(c.opacity, 0.f, 1.f) * 255)});
                } else if constexpr (std::same_as<T, DrawText>) {
                    if (!std::isfinite(c.origin.x) || !std::isfinite(c.origin.y))
                        return "invalid text origin";
                    if (!c.layout)
                        return {};
                    if (c.layout->glyphs.size() > limits.vertices / 4)
                        return "glyph budget exceeded";
                    for (auto &glyph : c.layout->glyphs) {
                        if (!glyph.bitmap)
                            continue;
                        auto &g = *glyph.bitmap;
                        if (g.width == 0 || g.height == 0)
                            continue;
                        if (g.width < 0 || g.height < 0 || g.width > 2048 || g.height > 2048 ||
                            g.alpha.size() != std::size_t(g.width) * g.height)
                            return "invalid glyph mask";
                        auto region = atlas.insert(glyph.bitmap, g.width, g.height, g.alpha, true);
                        if (!region)
                            return "atlas capacity exceeded";
                        if (auto error = sampled(
                                {c.origin.x + glyph.x, c.origin.y + glyph.y, float(g.width), float(g.height)},
                                *region, c.color);
                            !error.empty())
                            return error;
                    }
                    return {};
                } else if constexpr (std::same_as<T, DrawVectorGeometry>) {
                    if (!c.geometry)
                        return {};
                    if (!finite(c.bounds) || !std::isfinite(c.opacity) || !finite(c.geometry->viewBox))
                        return "invalid SVG parameters";
                    auto vb = c.geometry->viewBox;
                    if (vb.w <= 0 || vb.h <= 0 || c.bounds.w <= 0 || c.bounds.h <= 0)
                        return {};
                    if (!vectors.contains(c.geometry.get())) {
                        if (vectors.size() >= 32)
                            vectors.clear();
                        VectorMeshes cached;
                        cached.owner = c.geometry;
                        std::size_t triangles = 0;
                        if (c.geometry->shapes.size() > 10000)
                            return "SVG shape budget exceeded";
                        for (auto &shape : c.geometry->shapes) {
                            auto trianglesForShape = tessellate(shape);
                            if (!trianglesForShape)
                                return trianglesForShape.error();
                            triangles += trianglesForShape->size();
                            if (triangles > 250000)
                                return "SVG document budget exceeded";
                            cached.shapes.push_back(std::move(*trianglesForShape));
                        }
                        vectors.emplace(c.geometry.get(), std::move(cached));
                        ++tessellations;
                    }
                    const auto &cached = vectors.at(c.geometry.get());
                    float k = std::min(c.bounds.w / vb.w, c.bounds.h / vb.h);
                    auto fit =
                        transforms.back() * Transform{k,
                                                      0,
                                                      0,
                                                      k,
                                                      c.bounds.x + (c.bounds.w - vb.w * k) * .5f - vb.x * k,
                                                      c.bounds.y + (c.bounds.h - vb.h * k) * .5f - vb.y * k};
                    if (!finite(fit))
                        return "SVG fit overflow";
                    clips.push_back(mappedClip(c.bounds, transforms.back()));
                    const float opacity = std::clamp(c.opacity, 0.f, 1.f);
                    auto faded = [&](Color color) {
                        color.a = std::uint8_t(color.a * opacity);
                        return color;
                    };
                    for (std::size_t shapeIndex = 0; shapeIndex < c.geometry->shapes.size(); ++shapeIndex) {
                        auto &shape = c.geometry->shapes[shapeIndex];
                        const bool shapeClipped = shape.clip.has_value() && !shape.image;
                        if (shapeClipped) {
                            if (!finite(*shape.clip))
                                return "invalid SVG clip";
                            clips.push_back(mappedClip(*shape.clip, fit));
                        }
                        if (shape.image) {
                            auto &image = *shape.image;
                            if (image.width <= 0 || image.height <= 0 || image.width > 2048 ||
                                image.height > 2048 ||
                                image.rgba.size() != std::size_t(image.width) * image.height * 4)
                                return "invalid SVG image";
                            auto region =
                                atlas.insert(shape.image, image.width, image.height, image.rgba, false);
                            if (!region)
                                return "atlas capacity exceeded";
                            transforms.push_back(fit * shape.imageTransform);
                            auto error = sampled(shape.imageBounds, *region,
                                                 {255, 255, 255, std::uint8_t(opacity * 255)});
                            transforms.pop_back();
                            if (!error.empty())
                                return error;
                        } else {
                            const auto from = fit.apply(shape.gradient.from),
                                       to = fit.apply(shape.gradient.to);
                            if (!finite(std::vector<Vec2>{from, to}))
                                return "invalid SVG gradient";
                            if (shape.fill.a || shape.gradient.enabled)
                                for (auto &triangle : cached.shapes[shapeIndex]) {
                                    auto first = mesh.vertices.size();
                                    if (auto error = polygon({fit.apply(triangle[0]), fit.apply(triangle[1]),
                                                              fit.apply(triangle[2])},
                                                             faded(shape.fill));
                                        !error.empty())
                                        return error;
                                    if (shape.gradient.enabled)
                                        for (auto i = first; i < mesh.vertices.size(); ++i) {
                                            auto &vertex = mesh.vertices[i];
                                            vertex.color = shape.gradient.first;
                                            vertex.border = shape.gradient.last;
                                            vertex.rounding = {-3, opacity};
                                            vertex.sample = {from.x, from.y, to.x, to.y};
                                        }
                                }
                            if (!std::isfinite(shape.strokeWidth))
                                return "invalid SVG stroke";
                            if (shape.stroke.a && shape.strokeWidth > 0) {
                                float radius = shape.strokeWidth *
                                               std::sqrt(std::abs(fit.a * fit.d - fit.b * fit.c)) * .5f;
                                if (!std::isfinite(radius))
                                    return "SVG stroke overflow";
                                transforms.push_back(Transform{});
                                for (auto &contour : shape.contours)
                                    for (std::size_t i = 1; i < contour.size(); ++i) {
                                        auto a = fit.apply(contour[i - 1]), b = fit.apply(contour[i]);
                                        auto first = mesh.vertices.size();
                                        if (auto error = emit({std::min(a.x, b.x) - radius - 1,
                                                               std::min(a.y, b.y) - radius - 1,
                                                               std::abs(a.x - b.x) + 2 * radius + 2,
                                                               std::abs(a.y - b.y) + 2 * radius + 2},
                                                              faded(shape.stroke));
                                            !error.empty())
                                            return error;
                                        for (auto j = first; j < mesh.vertices.size(); ++j) {
                                            auto &vertex = mesh.vertices[j];
                                            vertex.rounding = {-4, radius};
                                            vertex.sample = {a.x, a.y, b.x, b.y};
                                        }
                                    }
                                transforms.pop_back();
                            }
                        }
                        if (shapeClipped)
                            clips.pop_back();
                    }
                    clips.pop_back();
                    return {};
                }
                return {};
            },
            list.commands[command]);
        if (!error.empty())
            return fail(std::move(error));
    }
    if (transforms.size() != 1 || clips.size() != 1)
        return fail("unbalanced draw list stacks");
    mesh.atlas = atlas.page();
    return mesh;
}
std::expected<Mesh, GeometryError> Compiler::compile(const DrawList &list, GeometryLimits limits) {
    // Repack at most once per size, then evict cached entries once at the limit.
    // A frame larger than the bounded page is diagnosed rather than looping.
    for (int attempt = 0; attempt < 5; ++attempt) {
        auto result = compileWithAtlas(list, limits, atlas_, vectors_, tessellations_);
        if (result || result.error().message != "atlas capacity exceeded")
            return result;
        atlas_.clear();
        if (atlas_.size < 2048)
            atlas_.size *= 2;
    }
    return std::unexpected(
        GeometryError{list.commands.size(), "atlas capacity exceeded after bounded repack"});
}
std::expected<Mesh, GeometryError> compileGeometry(const DrawList &list, GeometryLimits limits) {
    Compiler compiler;
    return compiler.compile(list, limits);
}
} // namespace voxelui::vulkan
