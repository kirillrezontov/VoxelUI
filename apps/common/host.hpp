#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <voxelui/runtime.hpp>
namespace demo {
inline void diagnostics(const voxelui::Diagnostics &ds) {
    for (auto &d : ds)
        std::cerr << d.source << ":" << d.line << ":" << d.column << ": "
                  << (d.severity == voxelui::Severity::error ? "error: " : "warning: ") << d.message << '\n';
}
inline std::filesystem::path rootFor(std::filesystem::path file) {
    auto dir = std::filesystem::absolute(file).parent_path();
    for (auto d = dir; !d.empty(); d = d.parent_path()) {
        if (std::filesystem::exists(d / "assets/font.ttf"))
            return d;
        if (d == d.root_path())
            break;
    }
    return dir;
}
inline void mount(voxelui::Runtime &ui, const std::filesystem::path &root) {
    ui.resources().mountDirectory("example", root);
    ui.resources().mountDirectory("ui", root / "assets");
    ui.resources().mountDirectory("input", root);
    ui.resources().mount([root](std::string_view id) -> std::optional<std::vector<std::uint8_t>> {
        if (id != "ui:font")
            return {};
        std::ifstream f(root / "assets/font.ttf", std::ios::binary);
        if (!f)
            return {};
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
    });
}
inline void mockData(voxelui::Runtime &ui) {
    using namespace voxelui;
    ui.data().set("menu.title", "VOXEL UI");
    ui.data().set("menu.status", "Standalone engine · Автономный интерфейс");
    ui.data().set("settings.name", "Алексей");
    ui.data().set("settings.volume", .65);
    ui.data().set("settings.music", true);
    ui.data().set("settings.quality", "High");
    ui.data().set("player.health", .78);
    ui.data().set("inventory.status", "Select, hover for tooltips, drag to another item");
    ui.data().set("gallery.status",
                  "Click the green host-drawn SVG slot. Unsupported filters warn in console.");
    Value::Array worlds, items;
    for (int i = 0; i < 24; ++i) {
        worlds.emplace_back(Value::Object{
            {"id", "world-" + std::to_string(i)},
            {"name", i % 2 ? "Берёзовый лес " + std::to_string(i) : "Mountain valley " + std::to_string(i)},
            {"subtitle", "Local mock world • seed " + std::to_string(i * 173)},
            {"icon", "example:assets/landscape.png"}});
        items.emplace_back(Value::Object{{"id", "item-" + std::to_string(i)},
                                         {"name", "Item " + std::to_string(i + 1)},
                                         {"quantity", i + 1},
                                         {"icon", "example:assets/icon.svg"}});
    }
    ui.data().set("worlds.available", std::move(worlds));
    ui.data().set("inventory.items", std::move(items));
}
inline void actions(voxelui::Runtime &ui) {
    ui.registerSlot(
        "gallery.badge",
        {{},
         [](const auto &c, auto &list) {
             list.commands.emplace_back(
                 voxelui::DrawRectangle{c.bounds, {60, 190, 135, 255}, {220, 255, 240, 255}, 1, 6});
         },
         [&ui](const auto &, const voxelui::Event &e) {
             if (e.type == voxelui::EventType::pointerUp)
                 ui.data().set("gallery.status", "Host slot activated using local SVG coordinates");
             return true;
         }});
    ui.observeEvents([&ui](voxelui::RoutedEvent &e) {
        if (e.current != e.target)
            return;
        if (e.event.type == voxelui::EventType::dragStart)
            ui.data().set("inventory.status", "Dragging " + e.target);
        if (e.event.type == voxelui::EventType::dragEnd)
            ui.data().set("inventory.status", "Dropped " + e.event.text + " onto " + e.target);
    });
    for (auto id : {"example:play", "example:settings", "example:quit", "example:apply", "example:open_world",
                    "example:item"})
        ui.actions().registerAction(
            id, {{}, "", [&ui](const voxelui::ActionContext &c) -> std::optional<std::string> {
                     std::cout << "Action " << c.action << " from " << c.elementId << '\n';
                     ui.data().set("menu.status", "Activated " + c.action);
                     return {};
                 }});
}
inline std::string screenId(const std::filesystem::path &file, const std::filesystem::path &root) {
    return "input:" +
           std::filesystem::relative(std::filesystem::absolute(file), std::filesystem::absolute(root))
               .generic_string();
}
} // namespace demo
