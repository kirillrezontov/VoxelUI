#include "geometry.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <voxelui/software.hpp>
using namespace voxelui;
using namespace voxelui::vulkan;
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
double edge(Vec2 a, Vec2 b, Vec2 p) {
    return (double(b.x) - a.x) * (double(p.y) - a.y) - (double(b.y) - a.y) * (double(p.x) - a.x);
}
bool contains(Vec2 a, Vec2 b, Vec2 c, Vec2 p) {
    if (edge(a, b, c) < 0)
        std::swap(b, c);
    auto inside = [&](Vec2 u, Vec2 v) {
        double e = edge(u, v, p);
        return e > 0 || (e == 0 && (v.y < u.y || (v.y == u.y && v.x > u.x)));
    };
    return edge(a, b, c) != 0 && inside(a, b) && inside(b, c) && inside(c, a);
}
// Independent triangle coverage oracle; this is CPU verification, not a GPU test.
void compare(const DrawList &list, bool exact = true) {
    auto result = compileGeometry(list);
    require(bool(result), "geometry rejected fixture");
    SoftwareRenderer reference;
    reference.render(list);
    auto expected = reference.pixels();
    std::vector<std::uint8_t> actual(expected.size());
    for (std::size_t i = 3; i < actual.size(); i += 4)
        actual[i] = 255;
    auto &m = *result;
    for (std::size_t i = 0; i < m.indices.size(); i += 3) {
        auto a = m.vertices[m.indices[i]], b = m.vertices[m.indices[i + 1]], c = m.vertices[m.indices[i + 2]];
        for (int y = 0; y < reference.height(); ++y)
            for (int x = 0; x < reference.width(); ++x) {
                if (!contains(a.position, b.position, c.position, {x + .5f, y + .5f}))
                    continue;
                auto p = (std::size_t(y) * reference.width() + x) * 4;
                for (int k = 0; k < 3; ++k) {
                    unsigned source = k == 0 ? a.color.r : k == 1 ? a.color.g : a.color.b;
                    actual[p + k] =
                        std::uint8_t((source * a.color.a + actual[p + k] * (255 - a.color.a) + 127) / 255);
                }
            }
    }
    for (std::size_t i = 0; i < actual.size(); i += 4) {
        if (std::equal(actual.begin() + i, actual.begin() + i + 4, expected.begin() + i))
            continue;
        require(!exact, "axis-aligned software coverage/color mismatch");
        // Inverse mapping and forward triangles can classify exact transformed
        // edges differently. Allow only a 0.001 framebuffer-pixel edge band.
        Vec2 p{float((i / 4) % reference.width()) + .5f, float((i / 4) / reference.width()) + .5f};
        double closest = 1e30;
        for (std::size_t j = 0; j < m.indices.size(); j += 3)
            for (int k = 0; k < 3; ++k) {
                auto a = m.vertices[m.indices[j + k]].position;
                auto b = m.vertices[m.indices[j + (k + 1) % 3]].position;
                double dx = double(b.x) - a.x, dy = double(b.y) - a.y;
                double t = std::clamp(
                    ((p.x - a.x) * dx + (p.y - a.y) * dy) / std::max(1e-30, dx * dx + dy * dy), 0., 1.);
                closest = std::min(closest, std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy));
            }
        require(closest <= .001, "transformed software mismatch away from triangle edges");
    }
}
int main() try {
    std::size_t comparisons = 0;
    for (float scale : {.5f, 1.f, 1.5f, 2.f, 3.f})
        for (Vec2 size : {Vec2{32, 24}, Vec2{57, 43}, Vec2{96, 64}}) {
            DrawList list;
            list.logicalSize = size;
            list.framebufferScale = scale;
            list.commands = {DrawRectangle{{0, 0, 100, 100}, {20, 40, 80, 255}, {}, 0, 0},
                             PushClip{{2, 2, 22, 18}},
                             PushTransform{{1, 0, 0, 1, 2, 2}},
                             DrawRectangle{{0, 0, 24, 18}, {200, 40, 10, 128}, {20, 220, 10, 128}, 2, 0},
                             PushClip{{8, 4, 8, 12}},
                             DrawRectangle{{0, 0, 25, 20}, {10, 10, 240, 128}, {}, 0, 0},
                             PopClip{},
                             PopTransform{},
                             PopClip{},
                             PushClip{{0, 0, 0, 10}},
                             DrawRectangle{{0, 0, 100, 100}, {255, 0, 0, 255}, {}, 0, 0},
                             PopClip{}};
            compare(list);
            ++comparisons;
            list.commands = {PushTransform{{.9f, .3f, -.3f, .9f, 12, 3}}, PushClip{{1, 1, 18, 16}},
                             DrawRectangle{{0, 0, 24, 18}, {200, 40, 10, 128}, {20, 220, 10, 128}, 2, 0},
                             PopClip{}, PopTransform{}};
            compare(list, false);
            ++comparisons;
        }
    DrawList list;
    list.logicalSize = {32, 24};
    auto reject = [&](std::vector<DrawCommand> commands, std::string_view reason,
                      GeometryLimits limits = GeometryLimits{}) {
        list.commands = std::move(commands);
        auto result = compileGeometry(list, limits);
        require(!result, "invalid draw list accepted");
        if (result.error().message.find(reason) == std::string::npos)
            throw std::runtime_error("expected " + std::string(reason) + ": " + result.error().message);
    };
    DrawRectangle rect{{1, 1, 10, 10}, {255, 255, 255, 128}, {255, 0, 0, 128}, 0, 0};
    auto rounded = rect;
    rounded.radius = 1;
    list.commands = {rounded};
    auto roundedMesh = compileGeometry(list);
    require(roundedMesh && !roundedMesh->vertices.empty() && roundedMesh->vertices[0].rounding.x == 1,
            "rounded fragment parameters missing");
    reject({PopClip{}}, "underflow");
    reject({PopTransform{}}, "underflow");
    reject({PushClip{{0, 0, 1, 1}}}, "unbalanced");
    reject({PushTransform{}}, "unbalanced");
    reject({PushTransform{}, PushTransform{}}, "depth", {100, 100, 100, 1});
    reject({PushClip{}, PushClip{}}, "depth", {100, 100, 100, 1});
    reject({rect}, "command budget", {0, 100, 100, 1});
    reject({rect}, "geometry budget", {1, 3, 6, 1});
    reject({rect}, "geometry budget", {1, 4, 5, 1});
    list.commands = {rect};
    require(bool(compileGeometry(list, {1, 4, 6, 0})), "exact budgets rejected");
    auto invalid = rect;
    invalid.bounds.x = std::numeric_limits<float>::infinity();
    reject({invalid}, "invalid rectangle");
    reject({PushTransform{{std::numeric_limits<float>::max(), 0, 0, 1, 0, 0}}, rect}, "overflow");
    list.commands = {PushTransform{{-1, 0, 0, 1, 20, 0}}, rect, PopTransform{}};
    compare(list);
    list.commands = {PushTransform{{0, 0, 0, 0, 0, 0}}, rect, PopTransform{}};
    compare(list);
    rect.borderWidth = 8;
    list.commands = {rect};
    compare(list);
    list.logicalSize.x = std::numeric_limits<float>::quiet_NaN();
    require(!compileGeometry(list), "NaN viewport accepted");
    list.logicalSize = {64, 64};
    Compiler compiler;
    auto image = std::make_shared<Image>(Image{1, 1, {20, 40, 60, 255}});
    list.commands = {DrawImage{{0, 0, 10, 10}, image, 1}};
    auto first = compiler.compile(list);
    require(bool(first), "image compile failed");
    const auto oldPixels = first->atlas->rgba;
    auto secondImage = std::make_shared<Image>(Image{1, 1, {90, 100, 110, 255}});
    list.commands.push_back(DrawImage{{10, 0, 10, 10}, secondImage, 1});
    auto second = compiler.compile(list);
    require(bool(second), "second image compile failed");
    require(first->atlas->rgba == oldPixels && first->atlas != second->atlas,
            "atlas edit mutated published frame");
    auto repeated = compiler.compile(list);
    require(repeated && repeated->atlas == second->atlas, "unchanged atlas was rebuilt");
    auto invalidImage = std::make_shared<Image>(Image{2, 2, {1}});
    reject({DrawImage{{0, 0, 2, 2}, invalidImage, 1}}, "invalid");
    auto oversized = std::make_shared<Image>();
    oversized->width = 4096;
    oversized->height = 1;
    reject({DrawImage{{0, 0, 2, 2}, oversized, 1}}, "oversized");
    std::cout << comparisons + 3 << " software comparisons and budget/diagnostic cases passed\n";
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
