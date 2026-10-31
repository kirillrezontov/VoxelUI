#include "../apps/common/host.hpp"
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <voxelui/runtime.hpp>
#include <voxelui/software.hpp>
using namespace voxelui;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(float a, float b) {
    check(std::abs(a - b) < .1f, "Geometry mismatch");
}
Resource resource(std::string text, std::uint64_t revision = 1) {
    return {"test:asset", {text.begin(), text.end()}, revision};
}
std::string screen(std::string body) {
    return "<Screen version=\"1\">" + body + "</Screen>";
}
void click(Runtime &ui, std::string_view id) {
    auto *n = ui.find(id);
    check(n, "Missing click target");
    Event e;
    e.type = EventType::pointerDown;
    e.position = {n->bounds.x + 3, n->bounds.y + 3};
    ui.processEvent(e);
    e.type = EventType::pointerUp;
    ui.processEvent(e);
    ui.update();
}
void key(Runtime &ui, Key k, bool control = false, bool shift = false) {
    Event e;
    e.type = EventType::keyDown;
    e.key = k;
    e.control = control;
    e.shift = shift;
    ui.processEvent(e);
    ui.update();
}
void input(Runtime &ui, std::string text) {
    Event e;
    e.type = EventType::textInput;
    e.text = std::move(text);
    ui.processEvent(e);
    ui.update();
}
std::uint64_t hash(const std::vector<std::uint8_t> &bytes) {
    std::uint64_t h = 1469598103934665603ull;
    for (auto c : bytes) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}
void sameTree(const LayoutNode &a, const LayoutNode &b) {
    check(a.node.id == b.node.id && a.node.tag == b.node.tag && a.node.text == b.node.text &&
              a.node.attributes == b.node.attributes,
          "Incremental node differs from fresh runtime");
    for (auto pair : {std::pair{a.bounds, b.bounds}, std::pair{a.clip, b.clip}}) {
        near(pair.first.x, pair.second.x);
        near(pair.first.y, pair.second.y);
        near(pair.first.w, pair.second.w);
        near(pair.first.h, pair.second.h);
    }
    near(a.contentSize.x, b.contentSize.x);
    near(a.contentSize.y, b.contentSize.y);
    check(a.style == b.style, "Incremental style differs from fresh runtime");
    check(a.children.size() == b.children.size(), "Incremental children differ");
    for (std::size_t i = 0; i < a.children.size(); ++i)
        sameTree(a.children[i], b.children[i]);
}
void samePixels(Runtime &a, Runtime &b) {
    SoftwareRenderer left, right;
    left.render(a.buildDrawList());
    right.render(b.buildDrawList());
    check(left.width() == right.width() && left.height() == right.height() && left.pixels() == right.pixels(),
          "Incremental pixels differ from fresh runtime");
}
} // namespace
int main() {
    int failures = 0, passed = 0;
    auto test = [&](const char *name, std::function<void()> f) {
        try {
            f();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception &e) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << e.what() << '\n';
        }
    };
    test("XML validation and locations", [] {
        check(bool(parseDocument(screen("<Column><Text value=\"Привет\"/></Column>"))), "Valid document");
        for (auto text : {"<Screen>", "<Screen version=\"2\"/>", "<Screen version=\"1\"><Unknown/></Screen>",
                          "<Screen version=\"1\"><Text><Panel/></Text></Screen>",
                          "<Screen version=\"1\"><Text id=\"x\"/><Text id=\"x\"/></Screen>",
                          "<Screen version=\"1\"><Text typo=\"x\"/></Screen>"})
            check(!parseDocument(text), "Invalid XML accepted");
        auto p = parseDocument("<Screen version=\"1\">\n<Unknown/>\n</Screen>", "bad.xml");
        check(p.diagnostics.front().line == 2 && p.diagnostics.front().source == "bad.xml",
              "Source locations");
    });
    test("XML resource and entity security", [] {
        check(!parseDocument("<!DOCTYPE Screen [<!ENTITY e 'expansion'>]><Screen version=\"1\">&e;</Screen>"),
              "DTD accepted");
        check(!parseDocument(screen("<Image source=\"https://host/a\"/>")), "Network reference accepted");
        check(!validResourceId("x:../secret") && !validResourceId("x:a//b") && !validResourceId("x:/abs") &&
                  !validResourceId("x:a\\b"),
              "Traversal accepted");
        check(validResourceId("pack:ui/main.xml"), "Valid resource rejected");
    });
    test("XML bounds and templates", [] {
        Limits l;
        l.depth = 3;
        check(!parseDocument(screen("<Panel><Panel><Panel/></Panel></Panel>"), "x", {}, l), "Depth limit");
        l = {};
        l.elements = 2;
        check(!parseDocument(screen("<Text/><Text/>"), "x", {}, l), "Element limit");
        check(!parseDocument(screen("<Component template=\"missing\"/>")), "Missing template");
        check(!parseDocument(screen("<Template name=\"a\"><Component template=\"a\"/></Template>")),
              "Template cycle");
        check(bool(parseDocument(
                  screen("<Template name=\"a\"><Text value=\"OK\"/></Template><Component template=\"a\"/>"))),
              "Template accepted");
    });
    test("CSS selectors cascade variables inheritance", [] {
        Diagnostics d;
        auto sheet = parseStylesheet("Text {color:#111111} .label {color:var(--ink)} #title {color:#abcdef} "
                                     "Text:hover {background:#123456}",
                                     "style", d);
        check(!hasErrors(d), "CSS parse");
        Node n;
        n.tag = "Text";
        n.id = "title";
        n.attributes["class"] = "label";
        Style parent;
        parent.fontSize = 29;
        parent.variables["--ink"] = "#567890";
        auto s = resolveStyle(n, &parent, sheet, {"hover"}, &d);
        check(s.foreground == Color{171, 205, 239, 255} && s.background == Color{18, 52, 86, 255},
              "Specificity");
        near(s.fontSize, 29);
        n.attributes["style"] = "color:#fedcba";
        check(resolveStyle(n, &parent, sheet, {}).foreground == Color{254, 220, 186, 255},
              "Inline precedence");
    });
    test("CSS invalid declarations imports cycles", [] {
        Diagnostics d;
        parseStylesheet("Text { fake: 2; width: potato; color: #nope }", "bad", d);
        check(d.size() >= 3 && hasErrors(d), "Bad declarations accepted");
        Resources r;
        r.mount([](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
            std::string s = id == "p:a" ? "@import \"p:b\";" : "@import \"p:a\";";
            return std::vector<std::uint8_t>(s.begin(), s.end());
        });
        d.clear();
        parseStylesheet("@import \"p:a\";", "root", d, &r);
        check(hasErrors(d), "Import cycle accepted");
        d.clear();
        auto sheet = parseStylesheet("Text {--a:var(--b);--b:var(--a);color:var(--a)}", "x", d);
        Node n;
        n.tag = "Text";
        resolveStyle(n, nullptr, sheet, {}, &d);
        check(hasErrors(d), "Variable cycle accepted");
    });
    test("Rows columns stack and constraints", [] {
        Runtime ui;
        check(ui.loadXml(screen(
                  "<Column id=\"col\" style=\"padding:10dp;gap:5dp\"><Row id=\"row\" "
                  "style=\"gap:4dp\"><Panel id=\"a\" style=\"width:20dp;height:30dp\"/><Panel id=\"b\" "
                  "style=\"width:40dp;height:10dp\"/></Row><Stack id=\"stack\"><Panel id=\"c\" "
                  "style=\"width:100dp;height:50dp;min-width:120dp;max-width:80dp\"/></Stack></Column>")),
              "Load layout");
        ui.update();
        near(ui.find("a")->bounds.x, 10);
        near(ui.find("b")->bounds.x, 34);
        near(ui.find("stack")->bounds.y, 45);
        near(ui.find("c")->bounds.w, 120);
    });
    test("Grid responsive percent and anchors", [] {
        Runtime ui;
        check(ui.loadXml(screen("<Grid id=\"g\" columns=\"2\" style=\"width:100%;gap:10dp\"><Panel id=\"a\" "
                                "style=\"height:20dp\"/><Panel id=\"b\" style=\"height:30dp\"/><Panel "
                                "id=\"c\" style=\"height:40dp\"/></Grid><Panel id=\"anchor\" "
                                "anchor=\"bottom-right\" style=\"width:20dp;height:10dp\"/>")),
              "Grid load");
        FrameInfo f;
        f.viewport = {400, 300};
        ui.update(f);
        near(ui.find("b")->bounds.x, 205);
        near(ui.find("c")->bounds.y, 40);
        near(ui.find("anchor")->bounds.x, 380);
        f.viewport = {800, 600};
        ui.update(f);
        near(ui.find("b")->bounds.x, 405);
        near(ui.find("anchor")->bounds.y, 590);
    });
    test("Dirty layout DPI and UI scale", [] {
        Runtime ui;
        check(ui.loadXml(screen("<Panel id=\"p\" style=\"width:50%;height:100dp\"/>")), "Load");
        FrameInfo f;
        f.viewport = {400, 300};
        ui.update(f);
        auto passes = ui.layoutStats().passes;
        ui.update(f);
        ui.buildDrawList();
        check(ui.layoutStats().passes == passes, "Unchanged frames relayout");
        f.uiScale = 2;
        f.dpiScale = 1.5f;
        ui.update(f);
        near(ui.find("p")->bounds.w, 100);
        auto list = ui.buildDrawList();
        SoftwareRenderer renderer;
        renderer.render(list);
        check(renderer.width() == 600 && renderer.height() == 450, "DPI framebuffer");
    });
    test("Nested clipping scroll and hit testing", [] {
        Runtime ui;
        check(ui.loadXml(screen("<ScrollView id=\"scroll\" style=\"height:50dp;width:100dp\"><Panel "
                                "id=\"one\" style=\"width:100dp;height:60dp\"/><Button id=\"two\" "
                                "style=\"width:100dp;height:40dp\">Two</Button></ScrollView>")),
              "Load scroll");
        ui.update();
        check(!ui.find("two")->clip.contains({10, 70}), "Clip");
        Event e;
        e.type = EventType::wheel;
        e.position = {10, 10};
        e.delta = -100;
        check(ui.processEvent(e), "Wheel ownership");
        ui.update();
        near(ui.state("scroll")->scroll.y, 50);
        near(ui.find("two")->bounds.y, 10);
        check(hitTest(*ui.tree(), {10, 15})->node.id == "two", "Final geometry hit test");
    });
    test("SVG shapes transforms paths clips cache", [] {
        auto r = resource(
            R"svg(<svg viewBox="0 0 100 100"><defs><clipPath id="c"><rect width="50" height="50"/></clipPath></defs><g transform="translate(10 20) scale(2)"><path d="M0 0 L10 0 H20 V10 Q20 20 10 20 C0 20 0 10 0 0 Z" fill="#ff0000" stroke="#ffffff"/></g><rect width="80" height="80" clip-path="url(#c)"/></svg>)svg");
        Diagnostics d;
        VectorCache cache;
        auto a = cache.get(r, d);
        check(a && !hasErrors(d) && a->shapes.size() == 2, "SVG parse");
        near(a->shapes[0].contours[0][0].x, 10);
        near(a->shapes[0].contours[0][0].y, 20);
        check(a->shapes[1].clip.has_value(), "SVG clip");
        check(cache.get(r, d) == a, "SVG cache miss");
        r.revision = 2;
        check(cache.get(r, d) != a, "SVG stale cache");
    });
    test("SVG arcs malformed and hostile assets", [] {
        Diagnostics d;
        auto a = parseSvg(
            resource("<svg viewBox=\"0 0 100 100\"><path d=\"M10 50 A40 40 0 1 1 90 50 Z\"/></svg>"), d);
        check(a && a->shapes[0].contours[0].size() > 10, "Arc flattening");
        for (auto body : {"<script/>", "<foreignObject/>", "<image href=\"https://host/image\"/>",
                          "<path d=\"Mwat\"/>"}) {
            d.clear();
            check(!parseSvg(resource("<svg viewBox=\"0 0 10 10\">" + std::string(body) + "</svg>"), d),
                  "Unsafe SVG accepted");
        }
    });
    test("Observable typed data and missing paths", [] {
        DataModel d;
        check(d.set("player.name", "Иван"), "Set");
        check(d.get("player.name")->string() == "Иван", "Read");
        auto revision = d.revision();
        d.set("player.name", "Иван");
        check(revision == d.revision(), "Spurious change");
        check(!d.get("missing") && !d.set("bad..path", 1), "Invalid paths");
        Runtime ui;
        ui.data().set("ratio", "wrong type");
        check(!ui.loadXml(screen("<Slider value=\"{ratio}\"/>")), "Type mismatch accepted");
        check(ui.loadXml(screen("<Text value=\"{missing}\"/>")), "Missing value recovery");
        check(!ui.diagnostics().empty(), "Missing binding diagnostic");
    });
    test("Binding update and keyed reconciliation", [] {
        Runtime ui;
        ui.data().set("title", "First");
        ui.data().set("items", Value::Array{Value::Object{{"id", "a"}, {"name", "Alpha"}},
                                            Value::Object{{"id", "b"}, {"name", "Beta"}}});
        check(ui.loadXml(screen(
                  "<Column><Text id=\"title\" value=\"{title}\"/><Repeat id=\"list\" items=\"{items}\" "
                  "key=\"id\"><Button id=\"row\" value=\"{item.name}\"/></Repeat></Column>")),
              "Load binding");
        ui.update();
        click(ui, "list/61/row");
        check(ui.state("list/61/row")->selected, "Selection");
        ui.data().set("title", "Second");
        ui.data().set("items", Value::Array{Value::Object{{"id", "b"}, {"name", "Beta"}},
                                            Value::Object{{"id", "a"}, {"name", "Renamed"}}});
        ui.update();
        check(ui.find("title")->node.attr("value") == "Second", "Visible update");
        check(ui.state("list/61/row")->selected && ui.focusedId() == "list/61/row", "Keyed state/focus lost");
        ui.data().set("items", Value::Array{});
        ui.update();
        check(ui.focusedId().empty() && !ui.state("list/61/row"), "Removed focused element");
    });
    test("Template parameters and expansion budget", [] {
        Runtime ui;
        check(
            ui.loadXml(screen("<Template name=\"Card\"><Text id=\"label\" "
                              "value=\"{props.title}\"/></Template><Component id=\"one\" template=\"Card\" "
                              "title=\"Hello\"/><Component id=\"two\" template=\"Card\" title=\"Привет\"/>")),
            "Template load");
        check(ui.find("one/label")->node.attr("value") == "Hello" &&
                  ui.find("two/label")->node.attr("value") == "Привет",
              "Template props");
    });
    test("Typed actions capability lifetime and errors", [] {
        Actions a;
        int called = 0;
        a.registerAction(
            "test:run",
            {{{"count", ValueType::number}}, "write", [&](const auto &) -> std::optional<std::string> {
                 ++called;
                 a.unregisterAction("test:run");
                 return {};
             }});
        ActionContext c{"button", "test:run", {{"count", Value(3)}}};
        check(a.invoke(c).has_value(), "Capability bypass");
        a.grant("write");
        c.parameters["count"] = "3";
        check(a.invoke(c).has_value(), "Parameter mismatch");
        c.parameters["count"] = 3;
        check(!a.invoke(c) && called == 1, "Action dispatch");
        check(a.invoke(c).has_value(), "Unregistered action");
        Runtime ui;
        check(!ui.loadXml(screen("<Button action=\"test:unknown\"/>")), "Unknown action accepted");
    });
    test("Pointer actions focus and keyboard navigation", [] {
        Runtime ui;
        int calls = 0;
        ui.actions().registerAction("test:run", {{}, "", [&](const auto &) -> std::optional<std::string> {
                                                     ++calls;
                                                     return {};
                                                 }});
        check(ui.loadXml(
                  screen("<Column><Button id=\"a\" action=\"test:run\"><Text value=\"Run\"/></Button><Button "
                         "id=\"disabled\" disabled=\"true\"/><Button id=\"b\"/></Column>")),
              "Input load");
        ui.update();
        click(ui, "a");
        check(calls == 1 && ui.focusedId() == "a", "Nested text targeting");
        key(ui, Key::tab);
        check(ui.focusedId() == "b", "Tab skip disabled");
        key(ui, Key::tab, false, true);
        key(ui, Key::enter);
        check(calls == 2, "Keyboard activation");
    });
    test("English Cyrillic selection clipboard composition", [] {
        Runtime ui;
        ui.data().set("name", "");
        check(ui.loadXml(screen("<TextInput id=\"input\" value=\"{name}\" writable=\"true\"/>")),
              "Text load");
        ui.update();
        key(ui, Key::tab);
        input(ui, "Hello Привет");
        check(ui.data().get("name")->string() == "Hello Привет", "UTF8 writable binding");
        key(ui, Key::backspace);
        check(ui.state("input")->text == "Hello Приве", "Codepoint backspace");
        key(ui, Key::left);
        key(ui, Key::del);
        check(ui.state("input")->text == "Hello Прив", "Delete");
        std::string clipboard;
        ui.clipboard([&] { return clipboard; }, [&](std::string s) { clipboard = std::move(s); });
        key(ui, Key::a, true);
        key(ui, Key::x, true);
        check(ui.state("input")->text.empty() && clipboard == "Hello Прив", "Cut selection");
        key(ui, Key::v, true);
        check(ui.state("input")->text == clipboard, "Paste");
        Event e;
        e.type = EventType::composition;
        e.text = "я";
        ui.processEvent(e);
        check(ui.state("input")->composition == "я", "Composition");
        input(ui, "я");
        check(ui.state("input")->composition.empty(), "Composition commit");
    });
    test("Unicode invalid input and combining marks", [] {
        auto cps = decodeUtf8("A\xc0\xafБ");
        check(cps.size() == 4 && cps[1] == 0xfffd, "Malformed UTF8");
        auto b = utf8Boundaries("e\xcc\x81Б");
        check(b == std::vector<std::size_t>{0, 3, 5}, "Combining boundary");
    });
    test("Toggle slider select writable bindings", [] {
        Runtime ui;
        ui.data().set("on", false);
        ui.data().set("level", .5);
        ui.data().set("choice", "A");
        check(ui.loadXml(
                  screen("<Column><Checkbox id=\"on\" value=\"{on}\" writable=\"true\"/><Slider "
                         "id=\"slider\" value=\"{level}\" writable=\"true\" step=\"0.1\"/><Select "
                         "id=\"choice\" options=\"A|B|C\" value=\"{choice}\" writable=\"true\"/></Column>")),
              "Controls load");
        ui.update();
        click(ui, "on");
        check(ui.data().get("on")->truth(), "Checkbox write");
        key(ui, Key::tab);
        key(ui, Key::right);
        check(std::abs(ui.data().get("level")->numeric().value() - .6) < .001, "Slider keyboard");
        click(ui, "choice");
        check(ui.data().get("choice")->string() == "B", "Select write");
    });
    test("Modal capture dragging and routing", [] {
        Runtime ui;
        check(ui.loadXml(screen(
                  "<Button id=\"behind\"/><Modal id=\"modal\" style=\"width:200dp;height:100dp\"><Button "
                  "id=\"drag\" draggable=\"true\" style=\"width:80dp;height:40dp\"/></Modal>")),
              "Modal load");
        ui.update();
        key(ui, Key::tab);
        check(ui.focusedId() == "drag", "Modal tab trap");
        std::vector<EventType> events;
        ui.observeEvents([&](RoutedEvent &e) { events.push_back(e.event.type); });
        Event e;
        e.type = EventType::pointerDown;
        e.position = {ui.find("drag")->bounds.x + 5, ui.find("drag")->bounds.y + 5};
        ui.processEvent(e);
        e.type = EventType::pointerMove;
        e.position = {250, 200};
        check(ui.processEvent(e), "Pointer capture outside");
        e.type = EventType::pointerUp;
        ui.processEvent(e);
        check(std::ranges::find(events, EventType::dragStart) != events.end() &&
                  std::ranges::find(events, EventType::dragEnd) != events.end(),
              "Drag lifecycle");
    });
    test("Resources priority revisions ownership and symlinks", [] {
        Resources r;
        std::string content = "one";
        r.mount([&](auto) -> std::optional<std::vector<std::uint8_t>> {
            return std::vector<std::uint8_t>(content.begin(), content.end());
        });
        auto first = r.get("test:a");
        content = "two";
        check(r.refresh(), "Reload notification");
        auto second = r.get("test:a");
        check(first->revision != second->revision &&
                  std::string(first->bytes.begin(), first->bytes.end()) == "one",
              "Owning resource snapshots");
        auto token = r.mount(
            [](auto) -> std::optional<std::vector<std::uint8_t>> { return std::vector<std::uint8_t>{'x'}; },
            10);
        check(r.get("test:a")->bytes[0] == 'x', "Priority");
        r.unmount(token);
        check(r.get("test:a")->bytes[0] == 't', "Unmount");
        Resources files;
        files.mountDirectory("test", VOXELUI_SOURCE_DIR);
        check(!files.get("test:../outside"), "Directory traversal");
    });
    test("Transactional hot reload and safe state preservation", [] {
        Runtime ui;
        std::string content = screen("<TextInput id=\"input\" value=\"original\"/>");
        ui.resources().mount([&](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
            if (id != "test:screen")
                return {};
            return std::vector<std::uint8_t>(content.begin(), content.end());
        });
        check(ui.load("test:screen"), "Initial load");
        ui.update();
        key(ui, Key::tab);
        key(ui, Key::end);
        input(ui, "!");
        content = "<broken>";
        ui.resources().refresh();
        ui.update();
        check(ui.find("input") && ui.state("input")->text == "original!" && hasErrors(ui.diagnostics()),
              "Last good lost");
        content = screen("<TextInput id=\"input\" value=\"new\"/>");
        ui.resources().refresh();
        ui.update();
        check(ui.state("input")->text == "original!" && ui.focusedId() == "input", "Safe reload state");
        content = screen("<Button id=\"input\"/>");
        ui.resources().refresh();
        ui.update();
        check(ui.state("input")->text.empty(), "Type changed state not reset");
        ui.close();
        check(!ui.tree() && ui.focusedId().empty(), "Close lifecycle");
    });
    test("Custom widget and dynamic SVG slots", [] {
        Runtime ui;
        int drawn = 0;
        ui.components().registerWidget("Badge", {[](const Node &, Vec2) { return Vec2{40, 20}; },
                                                 [&](const WidgetContext &, DrawList &) { ++drawn; },
                                                 {}});
        check(ui.loadXml(screen("<Badge id=\"badge\"/>")), "Custom tag");
        ui.update();
        near(ui.find("badge")->bounds.w, 40);
        ui.buildDrawList();
        check(drawn == 1, "Custom draw");
        auto r = resource("<svg viewBox=\"0 0 100 100\"><g id=\"a\" data-voxelui-slot=\"badge\" "
                          "transform=\"translate(10 20)\" data-width=\"30\" data-height=\"40\"/></svg>");
        Diagnostics d;
        auto v = parseSvg(r, d);
        check(v && v->slots.size() == 1 && v->slots[0].name == "badge", "Slot parse");
        near(v->slots[0].transform.e, 10);
    });
    test("Text measurement fallback and cache", [] {
        Resources r; // font is loaded through an isolated resolver
        r.mountDirectory("font", std::filesystem::path(VOXELUI_SOURCE_DIR) / "examples/assets");
        TextSystem text(r);
        auto a = text.layout("Hello Привет", "font:font.ttf", 18, 100);
        check(a->size.y > 23 && a->glyphs.size() == 12, "UTF8 wrapped measure");
        check(a == text.layout("Hello Привет", "font:font.ttf", 18, 100), "Text layout cache");
        auto b = text.layout("Missing", "missing:font", 18);
        check(b->size.x > 0 && !b->glyphs.empty(), "Missing font fallback");
    });
    test("Draw ordering clipping transforms deterministic pixels", [] {
        DrawList list;
        list.logicalSize = {64, 64};
        list.commands = {DrawRectangle{{0, 0, 64, 64}, {10, 20, 30, 255}, {}, 0, 0},
                         PushClip{{8, 8, 40, 40}},
                         DrawRectangle{{0, 0, 64, 64}, {200, 60, 40, 255}, {}, 0, 0},
                         PushTransform{{1, 0, 0, 1, 16, 16}},
                         DrawRectangle{{0, 0, 40, 40}, {20, 180, 100, 128}, {}, 0, 0},
                         PopTransform{},
                         PopClip{},
                         DrawRectangle{{52, 4, 8, 8}, {240, 230, 80, 255}, {}, 0, 2}};
        SoftwareRenderer r;
        r.render(list);
        auto expected = r.pixels();
        auto h = hash(expected);
        r.render(list);
        check(r.pixels() == expected, "Nondeterministic rendering");
        auto at = [&](int x, int y, int c) { return expected[(y * 64 + x) * 4 + c]; };
        check(at(0, 0, 0) == 10 && at(10, 10, 0) == 200 && at(20, 20, 0) == 110 && at(50, 20, 0) == 10,
              "Stack/clip/alpha pixels");
        std::cout << "Reference primitive hash: " << h << '\n';
        check(h == 12769330923178300131ull, "Reference screenshot hash changed");
    });
    test("All examples validate render and survive resizing", [] {
        auto root = std::filesystem::path(VOXELUI_SOURCE_DIR) / "examples";
        for (auto name : {"main_menu", "settings", "inventory", "world_selection", "hud", "svg_gallery"}) {
            Runtime ui;
            demo::mount(ui, root);
            demo::mockData(ui);
            demo::actions(ui);
            check(ui.load("example:" + std::string(name) + "/main.xml"), "Example loading");
            for (auto size : {Vec2{960, 640}, Vec2{1280, 720}}) {
                FrameInfo f;
                f.viewport = size;
                ui.update(f);
                SoftwareRenderer renderer;
                auto draw = ui.buildDrawList();
                renderer.render(draw);
                check(renderer.width() == int(size.x) && draw.commands.size() > 10, "Example frame");
            }
        }
    });

    test("Unrelated data avoids layout and overrides respect precedence", [] {
        Runtime ui;
        ui.data().set("label", "A");
        check(ui.setStyleOverride("Text {color:#123456}"), "Set override");
        check(ui.loadXml(screen("<Text id=\"t\" value=\"{label}\"/>")), "Load override");
        ui.update();
        check(ui.find("t")->style.foreground == Color{18, 52, 86, 255}, "Override color");
        auto passes = ui.layoutStats().passes;
        ui.data().set("unrelated", 42);
        ui.update();
        check(passes == ui.layoutStats().passes, "Unrelated model triggered layout");
        ui.data().set("label", "B");
        ui.update();
        check(ui.layoutStats().passes > passes, "Relevant data not invalidated");
    });
    test("Transition frames interpolate without layout", [] {
        Runtime ui;
        check(ui.loadXml(screen("<Button id=\"b\" style=\"transition:0.2s\">B</Button>")), "Transition load");
        ui.update();
        auto old = ui.find("b")->style.background;
        // A theme hot reload creates a target style with a transition.
        std::string xml = screen("<Button id=\"b\" style=\"background:#ff0000;transition:0.2s\">B</Button>");
        check(ui.loadXml(xml), "Transition reload");
        FrameInfo f;
        f.deltaSeconds = .1f;
        ui.update(f);
        auto middle = ui.find("b")->style.background;
        check(middle.r > old.r && middle.r < 255, "No interpolation");
        auto passes = ui.layoutStats().passes;
        ui.update(f);
        check(ui.find("b")->style.background.r == 255 && passes == ui.layoutStats().passes,
              "Transition relayout");
    });
    test("SVG slot local input and capture", [] {
        Runtime ui;
        ui.resources().mount([](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
            if (id != "test:vector")
                return {};
            std::string svg =
                R"svg(<svg viewBox="0 0 100 100"><g id="slot" data-voxelui-slot="host.slot" transform="translate(10 20)" data-width="30" data-height="40"/></svg>)svg";
            return std::vector<std::uint8_t>(svg.begin(), svg.end());
        });
        int calls = 0;
        Vec2 position;
        ui.registerSlot("host.slot", {{},
                                      [](const auto &, auto &) {},
                                      [&](const auto &, const Event &e) {
                                          ++calls;
                                          position = e.position;
                                          return true;
                                      }});
        check(ui.loadXml(
                  screen("<Vector id=\"v\" source=\"test:vector\" style=\"width:200dp;height:200dp\"/>")),
              "Slot screen");
        ui.update();
        Event e;
        e.type = EventType::pointerDown;
        e.position = {30, 50};
        check(ui.processEvent(e), "Slot consumption");
        near(position.x, 5);
        near(position.y, 5);
        e.type = EventType::pointerMove;
        e.position = {400, 400};
        ui.processEvent(e);
        e.type = EventType::pointerUp;
        ui.processEvent(e);
        check(calls == 3, "Slot capture");
    });
    test("SVG resolver image and invalid asset reload keep visuals", [] {
        Runtime ui;
        auto root = std::filesystem::path(VOXELUI_SOURCE_DIR) / "examples";
        demo::mount(ui, root);
        std::string xml =
            screen("<Vector id=\"v\" source=\"test:vector\" style=\"width:200dp;height:100dp\"/>");
        std::string svg =
            R"svg(<svg viewBox="0 0 200 100"><image href="example:assets/landscape.png" width="200" height="100"/></svg>)svg";
        ui.resources().mount([&](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
            auto *content = id == "test:screen" ? &xml : id == "test:vector" ? &svg : nullptr;
            if (!content)
                return {};
            return std::vector<std::uint8_t>(content->begin(), content->end());
        });
        check(ui.load("test:screen"), "SVG image load");
        ui.update();
        SoftwareRenderer renderer;
        renderer.render(ui.buildDrawList());
        auto before = renderer.pixels();
        svg = "<invalid>";
        ui.resources().refresh();
        ui.update();
        check(hasErrors(ui.diagnostics()), "Bad SVG accepted");
        renderer.render(ui.buildDrawList());
        check(renderer.pixels() == before, "Last good SVG pixels lost");
    });
    test("Bad collection update retains last good tree", [] {
        Runtime ui;
        ui.data().set("items", Value::Array{Value::Object{{"id", "a"}}});
        check(ui.loadXml(screen("<Column><Repeat id=\"r\" items=\"{items}\" key=\"id\"><Button "
                                "id=\"b\"/></Repeat></Column>")),
              "Load collection");
        ui.update();
        ui.data().set("items", Value::Array{Value::Object{{"id", "same"}}, Value::Object{{"id", "same"}}});
        ui.update();
        check(hasErrors(ui.diagnostics()) && ui.find("r/61/b"), "Bad update destroyed active tree");
    });
    test("Draw resources outlive runtime and callbacks may close screens", [] {
        DrawList draw;
        {
            Runtime ui;
            check(ui.loadXml(screen("<Text value=\"Survives\"/>")), "Load");
            ui.update();
            draw = ui.buildDrawList();
        }
        SoftwareRenderer renderer;
        renderer.render(draw);
        check(!renderer.pixels().empty(), "Draw lifetime");
        Runtime ui;
        ui.actions().registerAction("test:close", {{}, "", [&](const auto &) -> std::optional<std::string> {
                                                       ui.close();
                                                       return {};
                                                   }});
        check(ui.loadXml(screen("<Button id=\"close\" action=\"test:close\"/>")), "Close screen");
        ui.update();
        key(ui, Key::tab);
        key(ui, Key::enter);
        check(!ui.tree(), "Close callback");
    });

    test("Font failure preserves glyphs and font weight renders", [] {
        Runtime ui;
        auto root = std::filesystem::path(VOXELUI_SOURCE_DIR) / "examples";
        demo::mount(ui, root);
        std::ifstream f(root / "assets/font.ttf", std::ios::binary);
        std::vector<std::uint8_t> font((std::istreambuf_iterator<char>(f)), {});
        std::string xml = screen("<Text value=\"Привет\" style=\"font-weight:700\"/>");
        ui.resources().mount(
            [&](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
                if (id == "ui:font")
                    return font;
                if (id == "test:screen")
                    return std::vector<std::uint8_t>(xml.begin(), xml.end());
                return {};
            },
            10);
        check(ui.load("test:screen"), "Font screen");
        ui.update();
        SoftwareRenderer renderer;
        renderer.render(ui.buildDrawList());
        auto before = renderer.pixels();
        font = {1, 2, 3};
        ui.resources().refresh();
        ui.update();
        check(hasErrors(ui.diagnostics()), "Bad font accepted");
        renderer.render(ui.buildDrawList());
        check(before == renderer.pixels(), "Known good font lost");
    });
    test("Load validates action parameter schemas without invocation", [] {
        Runtime ui;
        int calls = 0;
        ui.actions().registerAction(
            "test:typed",
            {{{"value", ValueType::number}}, "", [&](const auto &) -> std::optional<std::string> {
                 ++calls;
                 return {};
             }});
        check(!ui.loadXml(screen("<Button action=\"test:typed\"/>")), "Missing action parameter accepted");
        ui.data().set("number", 3);
        check(ui.loadXml(screen("<Button id=\"b\" action=\"test:typed\" value=\"{number}\"/>")),
              "Typed action load");
        check(calls == 0, "Validation invoked action");
    });
    test("Invalid UTF8 boundaries stay within byte storage", [] {
        auto boundaries = utf8Boundaries("\xc0\xaf\xf0");
        check(boundaries == std::vector<std::size_t>{0, 1, 2, 3}, "Invalid UTF8 offsets escaped storage");
    });

    test("Batch runs preserve order resources and clip barriers", [] {
        DrawList list;
        auto image = std::make_shared<Image>();
        list.commands = {DrawRectangle{},
                         DrawRectangle{},
                         PushClip{{0, 0, 10, 10}},
                         DrawRectangle{},
                         DrawImage{{}, image, 1},
                         DrawImage{{}, image, 1},
                         DrawImage{{}, std::make_shared<Image>(), 1},
                         PopClip{}};
        list.rebuildBatches();
        check(list.batches.size() == 6 && list.batches[0].count == 2 && list.batches[3].count == 2,
              "Batch grouping");
        std::size_t index = 0;
        for (auto &batch : list.batches) {
            check(batch.first == index, "Reordered batch");
            index += batch.count;
        }
        check(index == list.commands.size(), "Lost commands");
    });
    test("Missing initial image uses visible fallback and recovers", [] {
        Runtime ui;
        check(ui.loadXml(screen("<Image id=\"image\" source=\"missing:icon\"/>")), "Fallback screen");
        ui.update();
        check(!ui.diagnostics().empty(), "Missing fallback diagnostic");
        SoftwareRenderer renderer;
        renderer.render(ui.buildDrawList());
        check(renderer.pixels()[0] != 0, "Invisible fallback");
    });

    test("Raster reuse and collection input budgets", [] {
        Resources resources;
        resources.mountDirectory("asset", std::filesystem::path(VOXELUI_SOURCE_DIR) / "examples/assets");
        Diagnostics d;
        auto svg = resource(
            R"svg(<svg viewBox="0 0 100 100"><image href="asset:landscape.png" width="50" height="50"/><image href="asset:landscape.png" x="50" width="50" height="50"/></svg>)svg");
        auto parsed = parseSvg(svg, d, {}, &resources);
        check(parsed && parsed->shapes.size() == 2 && parsed->shapes[0].image == parsed->shapes[1].image,
              "Repeated SVG images decoded separately");
        check(!parseDocument(screen("<Text id=\"" + std::string(257, 'a') + "\"/>")), "Unbounded element ID");
        Runtime ui;
        ui.data().set("items", Value::Array(10001, Value::Object{{"id", "duplicate"}}));
        check(!ui.loadXml(screen("<Repeat items=\"{items}\" key=\"id\"><Text/></Repeat>")),
              "Oversized collection accepted");
    });
    test("Localized bindings preserve tree storage and cached measurement", [] {
        Runtime ui;
        ui.data().set("player.health", "A");
        ui.data().set("player.name", "Name");
        int measured = 0;
        ui.components().registerWidget("External", {[&](const Node &, Vec2) {
                                                        ++measured;
                                                        return Vec2{20, 20};
                                                    },
                                                    [](const auto &, auto &) {},
                                                    {}});
        auto xml =
            screen("<Column><Text id=\"health\" value=\"{player.health}\"/><Text id=\"name\" "
                   "value=\"{player.name}\"/><External id=\"external\"/><TextInput id=\"input\"/></Column>");
        check(ui.loadXml(xml), "Localized load");
        ui.update();
        key(ui, Key::tab);
        auto *root = ui.tree();
        auto *name = ui.find("name");
        auto *health = ui.find("health");
        auto rebuilds = ui.runtimeStats().fullRebuilds, evaluations = ui.runtimeStats().bindingEvaluations;
        int before = measured;
        ui.data().set("player.health", "Longer / Здоровье");
        ui.update();
        check(ui.tree() == root && ui.find("name") == name && ui.find("health") == health,
              "Computed nodes reconstructed");
        check(ui.focusedId() == "input" && measured == before, "Unrelated state/measurement changed");
        check(ui.runtimeStats().fullRebuilds == rebuilds &&
                  ui.runtimeStats().bindingEvaluations == evaluations + 1,
              "Unrelated binding evaluated");
        check(ui.layoutStats().nodes == 3, "Measurement escaped ancestor chain");
        ui.data().set("player", Value::Object{{"health", "B"}, {"name", "New"}});
        ui.update();
        check(ui.find("name")->node.attr("value") == "New", "Ancestor dependency missed");
        ui.data().set("player.health.extra", "invalid");
        ui.update();
        check(ui.find("health")->node.attr("value") == "B", "Invalid descendant write changed scalar");
    });
    test("Progress and pseudo state avoid expansion and measurement", [] {
        Runtime ui;
        ui.data().set("ratio", .2);
        ui.setStyleOverride("Button:hover {color:#ff0000}");
        check(ui.loadXml(screen("<ProgressBar id=\"p\" value=\"{ratio}\"/><Button id=\"b\" value=\"B\"/>")),
              "Load");
        ui.update();
        auto passes = ui.layoutStats().passes;
        auto builds = ui.runtimeStats().fullRebuilds;
        ui.data().set("ratio", .8);
        ui.update();
        check(ui.layoutStats().passes == passes && ui.runtimeStats().fullRebuilds == builds,
              "Progress recomputed layout");
        Event e;
        e.type = EventType::pointerMove;
        e.position = {3, ui.find("b")->bounds.y + 3};
        ui.processEvent(e);
        ui.update();
        check(ui.find("b")->style.foreground == Color{255, 0, 0, 255} && ui.layoutStats().passes == passes,
              "Pseudo paint recomputed layout or stale style");
    });
    test("Incremental geometry styles and pixels equal fresh recomputation", [] {
        auto xml = screen(
            "<Template name=\"T\"><Row style=\"width:100%;gap:3dp\"><Text id=\"text\" "
            "value=\"{props.label}\"/>"
            "<ProgressBar id=\"p\" value=\"{props.ratio}\" style=\"width:20%\"/></Row></Template>"
            "<Column class=\"{klass}\" style=\"width:100%\"><Component id=\"card\" template=\"T\" "
            "label=\"{label}\" ratio=\"{ratio}\"/>"
            "<ScrollView style=\"width:80%;height:110dp\"><Repeat id=\"r\" items=\"{items}\" key=\"id\">"
            "<Row><Text id=\"t\" value=\"{item.name}\"/><Vector source=\"example:assets/decor.svg\" "
            "style=\"width:24dp;height:18dp\"/></Row>"
            "</Repeat></ScrollView><Panel anchor=\"bottom-right\" "
            "style=\"width:20dp;height:10dp\"/></Column>");
        const std::string css = ".small {font-size:14dp;color:#aabbcc} .large {font-size:23dp;color:#ccbb88}";
        auto assets = std::filesystem::path(VOXELUI_SOURCE_DIR) / "examples";
        Runtime incremental;
        demo::mount(incremental, assets);
        incremental.setStyleOverride(css);
        auto seed = [&](Runtime &ui, int i) {
            ui.data().set("label", i % 2 ? "English / Пример с переносом" : "A");
            ui.data().set("ratio", (i % 9) / 10.0);
            ui.data().set("klass", i % 3 ? "small" : "large");
            Value::Array items;
            for (int k = 0; k < 6; ++k)
                items.emplace_back(Value::Object{{"id", std::to_string(k)},
                                                 {"name", k == i % 6 ? "Changed / Изменено" : "Item"}});
            ui.data().set("items", std::move(items));
        };
        seed(incremental, 0);
        check(incremental.loadXml(xml), "Incremental scene load");
        for (int i = 1; i <= 12; ++i) {
            FrameInfo frame;
            frame.viewport = i % 2 ? Vec2{320, 240} : Vec2{480, 320};
            frame.uiScale = i % 3 ? 1.f : 1.5f;
            frame.dpiScale = i % 4 ? 1.f : 1.25f;
            seed(incremental, i);
            incremental.update(frame);
            Runtime fresh;
            demo::mount(fresh, assets);
            fresh.setStyleOverride(css);
            seed(fresh, i);
            fresh.update(frame);
            check(fresh.loadXml(xml), "Fresh scene load");
            fresh.update(frame);
            sameTree(*incremental.tree(), *fresh.tree());
            samePixels(incremental, fresh);
        }
    });
    test("Keyed item properties and nested template aliases patch in place", [] {
        Runtime ui;
        ui.data().set("items", Value::Array{Value::Object{{"id", "a"}, {"name", "First"}}});
        auto xml = screen("<Template name=\"Inner\"><Text id=\"text\" value=\"{props.label}\"/></Template>"
                          "<Template name=\"Outer\"><Component id=\"inner\" template=\"Inner\" "
                          "label=\"{props.name}\"/></Template>"
                          "<Repeat id=\"r\" items=\"{items}\" key=\"id\"><Component id=\"outer\" "
                          "template=\"Outer\" name=\"{item.name}\"/></Repeat>");
        check(ui.loadXml(xml), "Nested template load");
        ui.update();
        auto *text = ui.find("r/61/outer/inner/text");
        check(text, "Nested text missing");
        auto builds = ui.runtimeStats().fullRebuilds;
        ui.data().set("items", Value::Array{Value::Object{{"id", "a"}, {"name", "Second"}}});
        ui.update();
        check(ui.find(text->node.id) == text && text->node.attr("value") == "Second" &&
                  ui.runtimeStats().fullRebuilds == builds,
              "Item update expanded tree");
    });
    test("Removal clears hover focus capture drag and clamps scroll", [] {
        Runtime ui;
        auto rows = [](int count) {
            Value::Array v;
            for (int i = 0; i < count; ++i)
                v.emplace_back(Value::Object{{"id", std::to_string(i)}});
            return v;
        };
        ui.data().set("items", rows(5));
        check(ui.loadXml(screen("<ScrollView id=\"s\" style=\"height:50dp;width:100dp\"><Repeat id=\"r\" "
                                "items=\"{items}\" key=\"id\">"
                                "<Button id=\"b\" draggable=\"true\" tooltip=\"tip\" "
                                "style=\"width:100dp;height:40dp\"/></Repeat></ScrollView>")),
              "Load");
        ui.update();
        Event e;
        e.type = EventType::pointerMove;
        e.position = {3, 3};
        ui.processEvent(e);
        e.type = EventType::pointerDown;
        ui.processEvent(e);
        ui.update();
        check(!ui.focusedId().empty(), "Focus missing");
        ui.data().set("items", Value::Array{});
        ui.update();
        check(ui.focusedId().empty() && !ui.state("r/30/b"), "Removed focus retained");
        e.type = EventType::pointerUp;
        ui.processEvent(e);
        ui.update();
        check(!hasErrors(ui.diagnostics()), "Stale captured target dispatched");
        ui.data().set("items", rows(5));
        ui.update();
        e.type = EventType::wheel;
        e.delta = -100;
        e.position = {3, 3};
        ui.processEvent(e);
        ui.update();
        check(ui.state("s")->scroll.y > 0, "Scroll missing");
        ui.data().set("items", rows(1));
        ui.update();
        near(ui.state("s")->scroll.y, 0);
        near(ui.find("r/30/b")->bounds.y, 0);
    });
    test("Bound visibility and inherited class update hit testing", [] {
        Runtime ui;
        ui.data().set("visible", "true");
        ui.data().set("class", "small");
        ui.setStyleOverride(".small {font-size:12dp} .large {font-size:32dp}");
        auto xml = screen("<Column class=\"{class}\"><Button id=\"b\" visible=\"{visible}\" "
                          "value=\"Test\"/><Button id=\"next\"/></Column>");
        check(ui.loadXml(xml), "Visibility load");
        ui.update();
        key(ui, Key::tab);
        ui.data().set("class", "large");
        ui.update();
        near(ui.find("b")->style.fontSize, 32);
        ui.data().set("visible", "false");
        ui.update();
        check(ui.focusedId().empty() && !ui.find("b")->style.visible, "Hidden focus retained");
        check(hitTest(*ui.tree(), {2, 2})->node.id != "b", "Hidden hit region retained");
    });
    test("Invalid incremental values and styles retain all last good patches", [] {
        Runtime ui;
        ui.data().set("text", "A");
        ui.data().set("ratio", .5);
        ui.data().set("style", "font-size:18dp");
        check(ui.loadXml(screen("<Text id=\"text\" value=\"{text}\" style=\"{style}\"/><Slider id=\"slider\" "
                                "value=\"{ratio}\"/>")),
              "Load");
        ui.update();
        auto *before = ui.tree();
        ui.data().set("text", "B");
        ui.data().set("ratio", "invalid");
        ui.update();
        check(ui.tree() == before && ui.find("text")->node.attr("value") == "A" &&
                  hasErrors(ui.diagnostics()),
              "Partial invalid update committed");
        ui.clearDiagnostics();
        ui.data().set("ratio", .6);
        ui.data().set("text", "C");
        ui.data().set("style", "unknown:5");
        ui.update();
        check(ui.find("text")->node.attr("value") == "A" && hasErrors(ui.diagnostics()),
              "Bad style committed");
        ui.clearDiagnostics();
        ui.data().set("style", "font-size:24dp");
        ui.data().set("text", "D");
        ui.update();
        check(ui.find("text")->node.attr("value") == "D" && !hasErrors(ui.diagnostics()),
              "Valid recovery failed");
        near(float(ui.state("slider")->value), .6f);
    });
    test("Typed action parameters update and action safely replaces screen", [] {
        Runtime ui;
        ui.data().set("count", 1);
        int seen = 0;
        ui.actions().registerAction(
            "test:replace",
            {{{"count", ValueType::number}}, "", [&](const ActionContext &c) -> std::optional<std::string> {
                 seen = int(*c.parameters.at("count").numeric());
                 check(ui.loadXml(screen("<Text id=\"replacement\" value=\"New\"/>")),
                       "Action replacement load");
                 return {};
             }});
        check(ui.loadXml(screen("<Button id=\"b\" action=\"test:replace\" count=\"{count}\"/>")),
              "Action load");
        ui.data().set("count", 7);
        ui.update();
        click(ui, "b");
        check(seen == 7 && ui.find("replacement"), "Stale action value or replacement failed");
    });
    test("External widget unregister releases callbacks after dispatch", [] {
        Runtime ui;
        auto lifetime = std::make_shared<int>(0);
        std::weak_ptr<int> weak = lifetime;
        int calls = 0;
        WidgetDefinition def{[lifetime](const Node &, Vec2) { return Vec2{40, 20}; },
                             [lifetime](const auto &c, auto &l) {
                                 l.commands.emplace_back(
                                     DrawRectangle{c.bounds, {10, 20, 30, 255}, {}, 0, 0});
                             },
                             [&, lifetime](const auto &, const auto &) {
                                 ++calls;
                                 ui.components().unregisterWidget("External");
                                 return true;
                             }};
        check(ui.components().registerWidget("External", std::move(def)), "Register external");
        lifetime.reset();
        check(ui.loadXml(screen("<External id=\"ext\"/>")), "External load");
        ui.update();
        auto draw = ui.buildDrawList();
        Event e;
        e.type = EventType::pointerMove;
        e.position = {3, 3};
        ui.processEvent(e);
        check(calls == 1 && weak.expired() && !ui.components().get("External"),
              "Executable callback retained");
        ui.update();
        near(ui.find("ext")->bounds.w, 0);
        ui.buildDrawList();
        SoftwareRenderer renderer;
        ui.close();
        renderer.render(draw);
        check(!renderer.pixels().empty(), "External owning output lost");
    });
    test("Text cache keeps fractional widths and handles hostile numeric input", [] {
        Resources resources;
        TextSystem text(resources);
        auto a = text.layout("A", "missing:font", 18, 30.1f, "right");
        auto b = text.layout("A", "missing:font", 18, 30.9f, "right");
        check(a != b && b->glyphs.front().x > a->glyphs.front().x, "Fractional alignment cache collision");
        auto c = text.layout("A", "missing:font", std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::quiet_NaN());
        check(std::isfinite(c->size.x), "Nonfinite text dimensions");
        check(utf8Boundaries("A\xef\xbf\xbdБ") == std::vector<std::size_t>{0, 1, 4, 6},
              "Replacement character split");
    });
    test("External bound UTF8 replacement snaps caret to a valid boundary", [] {
        Runtime ui;
        ui.data().set("name", "abc");
        ui.data().set("choice", "A");
        check(
            ui.loadXml(screen("<TextInput id=\"t\" value=\"{name}\"/><Select id=\"s\" value=\"{choice}\"/>")),
            "Load");
        ui.update();
        key(ui, Key::tab);
        key(ui, Key::left);
        ui.data().set("name", "ББ");
        ui.data().set("choice", "B");
        ui.update();
        auto boundaries = utf8Boundaries(ui.state("t")->text);
        check(std::ranges::find(boundaries, ui.state("t")->caret) != boundaries.end(),
              "Caret inside UTF8 sequence");
        check(ui.state("s")->text == "B", "Select binding stale");
    });
    test("Provider exceptions retain cached resource on refresh", [] {
        Resources resources;
        bool broken = false;
        resources.mount([&](auto) -> std::optional<std::vector<std::uint8_t>> {
            if (broken)
                throw std::runtime_error("broken");
            return std::vector<std::uint8_t>{1};
        });
        auto before = resources.get("test:data");
        broken = true;
        Diagnostics d;
        check(!resources.refresh(&d) && hasErrors(d) && resources.get("test:data") == before,
              "Provider exception lost resource");
        check(!resources.get("test:new", &d), "Broken provider returned data");
    });
    test("Acyclic template fanout and SVG contour budgets are bounded", [] {
        std::string body = "<Template name=\"T0\"><Text/></Template>";
        for (int i = 1; i < 16; ++i)
            body += "<Template name=\"T" + std::to_string(i) + "\"><Row><Component template=\"T" +
                    std::to_string(i - 1) + "\"/><Component template=\"T" + std::to_string(i - 1) +
                    "\"/></Row></Template>";
        auto result = parseDocument(screen(body + "<Component template=\"T15\"/>"));
        check(!result &&
                  std::ranges::any_of(result.diagnostics,
                                      [](auto &d) { return d.message.find("budget") != std::string::npos; }),
              "Unbounded acyclic template validation");
        Limits limits;
        limits.geometry = 64;
        Diagnostics d;
        std::string path;
        for (int i = 0; i < 8; ++i)
            path += "M0 0 C100 0 0 100 100 100 Z ";
        check(!parseSvg(resource("<svg viewBox=\"0 0 100 100\"><path d=\"" + path + "\"/></svg>"), d, limits),
              "Per-contour budget bypass accepted");
        d.clear();
        auto tiny = parseSvg(
            resource("<svg viewBox=\"0 0 100 100\"><path d=\"M0 0 A1e-30 1e-30 0 0 1 90 50\"/></svg>"), d);
        check(tiny && !hasErrors(d), "Tiny finite arc failed");
    });
    test("Incremental payload budget rejects oversized generated content", [] {
        Runtime ui;
        ui.data().set("label", "A");
        std::string body;
        for (int i = 0; i < 20; ++i)
            body += "<Text value=\"{label}\"/>";
        check(ui.loadXml(screen(body)), "Budget load");
        ui.update();
        auto *before = ui.tree();
        ui.data().set("label", std::string(1024 * 1024, 'x'));
        ui.update();
        check(ui.tree() == before && ui.tree()->children.front().node.attr("value") == "A" &&
                  hasErrors(ui.diagnostics()),
              "Incremental payload bypass");
        ui.clearDiagnostics();
        ui.data().set("label", "B");
        ui.update();
        check(ui.tree()->children.front().node.attr("value") == "B", "Payload recovery failed");
    });
    test("Empty repeats populate and keyed edits match fresh layout", [] {
        Runtime ui;
        auto xml = screen("<Column><Repeat id=\"r\" items=\"{items}\" key=\"id\"><Button id=\"b\" "
                          "value=\"{item.name}\"/></Repeat>"
                          "<Text id=\"tail\" value=\"Tail\"/></Column>");
        check(ui.loadXml(xml), "Missing repeat load");
        ui.update();
        Value::Array items{Value::Object{{"id", "a"}, {"name", "Alpha"}},
                           Value::Object{{"id", "b"}, {"name", "Beta"}}};
        ui.data().set("items", items);
        ui.update();
        check(ui.find("r/61/b"), "Missing repeat failed to populate");
        click(ui, "r/61/b");
        std::rotate(items.begin(), items.begin() + 1, items.end());
        ui.data().set("items", items);
        ui.update();
        check(ui.state("r/61/b")->selected && ui.focusedId() == "r/61/b", "Keyed reorder lost state");
        items.insert(items.begin(), Value::Object{{"id", "c"}, {"name", "Inserted"}});
        ui.data().set("items", items);
        ui.update();
        items.erase(items.begin() + 1);
        ui.data().set("items", items);
        ui.update();
        Runtime fresh;
        fresh.data().set("items", items);
        check(fresh.loadXml(xml), "Fresh edits load");
        fresh.update();
        sameTree(*ui.tree(), *fresh.tree());
        // Match the valid selected/focused state before comparing pixels.
        click(fresh, "r/61/b");
        samePixels(ui, fresh);
        check(ui.find("tail"), "Unaffected tail lost");
    });
    test("SVG cache observes nested image revisions and owning draw snapshots", [] {
        Runtime ui;
        demo::mount(ui, std::filesystem::path(VOXELUI_SOURCE_DIR) / "examples");
        auto png = ui.resources().get("example:assets/landscape.png");
        check(bool(png), "PNG fixture");
        std::vector<std::uint8_t> bytes = png->bytes;
        ui.resources().mount([&](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
            if (id == "test:image")
                return bytes;
            return {};
        });
        auto svg = resource(
            "<svg viewBox=\"0 0 100 100\"><image href=\"test:image\" width=\"100\" height=\"100\"/></svg>");
        VectorCache cache;
        Diagnostics d;
        auto before = cache.get(svg, d, &ui.resources());
        check(bool(before), "Nested image parse");
        ui.resources().invalidate("test:image");
        auto after = cache.get(svg, d, &ui.resources());
        check(after && after != before && after->shapes.front().image != before->shapes.front().image,
              "Stale nested image cache");
        ui.data().set("source", "example:assets/icon.svg");
        ui.data().set("label", "A");
        check(ui.loadXml(screen("<Vector id=\"v\" source=\"{source}\"/><Text id=\"t\" value=\"{label}\"/>")),
              "Resource binding load");
        auto draw = ui.buildDrawList();
        SoftwareRenderer renderer;
        renderer.render(draw);
        auto pixels = renderer.pixels();
        ui.data().set("source", "example:assets/decor.svg");
        ui.data().set("label", "B");
        ui.update();
        check(ui.find("t")->node.attr("value") == "B", "Resource update missed data");
        ui.close();
        renderer.render(draw);
        check(renderer.pixels() == pixels, "Old draw payload changed");
    });
    test("Reference renderer rejects nonfinite framebuffer scale", [] {
        DrawList list;
        list.logicalSize = {20, 20};
        list.framebufferScale = std::numeric_limits<float>::quiet_NaN();
        SoftwareRenderer renderer;
        bool rejected = false;
        try {
            renderer.render(list);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        check(rejected, "NaN framebuffer scale accepted");
    });
    test("Host widget intrinsic changes invalidate through public IDs", [] {
        Runtime ui;
        float width = 20;
        ui.data().set("minimum", 10);
        ui.components().registerWidget(
            "External", {[&](const Node &n, Vec2) {
                             return Vec2{std::max(width, float(number(n.attr("min")).value_or(0))), 20};
                         },
                         [](const auto &, auto &) {},
                         {}});
        check(ui.loadXml(screen("<External id=\"ext\" min=\"{minimum}\"/>")), "Host size load");
        ui.update();
        width = 80;
        check(ui.invalidateMeasurement("ext") && !ui.invalidateMeasurement("missing"),
              "Host invalidation ID");
        ui.update();
        near(ui.find("ext")->bounds.w, 80);
        ui.data().set("minimum", 120);
        ui.update();
        near(ui.find("ext")->bounds.w, 120);
    });
    test("Rejected model changes preserve pending focus styling", [] {
        Runtime ui;
        ui.data().set("style", "font-size:18dp");
        ui.data().set("text", "A");
        ui.setStyleOverride("TextInput:focus {color:#ff0000}");
        check(ui.loadXml(screen("<TextInput id=\"t\" value=\"{text}\" style=\"{style}\"/>")),
              "Focus rollback load");
        ui.update();
        Event e;
        e.type = EventType::keyDown;
        e.key = Key::tab;
        ui.processEvent(e);
        ui.data().set("style", "invalid:3");
        ui.update();
        check(ui.focusedId() == "t" && ui.find("t")->style.foreground == Color{255, 0, 0, 255},
              "Rejected data lost focus styling");
    });
    std::cout << passed << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
