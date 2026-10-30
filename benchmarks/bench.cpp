#include "../apps/common/host.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <voxelui/layout.hpp>
using namespace voxelui;
int main(int argc, char **argv) {
    Runtime ui;
    demo::mount(ui, argc > 1 ? argv[1] : "examples");
    std::size_t sink = 0;
    auto measure = [&](std::string name, int iterations, auto &&f) {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i)
            f(i);
        double us =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() /
            iterations;
        std::cout << std::left << std::setw(34) << name << std::right << std::fixed << std::setprecision(2)
                  << us << " us/op (" << iterations << ")\n";
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
    std::cout << "Observable checksum: " << sink << '\n';
}
