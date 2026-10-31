#include "../apps/common/host.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <voxelui/layout.hpp>
// Counts ordinary C++ allocations in this executable (excludes aligned new and C libraries).
static std::size_t allocations;
void *operator new(std::size_t size) {
    ++allocations;
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept {
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
void *operator new[](std::size_t n) {
    return ::operator new(n);
}
void operator delete[](void *p) noexcept {
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}
using namespace voxelui;
int main(int argc, char **argv) {
    Runtime ui;
    demo::mount(ui, argc > 1 ? argv[1] : "examples");
    std::size_t sink = 0;
    auto measure = [&](std::string name, int iterations, auto &&f, Runtime *observed = nullptr) {
#ifdef VOXELUI_BENCH_STATS
        auto before = observed ? observed->runtimeStats() : RuntimeStats{};
#else
        (void)observed;
#endif
        auto start = std::chrono::steady_clock::now();
        auto allocationStart = allocations;
        for (int i = 0; i < iterations; ++i)
            f(i);
        double us =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() /
            iterations;
        std::cout << std::left << std::setw(34) << name << std::right << std::fixed << std::setprecision(2)
                  << us << " us/op (" << iterations << "), "
                  << double(allocations - allocationStart) / iterations << " allocations/op\n";
#ifdef VOXELUI_BENCH_STATS
        if (observed) {
            auto after = observed->runtimeStats();
            std::cout << "  nodes/op: bindings="
                      << double(after.bindingEvaluations - before.bindingEvaluations) / iterations
                      << " patched=" << double(after.patchedNodes - before.patchedNodes) / iterations
                      << " styled=" << double(after.styledNodes - before.styledNodes) / iterations
                      << " measured=" << double(after.measuredNodes - before.measuredNodes) / iterations
                      << " placed=" << double(after.placedNodes - before.placedNodes) / iterations
                      << " rebuilds=" << double(after.fullRebuilds - before.fullRebuilds) / iterations
                      << '\n';
        }
#endif
    };
    std::string xml = "<Screen version=\"1\"><Column>";
    for (int i = 0; i < 1000; ++i)
        xml += "<Text class=\"label\" value=\"{label}\"/>";
    xml += "</Column></Screen>";
    ui.data().set("label", "Example / Пример");
    measure("XML parse + authored tree (1000)", 20, [&](int) {
        auto p = parseDocument(xml);
        if (!p)
            throw std::runtime_error("parse benchmark");
        sink += p.document->root.children.size();
    });
    Diagnostics d;
    auto sheet = parseStylesheet("Text {font-size:18dp} .label {color:#88ccaa}", "bench", d);
    Node node;
    node.tag = "Text";
    node.attributes["class"] = "label";
    measure("Style resolution (one node)", 5000,
            [&](int) { sink += resolveStyle(node, nullptr, sheet, {}).foreground.r; });
    measure("Runtime tree/style/initial layout", 10, [&](int) {
        if (!ui.loadXml(xml))
            throw std::runtime_error("runtime benchmark");
    });
    ui.update();
    TextSystem text(ui.resources());
    Components components;
    auto layout = *ui.tree();
    LayoutStats stats;
    measure("Full layout (1000 nodes)", 20, [&](int) {
        computeLayout(layout, {960, 640}, text, components, {}, stats);
        sink += stats.nodes;
    });
    measure("Unchanged frame update", 500, [&](int) { ui.update(); });
    measure("Unrelated model update", 500, [&](int i) {
        ui.data().set("unrelated", i);
        ui.update();
    });
    measure("Bound label update + layout", 20, [&](int i) {
        ui.data().set("label", "Label " + std::to_string(i));
        ui.update();
    });
    Runtime list;
    demo::mount(list, argc > 1 ? argv[1] : "examples");
    Value::Array values;
    for (int i = 0; i < 200; ++i)
        values.emplace_back(Value::Object{{"id", std::to_string(i)}, {"name", "Item"}});
    list.data().set("items", values);
    if (!list.loadXml("<Screen version=\"1\"><Column><Repeat id=\"r\" items=\"{items}\" key=\"id\"><Text "
                      "value=\"{item.name}\"/></Repeat></Column></Screen>"))
        return 1;
    measure("Keyed collection rotate (200)", 20, [&](int) {
        std::rotate(values.begin(), values.begin() + 1, values.end());
        list.data().set("items", values);
        list.update();
    });
    measure("Text layout uncached", 100, [&](int i) {
        sink += text.layout("English / Русский " + std::to_string(i), "ui:font", 18, 300)->glyphs.size();
    });
    measure("Text layout cached", 5000,
            [&](int) { sink += text.layout("English / Русский 1", "ui:font", 18, 300)->glyphs.size(); });
    auto svg = ui.resources().get("example:assets/icon.svg");
    if (!svg)
        return 1;
    measure("SVG parse + flatten", 100, [&](int) {
        Diagnostics d;
        auto v = parseSvg(*svg, d);
        if (!v)
            throw std::runtime_error("SVG benchmark");
        sink += v->shapes.size();
    });
    VectorCache vectors;
    auto geometry = vectors.get(*svg, d);
    measure("SVG cached geometry access", 10000, [&](int) { sink += vectors.get(*svg, d)->shapes.size(); });
    DrawList draw;
    measure("Cached vector command emission", 10000, [&](int) {
        draw.commands.clear();
        draw.commands.emplace_back(DrawVectorGeometry{{0, 0, 100, 100}, geometry, 1});
    });
    measure("Draw list (1000 text nodes)", 100, [&](int) { sink += ui.buildDrawList().commands.size(); });
    // The complete model/event -> update -> owning commands path. Static siblings are deliberate.
    Runtime local;
    demo::mount(local, argc > 1 ? argv[1] : "examples");
    local.data().set("label", "A");
    local.data().set("ratio", .5);
    local.data().set("klass", "small");
    local.setStyleOverride(".small {font-size:16dp} .large {font-size:24dp} Button:hover {color:#ff0000}");
    std::string localized = "<Screen version=\"1\"><Column><Row><Text id=\"bound\" value=\"{label}\" "
                            "class=\"{klass}\"/><Button id=\"button\" value=\"Button\"/></Row>"
                            "<ProgressBar id=\"progress\" value=\"{ratio}\"/>";
    for (int i = 0; i < 998; ++i)
        localized += "<Text value=\"Static sibling\"/>";
    localized += "</Column></Screen>";
    if (!local.loadXml(localized))
        return 1;
    auto emit = [&](Runtime &runtime) { sink += runtime.buildDrawList().commands.size(); };
    measure(
        "Pipeline unchanged (1000)", 100,
        [&](int) {
            local.update();
            emit(local);
        },
        &local);
    measure(
        "Pipeline unrelated (1000)", 100,
        [&](int i) {
            local.data().set("unrelated", i);
            local.update();
            emit(local);
        },
        &local);
    measure(
        "Pipeline one text (1000)", 100,
        [&](int i) {
            local.data().set("label", i % 2 ? "B" : "A");
            local.update();
            emit(local);
        },
        &local);
    measure(
        "Pipeline progress (1000)", 100,
        [&](int i) {
            local.data().set("ratio", i % 2 ? .25 : .75);
            local.update();
            emit(local);
        },
        &local);
    measure(
        "Pipeline pseudo state (1000)", 100,
        [&](int i) {
            auto b = local.find("button")->bounds;
            Event e;
            e.type = EventType::pointerMove;
            e.position = i % 2 ? Vec2{b.x + 1, b.y + 1} : Vec2{900, 600};
            local.processEvent(e);
            local.update();
            emit(local);
        },
        &local);
    measure(
        "Pipeline text resize (1000)", 100,
        [&](int i) {
            local.data().set("label", i % 2 ? "Long changing label / Пример" : "A");
            local.update();
            emit(local);
        },
        &local);
    measure(
        "Pipeline class change (1000)", 100,
        [&](int i) {
            local.data().set("klass", i % 2 ? "large" : "small");
            local.update();
            emit(local);
        },
        &local);
    measure(
        "Pipeline keyed rotate (200)", 100,
        [&](int) {
            std::rotate(values.begin(), values.begin() + 1, values.end());
            list.data().set("items", values);
            list.update();
            emit(list);
        },
        &list);
    measure(
        "Pipeline keyed insert (200)", 100,
        [&](int i) {
            auto v = values;
            v.emplace_back(Value::Object{{"id", "insert" + std::to_string(i)}, {"name", "New"}});
            list.data().set("items", std::move(v));
            list.update();
            emit(list);
        },
        &list);
    measure(
        "Pipeline keyed remove (200)", 100,
        [&](int i) {
            auto v = values;
            v.erase(v.begin() + i % 200);
            list.data().set("items", std::move(v));
            list.update();
            emit(list);
        },
        &list);
    measure(
        "Pipeline one list item (200)", 100,
        [&](int i) {
            auto v = values;
            std::get<Value::Object>(v[100].data)["name"] = i % 2 ? "Changed" : "Item";
            list.data().set("items", std::move(v));
            list.update();
            emit(list);
        },
        &list);
    Runtime complex;
    demo::mount(complex, argc > 1 ? argv[1] : "examples");
    complex.data().set("items", values);
    complex.data().set("title", "Worlds");
    complex.data().set("ratio", .5);
    if (!complex.loadXml(
            "<Screen version=\"1\"><Column style=\"width:100%\"><Row><Vector "
            "source=\"example:assets/decor.svg\" "
            "style=\"width:80dp;height:50dp\"/><Text value=\"{title}\"/><Button value=\"Choose\"/></Row>"
            "<ProgressBar value=\"{ratio}\"/><ScrollView style=\"width:90%;height:400dp\">"
            "<Repeat id=\"r\" items=\"{items}\" key=\"id\"><Row><Button value=\"{item.name}\"/>"
            "<Text value=\"Details / Описание\"/></Row></Repeat></ScrollView></Column></Screen>"))
        return 1;
    measure(
        "Pipeline mixed SVG/list screen", 100,
        [&](int i) {
            complex.data().set("title", i % 2 ? "Worlds" : "Миры");
            complex.update();
            emit(complex);
        },
        &complex);
    std::cout << "Observable checksum: " << sink << '\n';
}
