#include <voxelui/runtime.hpp>
#include <voxelui/software.hpp>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("Public host contract failed"); }
struct Gauge {
    float width{24};
    voxelui::Vec2 measure(const voxelui::Node &, voxelui::Vec2) { return {width, 12}; }
    void draw(const voxelui::WidgetContext &context, voxelui::DrawList &list) {
        list.commands.emplace_back(voxelui::DrawRectangle{context.bounds, {50,170,80,255}, {}, 0, 0});
    }
};
static_assert(!std::is_move_constructible_v<voxelui::Runtime>);
}
int main() {
    voxelui::DrawList snapshot;
    {
        voxelui::Runtime ui;
        auto widget = std::make_shared<Gauge>();
        std::weak_ptr<Gauge> weak = widget;
        require(ui.components().registerWidget("Gauge", widget));
        int actions = 0;
        require(ui.actions().registerAction("host:close", {{}, {}, [&](const auto &) -> std::optional<std::string> {
            ++actions;ui.close();return {};
        }}));
        ui.data().set("health", 75);
        require(ui.loadXml(R"(<Screen version="1"><Gauge id="gauge"/><ProgressBar id="health" value="{health}"/><Button id="close" action="host:close"/></Screen>)"));
        ui.update();
        widget->width = 64;
        require(ui.invalidateMeasurement("gauge"));ui.update();require(ui.find("gauge")->bounds.w == 64);
        const auto *unrelated = ui.find("gauge");
        ui.data().set("health", 50);ui.update();require(ui.find("gauge") == unrelated);
        snapshot = ui.buildDrawList();
        voxelui::Event event;event.type = voxelui::EventType::keyDown;event.key = voxelui::Key::tab;
        ui.processEvent(event);event.key = voxelui::Key::enter;ui.processEvent(event);
        require(actions == 1 && ui.tree() == nullptr);
        ui.actions().unregisterAction("host:close");
        widget.reset();require(!weak.expired());
        require(ui.components().unregisterWidget("Gauge"));require(weak.expired());
    }
    voxelui::SoftwareRenderer software;software.render(snapshot, {0,0,0,0});
    require(software.width() > 0 && software.pixels()[3] == 255);
}
