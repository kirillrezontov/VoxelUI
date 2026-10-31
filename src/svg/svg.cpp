#include <charconv>
#include <functional>
#include <numbers>
#include <sstream>
#include <voxelui/style.hpp>
#include <voxelui/svg.hpp>
namespace voxelui {
inline namespace v1 {
namespace {
constexpr float pi = std::numbers::pi_v<float>;
struct Scanner {
    std::string_view s;
    std::size_t pos{};
    void skip() {
        while (pos < s.size() && (std::isspace(static_cast<unsigned char>(s[pos])) || s[pos] == ','))
            ++pos;
    }
    bool next(float &f) {
        skip();
        if (pos == s.size())
            return false;
        bool plus = s[pos] == '+';
        if (plus)
            ++pos;
        auto start = s.data() + pos;
        auto [p, e] = std::from_chars(start, s.data() + s.size(), f);
        if (e != std::errc{} || p == start || !std::isfinite(f) || std::abs(f) > 1e6)
            return false;
        pos = std::size_t(p - s.data());
        return true;
    }
    bool end() {
        skip();
        return pos == s.size();
    }
};
float num(const Node &n, std::string_view key, float fallback = 0) {
    auto v = number(n.attr(key));
    return v ? float(*v) : fallback;
}
std::vector<float> numbers(std::string_view s) {
    Scanner sc{s};
    std::vector<float> out;
    float v;
    while (!sc.end() && out.size() < 100000) {
        if (!sc.next(v))
            return {};
        out.push_back(v);
    }
    return out;
}
Transform transform(std::string_view s, bool &ok) {
    Transform out;
    while (!s.empty()) {
        auto start = s.find_first_not_of(" ,\t\n\r");
        if (start == s.npos)
            break;
        s.remove_prefix(start);
        auto a = s.find('('), b = s.find(')');
        if (a == s.npos || b == s.npos || b < a) {
            ok = false;
            break;
        }
        auto name = trim(s.substr(0, a));
        auto ns = numbers(s.substr(a + 1, b - a - 1));
        Transform t;
        if (name == "translate" && (ns.size() == 1 || ns.size() == 2)) {
            t.e = ns[0];
            t.f = ns.size() == 2 ? ns[1] : 0;
        } else if (name == "scale" && (ns.size() == 1 || ns.size() == 2)) {
            t.a = ns[0];
            t.d = ns.size() == 2 ? ns[1] : ns[0];
        } else if (name == "matrix" && ns.size() == 6)
            t = {ns[0], ns[1], ns[2], ns[3], ns[4], ns[5]};
        else if (name == "rotate" && (ns.size() == 1 || ns.size() == 3)) {
            float r = ns[0] * pi / 180;
            t.a = t.d = std::cos(r);
            t.b = std::sin(r);
            t.c = -t.b;
            if (ns.size() == 3) {
                t.e = ns[1] - t.a * ns[1] - t.c * ns[2];
                t.f = ns[2] - t.b * ns[1] - t.d * ns[2];
            }
        } else {
            ok = false;
            break;
        }
        out = out * t;
        s.remove_prefix(b + 1);
    }
    return out;
}
float distance(Vec2 p, Vec2 a, Vec2 b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::hypot(dx, dy);
    return len < 1e-6f ? std::hypot(p.x - a.x, p.y - a.y)
                       : std::abs(dy * p.x - dx * p.y + b.x * a.y - b.y * a.x) / len;
}
void curve(std::vector<Vec2> &p, Vec2 a, Vec2 b, Vec2 c, Vec2 d, int depth, std::size_t limit) {
    if (p.size() >= limit)
        return;
    if (depth >= 12 || (distance(b, a, d) < .25f && distance(c, a, d) < .25f)) {
        p.push_back(d);
        return;
    }
    auto mid = [](Vec2 x, Vec2 y) { return Vec2{(x.x + y.x) * .5f, (x.y + y.y) * .5f}; };
    auto ab = mid(a, b), bc = mid(b, c), cd = mid(c, d), abc = mid(ab, bc), bcd = mid(bc, cd),
         m = mid(abc, bcd);
    curve(p, a, ab, abc, m, depth + 1, limit);
    curve(p, m, bcd, cd, d, depth + 1, limit);
}
bool path(std::string_view input, VectorShape &shape, std::size_t limit) {
    Scanner sc{input};
    char command = 0, previous = 0;
    Vec2 current{}, start{}, control{};
    std::vector<Vec2> *points = nullptr;
    std::size_t operations = 0, vertices = 0;
    while (!sc.end()) {
        if (++operations > limit)
            return false;
        if (std::isalpha(static_cast<unsigned char>(sc.s[sc.pos])))
            command = sc.s[sc.pos++];
        if (!command)
            return false;
        char c = char(std::toupper(static_cast<unsigned char>(command)));
        bool relative = std::islower(static_cast<unsigned char>(command));
        if (c == 'Z') {
            if (points && points->size() > 1) {
                if (++vertices > limit)
                    return false;
                points->push_back(start);
            }
            current = start;
            command = 0;
            previous = 'Z';
            continue;
        }
        float v[7]{};
        int count = c == 'H' || c == 'V'               ? 1
                    : c == 'M' || c == 'L' || c == 'T' ? 2
                    : c == 'Q' || c == 'S'             ? 4
                    : c == 'C'                         ? 6
                    : c == 'A'                         ? 7
                                                       : 0;
        if (!count)
            return false;
        for (int i = 0; i < count; ++i)
            if (!sc.next(v[i]))
                return false;
        auto point = [&](int i) {
            return Vec2{v[i] + (relative ? current.x : 0), v[i + 1] + (relative ? current.y : 0)};
        };
        if (c == 'M') {
            if (++vertices > limit)
                return false;
            current = point(0);
            start = current;
            shape.contours.emplace_back();
            points = &shape.contours.back();
            points->push_back(current);
            command = relative ? 'l' : 'L';
        } else {
            if (!points)
                return false;
            auto before = points->size();
            auto pointLimit = before + (limit - vertices);
            Vec2 end = current;
            if (c == 'L')
                end = point(0);
            if (c == 'H')
                end.x = v[0] + (relative ? current.x : 0);
            if (c == 'V')
                end.y = v[0] + (relative ? current.y : 0);
            if (c == 'C') {
                auto b = point(0), cc = point(2);
                end = point(4);
                curve(*points, current, b, cc, end, 0, pointLimit);
                control = cc;
            } else if (c == 'S') {
                Vec2 b = (previous == 'C' || previous == 'S')
                             ? Vec2{2 * current.x - control.x, 2 * current.y - control.y}
                             : current;
                auto cc = point(0);
                end = point(2);
                curve(*points, current, b, cc, end, 0, pointLimit);
                control = cc;
            } else if (c == 'Q' || c == 'T') {
                Vec2 q;
                if (c == 'Q') {
                    q = point(0);
                    end = point(2);
                } else {
                    q = (previous == 'Q' || previous == 'T')
                            ? Vec2{2 * current.x - control.x, 2 * current.y - control.y}
                            : current;
                    end = point(0);
                }
                curve(*points, current,
                      {current.x + (q.x - current.x) * 2 / 3, current.y + (q.y - current.y) * 2 / 3},
                      {end.x + (q.x - end.x) * 2 / 3, end.y + (q.y - end.y) * 2 / 3}, end, 0, pointLimit);
                control = q;
            } else if (c == 'A') {
                end = point(5);
                double rx = std::abs(v[0]), ry = std::abs(v[1]), angle = double(v[2]) * pi / 180;
                if ((v[3] != 0 && v[3] != 1) || (v[4] != 0 && v[4] != 1))
                    return false;
                if (rx == 0 || ry == 0)
                    points->push_back(end);
                else if (current.x != end.x || current.y != end.y) {
                    double ca = std::cos(angle), sa = std::sin(angle), dx = (current.x - end.x) * .5f,
                           dy = (current.y - end.y) * .5f, x = ca * dx + sa * dy, y = -sa * dx + ca * dy;
                    double scale = x * x / (rx * rx) + y * y / (ry * ry);
                    if (scale > 1) {
                        rx *= std::sqrt(scale);
                        ry *= std::sqrt(scale);
                    }
                    double denom = rx * rx * y * y + ry * ry * x * x;
                    double k =
                        (v[3] == v[4] ? -1.f : 1.f) *
                        std::sqrt(std::max(0.0, (rx * rx * ry * ry - denom) / std::max(denom, 1e-300)));
                    double cx = k * rx * y / ry, cy = -k * ry * x / rx,
                           centerX = ca * cx - sa * cy + (current.x + end.x) * .5f,
                           centerY = sa * cx + ca * cy + (current.y + end.y) * .5f;
                    double a = std::atan2((y - cy) / ry, (x - cx) / rx),
                           b = std::atan2((-y - cy) / ry, (-x - cx) / rx), delta = b - a;
                    if (!v[4] && delta > 0)
                        delta -= 2 * pi;
                    if (v[4] && delta < 0)
                        delta += 2 * pi;
                    int segments = std::clamp(
                        int(std::clamp(std::ceil(std::abs(delta) * std::sqrt(std::max(rx, ry) / .5)), 1.0,
                                       4096.0)),
                        1, 4096);
                    for (int i = 1; i <= segments && points->size() < pointLimit; ++i) {
                        double t = a + delta * i / segments;
                        points->push_back({float(centerX + ca * rx * std::cos(t) - sa * ry * std::sin(t)),
                                           float(centerY + sa * rx * std::cos(t) + ca * ry * std::sin(t))});
                    }
                }
            } else
                points->push_back(end);
            current = end;
            vertices += points->size() - before;
            if (vertices >= limit)
                return false;
        }
        previous = c;
    }
    return true;
}
Rect bounds(const std::vector<std::vector<Vec2>> &cs) {
    float x = 1e9, y = 1e9, r = -1e9, b = -1e9;
    for (auto &c : cs)
        for (auto p : c) {
            x = std::min(x, p.x);
            y = std::min(y, p.y);
            r = std::max(r, p.x);
            b = std::max(b, p.y);
        }
    return r < x ? Rect{} : Rect{x, y, r - x, b - y};
}
} // namespace
std::shared_ptr<const VectorDocument> parseSvg(const Resource &r, Diagnostics &d, Limits limits,
                                               Resources *resources) {
    auto root = detail::parseXml(
        std::string_view(reinterpret_cast<const char *>(r.bytes.data()), r.bytes.size()), r.id, d, limits);
    if (!root)
        return {};
    auto out = std::make_shared<VectorDocument>();
    auto diagnostic = [&](const Node &n, Severity s, std::string msg) {
        d.push_back({s, r.id, n.line, 1, std::move(msg)});
    };
    if (root->tag != "svg") {
        diagnostic(*root, Severity::error, "Expected svg root");
        return {};
    }
    auto vb = numbers(root->attr("viewBox"));
    if (vb.size() == 4 && vb[2] > 0 && vb[3] > 0)
        out->viewBox = {vb[0], vb[1], vb[2], vb[3]};
    else if (num(*root, "width") > 0 && num(*root, "height") > 0)
        out->viewBox = {0, 0, num(*root, "width"), num(*root, "height")};
    else {
        diagnostic(*root, Severity::error, "SVG needs a positive viewBox or width/height");
        return {};
    }
    std::map<std::string, Gradient> gradients;
    std::map<std::string, Rect> clips;
    std::map<std::string, std::shared_ptr<const Image>> rasterCache;
    std::size_t rasterBytes = 0;
    std::function<void(const Node &)> defs = [&](const Node &n) {
        if (n.tag == "script" || n.tag == "foreignObject" || n.tag == "use") {
            diagnostic(n, Severity::error, "Executable, embedded or referenced SVG content is forbidden");
        }
        for (auto &[k, v] : n.attributes)
            if (k.starts_with("on") ||
                ((k == "href" || k == "xlink:href") && !v.starts_with('#') && !validResourceId(v)))
                diagnostic(n, Severity::error, "Illegal SVG external reference/event handler");
        if (n.tag == "linearGradient") {
            Gradient g;
            g.enabled = true;
            auto fraction = [&](const char *k, float fallback) {
                auto val = n.attr(k);
                if (val.empty())
                    return fallback;
                if (val.ends_with('%'))
                    return float(number(std::string_view(val).substr(0, val.size() - 1)).value_or(0)) * .01f;
                return float(number(val).value_or(fallback));
            };
            g.from = {fraction("x1", 0), fraction("y1", 0)};
            g.to = {fraction("x2", 1), fraction("y2", 0)};
            if (n.children.size() >= 2) {
                g.first = parseColor(n.children.front().attr("stop-color")).value_or(Color{});
                g.last = parseColor(n.children.back().attr("stop-color")).value_or(Color{});
            }
            gradients[n.id] = g;
        }
        if (n.tag == "clipPath") {
            if (n.children.size() == 1 && n.children[0].tag == "rect") {
                auto &c = n.children[0];
                clips[n.id] = {num(c, "x"), num(c, "y"), num(c, "width"), num(c, "height")};
            } else
                diagnostic(n, Severity::warning, "Only rectangular clipPath is supported");
        }
        for (auto &c : n.children)
            defs(c);
    };
    defs(*root);
    if (hasErrors(d))
        return {};
    std::size_t vertices = 0;
    std::function<void(const Node &, Transform, VectorShape)> walk = [&](const Node &n, Transform matrix,
                                                                         VectorShape inherited) {
        bool ok = true;
        matrix = matrix * transform(n.attr("transform"), ok);
        for (float v : {matrix.a, matrix.b, matrix.c, matrix.d, matrix.e, matrix.f})
            if (!std::isfinite(v) || std::abs(v) > 1e7f)
                ok = false;
        if (!ok) {
            diagnostic(n, Severity::error, "Invalid SVG transform");
            return;
        }
        if (n.attributes.contains("style"))
            diagnostic(n, Severity::warning, "SVG style attributes unsupported; use presentation attributes");
        if (n.attributes.contains("fill")) {
            auto v = n.attr("fill");
            if (v == "none")
                inherited.fill = {0, 0, 0, 0};
            else if (v.starts_with("url(#") && v.ends_with(')')) {
                auto it = gradients.find(v.substr(5, v.size() - 6));
                if (it != gradients.end())
                    inherited.gradient = it->second;
                else
                    diagnostic(n, Severity::error, "Missing gradient");
            } else if (auto color = parseColor(v)) {
                inherited.fill = *color;
                inherited.gradient.enabled = false;
            } else
                diagnostic(n, Severity::error, "Invalid SVG fill");
        }
        if (n.attributes.contains("stroke")) {
            auto v = n.attr("stroke");
            if (v == "none")
                inherited.stroke = {0, 0, 0, 0};
            else if (auto c = parseColor(v))
                inherited.stroke = *c;
            else
                diagnostic(n, Severity::error, "Invalid SVG stroke");
        }
        inherited.strokeWidth = num(n, "stroke-width", inherited.strokeWidth);
        float opacity = std::clamp(num(n, "opacity", 1), 0.f, 1.f);
        inherited.fill.a =
            std::uint8_t(inherited.fill.a * opacity * std::clamp(num(n, "fill-opacity", 1), 0.f, 1.f));
        inherited.stroke.a =
            std::uint8_t(inherited.stroke.a * opacity * std::clamp(num(n, "stroke-opacity", 1), 0.f, 1.f));
        if (auto cp = n.attr("clip-path"); !cp.empty()) {
            if (cp.starts_with("url(#") && cp.ends_with(')') && clips.contains(cp.substr(5, cp.size() - 6))) {
                auto clip = clips[cp.substr(5, cp.size() - 6)];
                auto a = matrix.apply({clip.x, clip.y}), b = matrix.apply({clip.x + clip.w, clip.y + clip.h});
                inherited.clip =
                    Rect{std::min(a.x, b.x), std::min(a.y, b.y), std::abs(a.x - b.x), std::abs(a.y - b.y)};
            } else
                diagnostic(n, Severity::error, "Missing or unsupported clip reference");
        }
        if (auto name = n.attr("data-voxelui-slot"); !name.empty())
            out->slots.push_back({name,
                                  n.id,
                                  matrix,
                                  {num(n, "data-x"), num(n, "data-y"), num(n, "data-width", out->viewBox.w),
                                   num(n, "data-height", out->viewBox.h)}});
        if (n.tag == "defs" || n.tag == "linearGradient" || n.tag == "clipPath" || n.tag == "stop")
            return;
        VectorShape shape = inherited;
        shape.contours.clear();
        float x = num(n, "x"), y = num(n, "y"), w = num(n, "width"), h = num(n, "height");
        if (n.tag == "rect") {
            if (w > 0 && h > 0)
                shape.contours = {{{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, {x, y}}};
        } else if (n.tag == "image") {
            auto id = n.attr("href", n.attr("xlink:href"));
            if (!resources || !validResourceId(id))
                diagnostic(n, Severity::error, "SVG images require a namespaced resource resolver");
            else if (auto r = resources->get(id, &d)) {
                if (auto it = rasterCache.find(id); it != rasterCache.end())
                    shape.image = it->second;
                else {
                    auto decoded = decodeImage(*r, d);
                    if (decoded) {
                        if (rasterBytes + decoded->rgba.size() > 128 * 1024 * 1024) {
                            diagnostic(n, Severity::error, "SVG raster memory budget exceeded");
                            return;
                        }
                        rasterBytes += decoded->rgba.size();
                        rasterCache[id] = decoded;
                        shape.image = decoded;
                    }
                }
                shape.imageBounds = {x, y, w, h};
                shape.imageTransform = matrix;
                if (w <= 0 || h <= 0)
                    diagnostic(n, Severity::error, "SVG image requires positive width and height");
                if (shape.image)
                    out->shapes.push_back(std::move(shape));
            }
        } else if (n.tag == "circle" || n.tag == "ellipse") {
            float cx = num(n, "cx"), cy = num(n, "cy"), rx = num(n, n.tag == "circle" ? "r" : "rx"),
                  ry = n.tag == "circle" ? rx : num(n, "ry");
            if (rx > 0 && ry > 0) {
                int steps = std::clamp(int(std::ceil(2 * pi * std::sqrt(std::max(rx, ry) / .5f))), 12, 4096);
                shape.contours.emplace_back();
                for (int i = 0; i <= steps; ++i) {
                    float a = 2 * pi * i / steps;
                    shape.contours[0].push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
                }
            }
        } else if (n.tag == "line") {
            shape.fill = {0, 0, 0, 0};
            shape.closed = false;
            shape.contours = {{{num(n, "x1"), num(n, "y1")}, {num(n, "x2"), num(n, "y2")}}};
        } else if (n.tag == "polygon" || n.tag == "polyline") {
            auto pts = numbers(n.attr("points"));
            if (pts.size() % 2 || pts.size() < 4)
                diagnostic(n, Severity::error, "Invalid polygon points");
            else {
                shape.contours.emplace_back();
                for (std::size_t i = 0; i < pts.size(); i += 2)
                    shape.contours[0].push_back({pts[i], pts[i + 1]});
                if (n.tag == "polygon")
                    shape.contours[0].push_back(shape.contours[0].front());
                else
                    shape.closed = false;
            }
        } else if (n.tag == "path") {
            if (!path(n.attr("d"), shape, limits.geometry))
                diagnostic(n, Severity::error, "Invalid or excessive SVG path");
        } else if (n.tag != "svg" && n.tag != "g" && n.tag != "title" && n.tag != "desc")
            diagnostic(n, Severity::warning, "Unsupported SVG element skipped: " + n.tag);
        if (!shape.contours.empty()) {
            auto localBounds = bounds(shape.contours);
            if (shape.gradient.enabled) {
                auto &g = shape.gradient;
                g.from = matrix.apply(
                    {localBounds.x + localBounds.w * g.from.x, localBounds.y + localBounds.h * g.from.y});
                g.to = matrix.apply(
                    {localBounds.x + localBounds.w * g.to.x, localBounds.y + localBounds.h * g.to.y});
                g.first.a = std::uint8_t(g.first.a * opacity);
                g.last.a = std::uint8_t(g.last.a * opacity);
            }
            for (auto &c : shape.contours)
                for (auto &p : c) {
                    p = matrix.apply(p);
                    if (!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x) > 1e7f ||
                        std::abs(p.y) > 1e7f) {
                        diagnostic(n, Severity::error, "Transformed SVG coordinate exceeds bounds");
                        return;
                    }
                    ++vertices;
                }
            shape.strokeWidth *= std::sqrt(std::abs(matrix.a * matrix.d - matrix.b * matrix.c));
            if (vertices > limits.geometry) {
                diagnostic(n, Severity::error, "SVG geometry budget exceeded");
                return;
            }
            out->shapes.push_back(std::move(shape));
        }
        for (auto &c : n.children)
            if (vertices <= limits.geometry)
                walk(c, matrix, inherited);
    };
    walk(*root, {}, {});
    if (hasErrors(d))
        return {};
    return out;
}
std::shared_ptr<const VectorDocument> VectorCache::get(const Resource &r, Diagnostics &d,
                                                       Resources *resources) {
    auto it = entries_.find(r.id);
    if (it != entries_.end() && it->second.revision == r.revision &&
        it->second.epoch == (resources ? resources->epoch() : 0))
        return it->second.document;
    auto parsed = parseSvg(r, d, {}, resources);
    if (entries_.size() >= 64)
        entries_.clear();
    if (parsed)
        entries_[r.id] = {r.revision, resources ? resources->epoch() : 0, parsed};
    return parsed;
}
} // namespace v1
} // namespace voxelui
