#include "../apps/common/host.hpp"
#include <voxelui/concepts.hpp>
#include <voxelui/software.hpp>
struct Badge {
    voxelui::Vec2 measure(const voxelui::Node &, voxelui::Vec2) {
        return {140, 44};
    }
    void draw(const voxelui::WidgetContext &c, voxelui::DrawList &list) {
        list.commands.emplace_back(voxelui::DrawRectangle{c.bounds, {50, 170, 130, 255}, {}, 0, 10});
    }
};
int main(int argc, char **argv) {
    voxelui::Runtime ui;
    demo::mount(ui, argc > 1 ? argv[1] : "examples");
    ui.components().registerWidget("HostBadge", std::make_shared<Badge>());
    ui.actions().registerAction(
        "host:accept",
        {{{"message", voxelui::ValueType::string}}, "", [](const auto &ctx) -> std::optional<std::string> {
             std::cout << ctx.parameters.at("message").string() << '\n';
             return {};
         }});
    ui.data().set("host.message", "Hello / Привет from the host");
    if (!ui.loadXml(
            R"(<Screen version="1"><Column style="padding: 24dp"><Text value="{host.message}"/><HostBadge/><Button id="accept" action="host:accept" message="{host.message}">Accept</Button></Column></Screen>)")) {
        demo::diagnostics(ui.diagnostics());
        return 1;
    }
    ui.update();
    voxelui::Event event;
    event.type = voxelui::EventType::keyDown;
    event.key = voxelui::Key::tab;
    ui.processEvent(event);
    event.key = voxelui::Key::enter;
    ui.processEvent(event);
    voxelui::SoftwareRenderer renderer;
    voxelui::render(renderer, ui.buildDrawList());
    return renderer.savePng("mock-host.png") ? 0 : 1;
}
