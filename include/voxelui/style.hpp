#pragma once
#include "document.hpp"
namespace voxelui {
inline namespace v1 {
enum class Unit { automatic, dp, percent };
struct Length {
    float value{};
    Unit unit{Unit::automatic};
    float resolve(float basis, float intrinsic) const {
        return unit == Unit::automatic ? intrinsic : unit == Unit::percent ? basis * value * .01f : value;
    }
};
struct Style {
    Color background{0, 0, 0, 0}, foreground{235, 237, 242, 255}, border{0, 0, 0, 0};
    float borderWidth{}, radius{}, opacity{1}, padding{}, margin{}, gap{}, fontSize{18};
    Length width, height, minWidth{0, Unit::dp}, minHeight{0, Unit::dp}, maxWidth{100000, Unit::dp},
        maxHeight{100000, Unit::dp};
    std::string align = "start", justify = "start", textAlign = "left", verticalAlign = "center",
                font = "ui:font", fit = "contain", anchor;
    int columns{1}, fontWeight{400};
    bool visible{true}, clip{};
    float transition{};
    std::map<std::string, std::string> variables;
};
struct Selector {
    std::string tag, id;
    std::vector<std::string> classes, states;
    int specificity{};
};
struct Declaration {
    std::string property, value;
};
struct Rule {
    Selector selector;
    std::vector<Declaration> declarations;
    std::size_t order{};
};
struct Stylesheet {
    std::vector<Rule> rules;
};
Stylesheet parseStylesheet(std::string_view css, std::string source, Diagnostics &,
                           Resources *resources = nullptr, Limits limits = {});
Style resolveStyle(const Node &, const Style *parent, const Stylesheet &, const std::set<std::string> &states,
                   Diagnostics *diagnostics = nullptr);
std::optional<Color> parseColor(std::string_view);
std::optional<Length> parseLength(std::string_view);
bool knownProperty(std::string_view);
} // namespace v1
} // namespace voxelui
