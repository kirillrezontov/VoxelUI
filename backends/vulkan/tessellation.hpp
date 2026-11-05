#pragma once
#include <array>
#include <expected>
#include <voxelui/svg.hpp>
namespace voxelui::vulkan {
using Triangle = std::array<Vec2, 3>;
// Even-odd planar trapezoid decomposition. Vertex and intersection ordinates
// split bands so edge ordering cannot change within a band. No polygon fans.
inline std::expected<std::vector<Triangle>, std::string> tessellate(const VectorShape &shape) {
    struct Edge {
        Vec2 a, b;
        double x(double y) const {
            return a.x + (y - a.y) * double(b.x - a.x) / (b.y - a.y);
        }
    };
    std::vector<Edge> edges;
    std::vector<double> levels;
    if (shape.contours.size() > 2048)
        return std::unexpected("SVG contour count exceeded");
    for (auto &contour : shape.contours) {
        if (contour.size() > 2048)
            return std::unexpected("SVG contour budget exceeded");
        for (auto p : contour)
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return std::unexpected("nonfinite SVG contour");
        if (contour.size() < 3)
            continue;
        for (std::size_t i = 0; i < contour.size(); ++i) {
            auto a = contour[i], b = contour[(i + 1) % contour.size()];
            levels.push_back(a.y);
            if (levels.size() > 8192)
                return std::unexpected("SVG vertex budget exceeded");
            if (a.y != b.y)
                edges.push_back({a, b});
        }
    }
    if (edges.size() > 2048)
        return std::unexpected("SVG edge budget exceeded");
    for (std::size_t i = 0; i < edges.size(); ++i)
        for (std::size_t j = 0; j < i; ++j) {
            auto &a = edges[i];
            auto &b = edges[j];
            const double lo = std::max(std::min(a.a.y, a.b.y), std::min(b.a.y, b.b.y));
            const double hi = std::min(std::max(a.a.y, a.b.y), std::max(b.a.y, b.b.y));
            if (lo >= hi)
                continue;
            const double d0 = a.x(lo) - b.x(lo), d1 = a.x(hi) - b.x(hi);
            if ((d0 < 0 && d1 > 0) || (d0 > 0 && d1 < 0))
                levels.push_back(lo + (hi - lo) * d0 / (d0 - d1));
            if (levels.size() > 8192)
                return std::unexpected("SVG intersection budget exceeded");
        }
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    std::vector<Triangle> result;
    std::vector<const Edge *> active;
    for (std::size_t band = 1; band < levels.size(); ++band) {
        const double lo = levels[band - 1], hi = levels[band], mid = (lo + hi) * .5;
        active.clear();
        for (auto &edge : edges)
            if (mid > std::min(edge.a.y, edge.b.y) && mid < std::max(edge.a.y, edge.b.y))
                active.push_back(&edge);
        std::stable_sort(active.begin(), active.end(),
                         [&](auto *a, auto *b) { return a->x(mid) < b->x(mid); });
        for (std::size_t i = 1; i < active.size(); i += 2) {
            auto *a = active[i - 1];
            auto *b = active[i];
            if (a->x(mid) == b->x(mid))
                continue;
            Vec2 p{float(a->x(lo)), float(lo)}, q{float(b->x(lo)), float(lo)}, r{float(b->x(hi)), float(hi)},
                s{float(a->x(hi)), float(hi)};
            result.push_back({p, q, r});
            result.push_back({p, r, s});
            if (result.size() > 250000)
                return std::unexpected("SVG triangle budget exceeded");
        }
    }
    return result;
}
struct VectorMeshes {
    std::shared_ptr<const VectorDocument> owner;
    std::vector<std::vector<Triangle>> shapes;
};
using VectorMeshesCache = std::map<const VectorDocument *, VectorMeshes>;
} // namespace voxelui::vulkan
