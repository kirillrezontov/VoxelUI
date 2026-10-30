#include "../common/host.hpp"
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: voxelui-validate document.xml [--mock-schema] [--root directory]\n";
        return 2;
    }
    try {
        std::filesystem::path file = argv[1], root = demo::rootFor(file);
        bool schema = false;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--mock-schema")
                schema = true;
            else if (a == "--root" && i + 1 < argc)
                root = argv[++i];
            else
                return 2;
        }
        std::ifstream f(file, std::ios::binary);
        if (!f) {
            std::cerr << "Cannot open " << file << '\n';
            return 1;
        }
        std::string xml((std::istreambuf_iterator<char>(f)), {});
        auto syntax = voxelui::parseDocument(xml, file.string());
        demo::diagnostics(syntax.diagnostics);
        if (!syntax)
            return 1;
        voxelui::Runtime ui;
        demo::mount(ui, root);
        if (schema) {
            demo::actions(ui);
            demo::mockData(ui);
        } else { // Syntax and resource/style validation can run without a host action schema.
            std::function<void(const voxelui::Node &)> allow = [&](const auto &n) {
                auto action = n.attr("action");
                if (!action.empty() && !ui.actions().contains(action))
                    ui.actions().registerAction(
                        action, {{}, "", [](const auto &) -> std::optional<std::string> { return {}; }});
                for (auto &c : n.children)
                    allow(c);
            };
            allow(syntax.document->root);
        }
        bool ok = ui.load(demo::screenId(file, root));
        demo::diagnostics(ui.diagnostics());
        if (!ok || std::ranges::any_of(ui.diagnostics(), [](auto &d) {
                return d.message.starts_with("Missing image resource:");
            }))
            return 1;
        if (schema && std::ranges::any_of(ui.diagnostics(), [](auto &d) {
                return d.message.starts_with("Missing binding") || d.message.starts_with("Repeat items");
            }))
            return 1;
        std::cout << "XML, component, style and resource validation passed. "
                  << (schema ? "Mock binding/action schema checked."
                             : "Binding/action schema NOT checked (use --mock-schema or the Runtime API with "
                               "host registrations).")
                  << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
