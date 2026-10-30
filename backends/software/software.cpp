#include <functional>
#include <png.h>
#include <stdexcept>
#include <voxelui/software.hpp>
namespace voxelui {
inline namespace v1 {
namespace {
Rect mapped(Rect b, Transform t) {
    auto a = t.apply({b.x, b.y}), c = t.apply({b.x + b.w, b.y}), d = t.apply({b.x, b.y + b.h}),
         e = t.apply({b.x + b.w, b.y + b.h});
    float x = std::min({a.x, c.x, d.x, e.x}), y = std::min({a.y, c.y, d.y, e.y});
    return {x, y, std::max({a.x, c.x, d.x, e.x}) - x, std::max({a.y, c.y, d.y, e.y}) - y};
}
std::optional<Transform> inverse(Transform t) {
    float det = t.a * t.d - t.b * t.c;
    if (!std::isfinite(det) || std::abs(det) < 1e-8f)
        return {};
    return Transform{t.d / det,
                     -t.b / det,
                     -t.c / det,
                     t.a / det,
                     (t.c * t.f - t.d * t.e) / det,
                     (t.b * t.e - t.a * t.f) / det};
}
Color opacity(Color c, float a) {
    c.a = std::uint8_t(c.a * std::clamp(a, 0.f, 1.f));
    return c;
}
float segmentDistance(Vec2 p, Vec2 a, Vec2 b) {
    float x = b.x - a.x, y = b.y - a.y;
    float t = std::clamp(((p.x - a.x) * x + (p.y - a.y) * y) / std::max(1e-12f, x * x + y * y), 0.f, 1.f);
    return std::hypot(p.x - a.x - t * x, p.y - a.y - t * y);
}
} // namespace
void SoftwareRenderer::render(const DrawList &list) {
    float dpi = std::clamp(list.framebufferScale, .25f, 8.f);
    if (!std::isfinite(list.logicalSize.x) || !std::isfinite(list.logicalSize.y) || list.logicalSize.x <= 0 ||
        list.logicalSize.y <= 0)
        throw std::invalid_argument("Invalid draw list viewport");
    double w = std::ceil(list.logicalSize.x * dpi), h = std::ceil(list.logicalSize.y * dpi);
    if (w > 8192 || h > 8192 || w * h > 16000000)
        throw std::length_error("Software framebuffer exceeds 16 million pixels");
    width_ = int(w);
    height_ = int(h);
    pixels_.assign(std::size_t(width_) * height_ * 4, 0);
    for (std::size_t i = 3; i < pixels_.size(); i += 4)
        pixels_[i] = 255;
    std::vector<Rect> clips{{0, 0, float(width_), float(height_)}};
    std::vector<Transform> transforms{{dpi, 0, 0, dpi, 0, 0}};
    auto blend = [&](int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_ || !c.a ||
            !clips.back().contains({x + .5f, y + .5f}))
            return;
        auto i = (std::size_t(y) * width_ + x) * 4;
        unsigned a = c.a;
        pixels_[i] = std::uint8_t((c.r * a + pixels_[i] * (255 - a) + 127) / 255);
        pixels_[i + 1] = std::uint8_t((c.g * a + pixels_[i + 1] * (255 - a) + 127) / 255);
        pixels_[i + 2] = std::uint8_t((c.b * a + pixels_[i + 2] * (255 - a) + 127) / 255);
    };
    auto raster = [&](Rect rect, Transform matrix, auto &&sample) {
        if (rect.w <= 0 || rect.h <= 0)
            return;
        auto inv = inverse(matrix);
        if (!inv)
            return;
        auto bound = intersect(mapped(rect, matrix), clips.back());
        if (!std::isfinite(bound.x) || !std::isfinite(bound.y) || !std::isfinite(bound.w) ||
            !std::isfinite(bound.h))
            return;
        int x0 = std::max(0, int(std::floor(bound.x))), y0 = std::max(0, int(std::floor(bound.y))),
            x1 = std::min(width_, int(std::ceil(bound.x + bound.w))),
            y1 = std::min(height_, int(std::ceil(bound.y + bound.h)));
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                auto p = inv->apply({x + .5f, y + .5f});
                if (rect.contains(p))
                    blend(x, y, sample(p));
            }
    };
    for (auto &command : list.commands)
        std::visit(
            [&](const auto &c) {
                using T = std::decay_t<decltype(c)>;
                auto matrix = transforms.back();
                if constexpr (std::same_as<T, PushClip>)
                    clips.push_back(intersect(clips.back(), mapped(c.bounds, matrix)));
                else if constexpr (std::same_as<T, PopClip>) {
                    if (clips.size() > 1)
                        clips.pop_back();
                } else if constexpr (std::same_as<T, PushTransform>)
                    transforms.push_back(matrix * c.transform);
                else if constexpr (std::same_as<T, PopTransform>) {
                    if (transforms.size() > 1)
                        transforms.pop_back();
                } else if constexpr (std::same_as<T, DrawRectangle>) {
                    auto inside = [](Vec2 p, Rect b, float radius) {
                        if (!b.contains(p))
                            return false;
                        float r = std::min({radius, b.w * .5f, b.h * .5f});
                        float x = std::clamp(p.x, b.x + r, b.x + b.w - r),
                              y = std::clamp(p.y, b.y + r, b.y + b.h - r);
                        return std::hypot(p.x - x, p.y - y) <= r;
                    };
                    raster(c.bounds, matrix, [&](Vec2 p) {
                        if (!inside(p, c.bounds, c.radius))
                            return Color{0, 0, 0, 0};
                        Rect inner{c.bounds.x + c.borderWidth, c.bounds.y + c.borderWidth,
                                   std::max(0.f, c.bounds.w - 2 * c.borderWidth),
                                   std::max(0.f, c.bounds.h - 2 * c.borderWidth)};
                        return c.borderWidth > 0 && !inside(p, inner, std::max(0.f, c.radius - c.borderWidth))
                                   ? c.border
                                   : c.fill;
                    });
                } else if constexpr (std::same_as<T, DrawImage>) {
                    if (!c.image || c.image->width <= 0 || c.image->height <= 0 ||
                        c.image->rgba.size() != std::size_t(c.image->width) * c.image->height * 4)
                        return;
                    raster(c.bounds, matrix, [&](Vec2 p) {
                        int x = std::clamp(int((p.x - c.bounds.x) / c.bounds.w * c.image->width), 0,
                                           c.image->width - 1),
                            y = std::clamp(int((p.y - c.bounds.y) / c.bounds.h * c.image->height), 0,
                                           c.image->height - 1);
                        auto i = (std::size_t(y) * c.image->width + x) * 4;
                        return opacity({c.image->rgba[i], c.image->rgba[i + 1], c.image->rgba[i + 2],
                                        c.image->rgba[i + 3]},
                                       c.opacity);
                    });
                } else if constexpr (std::same_as<T, DrawText>) {
                    if (!c.layout)
                        return;
                    for (auto &glyph : c.layout->glyphs) {
                        if (!glyph.bitmap)
                            continue;
                        auto &g = *glyph.bitmap;
                        if (g.width <= 0 || g.height <= 0 ||
                            g.alpha.size() != std::size_t(g.width) * g.height)
                            continue;
                        Rect rect{c.origin.x + glyph.x, c.origin.y + glyph.y, float(g.width),
                                  float(g.height)};
                        raster(rect, matrix, [&](Vec2 p) {
                            int x = std::clamp(int(p.x - rect.x), 0, g.width - 1),
                                y = std::clamp(int(p.y - rect.y), 0, g.height - 1);
                            Color color = c.color;
                            color.a =
                                std::uint8_t(unsigned(color.a) * g.alpha[std::size_t(y) * g.width + x] / 255);
                            return color;
                        });
                    }
                } else if constexpr (std::same_as<T, DrawVectorGeometry>) {
                    if (!c.geometry)
                        return;
                    auto vb = c.geometry->viewBox;
                    if (vb.w <= 0 || vb.h <= 0)
                        return;
                    float k = std::min(c.bounds.w / vb.w, c.bounds.h / vb.h);
                    auto fit = matrix * Transform{k,
                                                  0,
                                                  0,
                                                  k,
                                                  c.bounds.x + (c.bounds.w - vb.w * k) * .5f - vb.x * k,
                                                  c.bounds.y + (c.bounds.h - vb.h * k) * .5f - vb.y * k};
                    clips.push_back(intersect(clips.back(), mapped(c.bounds, matrix)));
                    for (auto &shape : c.geometry->shapes) {
                        if (shape.image) {
                            auto im = shape.image;
                            auto box = shape.imageBounds;
                            raster(box, fit * shape.imageTransform, [&](Vec2 p) {
                                int x = std::clamp(int((p.x - box.x) / box.w * im->width), 0, im->width - 1);
                                int y =
                                    std::clamp(int((p.y - box.y) / box.h * im->height), 0, im->height - 1);
                                auto i = (std::size_t(y) * im->width + x) * 4;
                                return opacity(
                                    {im->rgba[i], im->rgba[i + 1], im->rgba[i + 2], im->rgba[i + 3]},
                                    c.opacity);
                            });
                            continue;
                        }

                        bool clipped = shape.clip.has_value();
                        if (clipped)
                            clips.push_back(intersect(clips.back(), mapped(*shape.clip, fit)));
                        std::vector<std::vector<Vec2>> contours;
                        float top = 1e9, bottom = -1e9;
                        for (auto &contour : shape.contours) {
                            contours.emplace_back();
                            for (auto p : contour) {
                                auto v = fit.apply(p);
                                contours.back().push_back(v);
                                top = std::min(top, v.y);
                                bottom = std::max(bottom, v.y);
                            }
                        }
                        auto gradFrom = fit.apply(shape.gradient.from), gradTo = fit.apply(shape.gradient.to);
                        auto color = [&](Vec2 p) {
                            Color col = shape.fill;
                            if (shape.gradient.enabled) {
                                float dx = gradTo.x - gradFrom.x, dy = gradTo.y - gradFrom.y,
                                      t = std::clamp(((p.x - gradFrom.x) * dx + (p.y - gradFrom.y) * dy) /
                                                         std::max(1e-12f, dx * dx + dy * dy),
                                                     0.f, 1.f);
                                auto mix = [&](int a, int b) { return std::uint8_t(a + (b - a) * t); };
                                auto a = shape.gradient.first, b = shape.gradient.last;
                                col = {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
                            }
                            return opacity(col, c.opacity);
                        };
                        if ((shape.fill.a || shape.gradient.enabled) && top <= bottom) {
                            int y0 = std::max(0, int(std::floor(std::max(top, clips.back().y)))),
                                y1 = std::min(height_, int(std::ceil(std::min(bottom, clips.back().y +
                                                                                          clips.back().h))));
                            std::vector<float> xs;
                            for (int y = y0; y < y1; ++y) {
                                xs.clear();
                                float scan = y + .5f;
                                for (auto &contour : contours)
                                    if (contour.size() > 2)
                                        for (std::size_t i = 0, j = contour.size() - 1; i < contour.size();
                                             j = i++) {
                                            auto a = contour[j], b = contour[i];
                                            if ((a.y > scan) != (b.y > scan))
                                                xs.push_back(a.x + (scan - a.y) * (b.x - a.x) / (b.y - a.y));
                                        }
                                std::sort(xs.begin(), xs.end());
                                for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
                                    int x0 = std::max(0,
                                                      int(std::ceil(std::max(xs[i], clips.back().x) - .5f))),
                                        x1 = std::min(
                                            width_,
                                            int(std::ceil(
                                                std::min(xs[i + 1], clips.back().x + clips.back().w) - .5f)));
                                    for (int x = x0; x < x1; ++x)
                                        blend(x, y, color({x + .5f, scan}));
                                }
                            }
                        }
                        if (shape.stroke.a && shape.strokeWidth > 0) {
                            float radius =
                                shape.strokeWidth * std::sqrt(std::abs(fit.a * fit.d - fit.b * fit.c)) * .5f;
                            for (auto &contour : contours)
                                for (std::size_t i = 1; i < contour.size(); ++i) {
                                    auto a = contour[i - 1], b = contour[i];
                                    Rect box{std::min(a.x, b.x) - radius, std::min(a.y, b.y) - radius,
                                             std::abs(b.x - a.x) + 2 * radius,
                                             std::abs(b.y - a.y) + 2 * radius};
                                    box = intersect(box, clips.back());
                                    int x0 = std::max(0, int(std::floor(box.x))),
                                        y0 = std::max(0, int(std::floor(box.y))),
                                        x1 = std::min(width_, int(std::ceil(box.x + box.w))),
                                        y1 = std::min(height_, int(std::ceil(box.y + box.h)));
                                    for (int y = y0; y < y1; ++y)
                                        for (int x = x0; x < x1; ++x)
                                            if (segmentDistance({x + .5f, y + .5f}, a, b) <= radius)
                                                blend(x, y, opacity(shape.stroke, c.opacity));
                                }
                        }
                        if (clipped)
                            clips.pop_back();
                    }
                    clips.pop_back();
                }
            },
            command);
}
bool SoftwareRenderer::savePng(const std::filesystem::path &path) const {
    if (pixels_.empty())
        return false;
    png_image p{};
    p.version = PNG_IMAGE_VERSION;
    p.width = png_uint_32(width_);
    p.height = png_uint_32(height_);
    p.format = PNG_FORMAT_RGBA;
    return png_image_write_to_file(&p, path.string().c_str(), 0, pixels_.data(), 0, nullptr) != 0;
}
} // namespace v1
} // namespace voxelui
