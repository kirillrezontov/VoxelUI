#include <charconv>
#include <functional>
#include <sstream>
#include <voxelui/style.hpp>
namespace voxelui {
inline namespace v1 {
std::optional<Color> parseColor(std::string_view s) {
    if (s == "transparent")
        return Color{0, 0, 0, 0};
    if (s == "white")
        return Color{255, 255, 255, 255};
    if (s == "black")
        return Color{0, 0, 0, 255};
    if (s == "red")
        return Color{255, 0, 0, 255};
    if (s.size() != 4 && s.size() != 7 && s.size() != 9)
        return {};
    if (s[0] != '#')
        return {};
    unsigned v{};
    auto [p, e] = std::from_chars(s.data() + 1, s.data() + s.size(), v, 16);
    if (e != std::errc{} || p != s.data() + s.size())
        return {};
    if (s.size() == 4)
        return Color{std::uint8_t(((v >> 8) & 15) * 17), std::uint8_t(((v >> 4) & 15) * 17),
                     std::uint8_t((v & 15) * 17), 255};
    if (s.size() == 7)
        return Color{std::uint8_t(v >> 16), std::uint8_t(v >> 8), std::uint8_t(v), 255};
    return Color{std::uint8_t(v >> 24), std::uint8_t(v >> 16), std::uint8_t(v >> 8), std::uint8_t(v)};
}
std::optional<Length> parseLength(std::string_view s) {
    if (s == "auto")
        return Length{};
    Unit unit = Unit::dp;
    if (s.ends_with("dp"))
        s.remove_suffix(2);
    else if (s.ends_with('%')) {
        unit = Unit::percent;
        s.remove_suffix(1);
    }
    auto v = number(s);
    if (!v || *v < 0 || *v > 100000)
        return {};
    return Length{float(*v), unit};
}
bool knownProperty(std::string_view p) {
    static std::set<std::string> props = {
        "background",  "background-color", "color",       "border-color", "border-width",   "border-radius",
        "opacity",     "padding",          "margin",      "gap",          "width",          "height",
        "min-width",   "min-height",       "max-width",   "max-height",   "align-items",    "justify-content",
        "font-family", "font-size",        "font-weight", "text-align",   "vertical-align", "image-fit",
        "overflow",    "visibility",       "columns",     "anchor",       "transition"};
    return p.starts_with("--") || props.contains(std::string(p));
}
namespace {
std::vector<Declaration> declarations(std::string_view s, std::string source, int line, Diagnostics &d) {
    std::vector<Declaration> out;
    while (!s.empty()) {
        auto end = s.find(';');
        auto item = trim(s.substr(0, end));
        if (!item.empty()) {
            auto colon = item.find(':');
            if (colon == item.npos)
                d.push_back({Severity::error, source, line, 1, "Expected property: value"});
            else {
                auto p = trim(std::string_view(item).substr(0, colon)),
                     v = trim(std::string_view(item).substr(colon + 1));
                if (!knownProperty(p))
                    d.push_back({Severity::error, source, line, 1, "Unknown style property: " + p});
                else if (v.empty())
                    d.push_back({Severity::error, source, line, 1, "Empty value for " + p});
                else
                    out.push_back({p, v});
            }
        }
        if (end == s.npos)
            break;
        line += int(std::count(s.begin(), s.begin() + std::ptrdiff_t(end), '\n'));
        s.remove_prefix(end + 1);
    }
    return out;
}
bool apply(Style &s, const Declaration &d) {
    auto &p = d.property;
    auto &v = d.value;
    if (p.starts_with("--")) {
        s.variables[p] = v;
        return true;
    }
    if (p == "background" || p == "background-color" || p == "color" || p == "border-color") {
        auto c = parseColor(v);
        if (!c)
            return false;
        if (p == "color")
            s.foreground = *c;
        else if (p == "border-color")
            s.border = *c;
        else
            s.background = *c;
        return true;
    }
    Length *l = nullptr;
    if (p == "width")
        l = &s.width;
    if (p == "height")
        l = &s.height;
    if (p == "min-width")
        l = &s.minWidth;
    if (p == "min-height")
        l = &s.minHeight;
    if (p == "max-width")
        l = &s.maxWidth;
    if (p == "max-height")
        l = &s.maxHeight;
    if (l) {
        auto parsed = parseLength(v);
        if (!parsed)
            return false;
        *l = *parsed;
        return true;
    }
    float *f = nullptr;
    if (p == "padding")
        f = &s.padding;
    if (p == "margin")
        f = &s.margin;
    if (p == "gap")
        f = &s.gap;
    if (p == "font-size")
        f = &s.fontSize;
    if (p == "border-width")
        f = &s.borderWidth;
    if (p == "border-radius")
        f = &s.radius;
    if (p == "transition") {
        std::string_view value = v;
        if (value.ends_with('s'))
            value.remove_suffix(1);
        auto parsed = number(value);
        if (!parsed || *parsed < 0 || *parsed > 60)
            return false;
        s.transition = float(*parsed);
        return true;
    }
    if (f) {
        auto parsed = parseLength(v);
        if (!parsed || parsed->unit != Unit::dp)
            return false;
        *f = parsed->value;
        return true;
    }
    if (p == "opacity") {
        auto n = number(v);
        if (!n || *n < 0 || *n > 1)
            return false;
        s.opacity = float(*n);
        return true;
    }
    if (p == "columns" || p == "font-weight") {
        auto n = number(v);
        if (!n || *n < 1 || *n > 1000 || std::floor(*n) != *n)
            return false;
        if (p == "columns")
            s.columns = int(*n);
        else
            s.fontWeight = int(*n);
        return true;
    }
    if (p == "align-items" || p == "justify-content") {
        if (v != "start" && v != "center" && v != "end" && v != "stretch")
            return false;
        (p == "align-items" ? s.align : s.justify) = v;
        return true;
    }
    if (p == "text-align") {
        if (v != "left" && v != "center" && v != "right")
            return false;
        s.textAlign = v;
        return true;
    }
    if (p == "vertical-align") {
        if (v != "top" && v != "center" && v != "bottom")
            return false;
        s.verticalAlign = v;
        return true;
    }
    if (p == "image-fit") {
        if (v != "contain" && v != "cover" && v != "stretch")
            return false;
        s.fit = v;
        return true;
    }
    if (p == "overflow") {
        if (v != "visible" && v != "hidden" && v != "scroll")
            return false;
        s.clip = v != "visible";
        return true;
    }
    if (p == "visibility") {
        if (v != "visible" && v != "hidden")
            return false;
        s.visible = v == "visible";
        return true;
    }
    if (p == "font-family") {
        if (!validResourceId(v))
            return false;
        s.font = v;
        return true;
    }
    if (p == "anchor") {
        if (v != "top-left" && v != "top-right" && v != "bottom-left" && v != "bottom-right" && v != "center")
            return false;
        s.anchor = v;
        return true;
    }
    return false;
}
std::optional<Selector> selector(std::string_view q) {
    Selector s;
    std::size_t p = 0;
    while (p < q.size()) {
        char kind = q[p];
        if (kind == '.' || kind == '#' || kind == ':')
            ++p;
        else
            kind = 't';
        auto start = p;
        while (p < q.size() &&
               (std::isalnum(static_cast<unsigned char>(q[p])) || q[p] == '-' || q[p] == '_' || q[p] == '*'))
            ++p;
        if (start == p)
            return {};
        auto token = std::string(q.substr(start, p - start));
        if (kind == 't') {
            if (!s.tag.empty())
                return {};
            s.tag = token;
            if (token != "*")
                ++s.specificity;
        }
        if (kind == '#') {
            s.id = token;
            s.specificity += 100;
        }
        if (kind == '.') {
            s.classes.push_back(token);
            s.specificity += 10;
        }
        if (kind == ':') {
            static std::set<std::string> states = {"hover", "focus", "active", "disabled", "selected"};
            if (!states.contains(token))
                return {};
            s.states.push_back(token);
            s.specificity += 10;
        }
    }
    return s;
}
bool matches(const Selector &s, const Node &n, const std::set<std::string> &states) {
    if (!s.tag.empty() && s.tag != "*" && s.tag != n.tag)
        return false;
    if (!s.id.empty() && s.id != n.id)
        return false;
    std::istringstream in(n.attr("class"));
    std::set<std::string> classes;
    std::string c;
    while (in >> c)
        classes.insert(c);
    for (auto &k : s.classes)
        if (!classes.contains(k))
            return false;
    for (auto &k : s.states)
        if (!states.contains(k))
            return false;
    return true;
}
} // namespace
Stylesheet parseStylesheet(std::string_view css, std::string source, Diagnostics &d, Resources *resources,
                           Limits limits) {
    Stylesheet result;
    std::set<std::string> chain;
    std::size_t total = 0;
    std::function<void(std::string_view, std::string, std::size_t)> parse = [&](std::string_view input,
                                                                                std::string file,
                                                                                std::size_t depth) {
        if (depth > limits.depth || input.size() > limits.bytes || total + input.size() > limits.bytes) {
            d.push_back({Severity::error, file, 1, 1, "Stylesheet import/size limit exceeded"});
            return;
        }
        total += input.size();
        if (!chain.insert(file).second) {
            d.push_back({Severity::error, file, 1, 1, "Circular stylesheet import"});
            return;
        }
        std::string clean(input);
        for (auto pos = clean.find("/*"); pos != clean.npos; pos = clean.find("/*", pos)) {
            auto end = clean.find("*/", pos + 2);
            if (end == clean.npos) {
                d.push_back({Severity::error, file, 1, 1, "Unterminated CSS comment"});
                break;
            }
            for (auto i = pos; i < end + 2; ++i)
                if (clean[i] != '\n')
                    clean[i] = ' ';
        }
        std::string_view text = clean;
        int line = 1;
        while (!text.empty()) {
            auto space = text.find_first_not_of(" \r\n\t");
            if (space == text.npos)
                break;
            line += int(std::count(text.begin(), text.begin() + std::ptrdiff_t(space), '\n'));
            text.remove_prefix(space);
            if (text.starts_with("@import")) {
                auto end = text.find(';');
                if (end == text.npos) {
                    d.push_back({Severity::error, file, line, 1, "Unterminated import"});
                    break;
                }
                auto id = trim(text.substr(7, end - 7));
                if (id.size() >= 2 && (id.front() == '\"' || id.front() == '\''))
                    id = id.substr(1, id.size() - 2);
                if (!resources || !validResourceId(id))
                    d.push_back(
                        {Severity::error, file, line, 1, "Import requires a namespaced resource resolver"});
                else if (auto r = resources->get(id, &d))
                    parse(std::string_view(reinterpret_cast<const char *>(r->bytes.data()), r->bytes.size()),
                          id, depth + 1);
                text.remove_prefix(end + 1);
                continue;
            }
            auto brace = text.find('{'), end = text.find('}');
            if (brace == text.npos || end == text.npos || end < brace) {
                d.push_back({Severity::error, file, line, 1, "Expected selector { declarations }"});
                break;
            }
            auto ds = declarations(text.substr(brace + 1, end - brace - 1), file, line, d);
            for (auto &dec : ds) {
                Style dummy;
                if (!dec.value.starts_with("var(") && !apply(dummy, dec))
                    d.push_back({Severity::error, file, line, 1,
                                 "Invalid value for " + dec.property + ": " + dec.value});
            }
            auto names = text.substr(0, brace);
            while (!names.empty()) {
                auto comma = names.find(',');
                auto s = selector(trim(names.substr(0, comma)));
                if (!s)
                    d.push_back({Severity::error, file, line, 1, "Unsupported or invalid selector"});
                else
                    result.rules.push_back({*s, ds, result.rules.size()});
                if (result.rules.size() > limits.elements) {
                    d.push_back({Severity::error, file, line, 1, "Too many style rules"});
                    break;
                }
                if (comma == names.npos)
                    break;
                names.remove_prefix(comma + 1);
            }
            line += int(std::count(text.begin(), text.begin() + std::ptrdiff_t(end + 1), '\n'));
            text.remove_prefix(end + 1);
        }
        chain.erase(file);
    };
    parse(css, std::move(source), 0);
    return result;
}
Style resolveStyle(const Node &n, const Style *parent, const Stylesheet &sheet,
                   const std::set<std::string> &states, Diagnostics *diagnostics) {
    Style style;
    if (parent) {
        style.foreground = parent->foreground;
        style.font = parent->font;
        style.fontSize = parent->fontSize;
        style.fontWeight = parent->fontWeight;
        style.textAlign = parent->textAlign;
        style.variables = parent->variables;
        style.opacity = parent->opacity;
    }
    if (n.tag == "Row" || n.tag == "Column")
        style.gap = 8;
    if (n.tag == "Button" || n.tag == "Select" || n.tag == "TextInput") {
        style.background = {47, 58, 78, 255};
        style.padding = 10;
        style.radius = 5;
    }
    if (n.tag == "ScrollView")
        style.clip = true;
    std::vector<const Rule *> rules;
    for (auto &r : sheet.rules)
        if (matches(r.selector, n, states))
            rules.push_back(&r);
    std::stable_sort(rules.begin(), rules.end(), [](auto *a, auto *b) {
        return a->selector.specificity < b->selector.specificity ||
               (a->selector.specificity == b->selector.specificity && a->order < b->order);
    });
    std::vector<Declaration> ds;
    for (auto *r : rules)
        ds.insert(ds.end(), r->declarations.begin(), r->declarations.end());
    Diagnostics local;
    auto in = declarations(n.attr("style"), n.source, n.line, local);
    ds.insert(ds.end(), in.begin(), in.end());
    for (auto &dec : ds)
        if (dec.property.starts_with("--"))
            apply(style, dec);
    for (auto dec : ds) {
        if (dec.property.starts_with("--"))
            continue;
        std::set<std::string> visited;
        bool bad = false;
        while (dec.value.starts_with("var(") && dec.value.ends_with(')')) {
            auto name = trim(std::string_view(dec.value).substr(4, dec.value.size() - 5));
            auto it = style.variables.find(name);
            if (!visited.insert(name).second || it == style.variables.end()) {
                bad = true;
                break;
            }
            dec.value = it->second;
        }
        if (bad || !apply(style, dec))
            local.push_back(
                {Severity::error, n.source, n.line, 1, "Invalid or unresolved style: " + dec.property});
    }
    if (diagnostics)
        diagnostics->insert(diagnostics->end(), local.begin(), local.end());
    return style;
}
} // namespace v1
} // namespace voxelui
