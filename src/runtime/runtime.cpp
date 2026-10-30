#include "internal.hpp"
#include <functional>
#include <sstream>
namespace voxelui {
inline namespace v1 {
using namespace runtime_detail;
const LayoutNode *Runtime::find(std::string_view id) const {
    return tree_ ? search(*tree_, id) : nullptr;
}
bool Runtime::load(std::string_view id) {
    auto r = resources_.get(id, &diagnostics_);
    if (!r)
        return false;
    if (!loadXml(bytes(*r), std::string(id)))
        return false;
    screenId_ = id;
    return true;
}
bool Runtime::loadXml(std::string_view xml, std::string source) {
    auto parsed = parseDocument(xml, source, components_.tags());
    diagnostics_ = std::move(parsed.diagnostics);
    if (!parsed)
        return false;
    std::function<void(const Node &)> validateInline = [&](const Node &n) {
        auto style = n.attr("style");
        if (!style.empty())
            parseStylesheet("* {" + style + "}", n.source, diagnostics_, &resources_);
        for (auto &c : n.children)
            validateInline(c);
    };
    validateInline(parsed.document->root);
    Stylesheet candidate;
    auto css = parsed.document->root.attr("stylesheet");
    if (!css.empty()) {
        auto r = resources_.get(css, &diagnostics_);
        if (r)
            candidate = parseStylesheet(bytes(*r), css, diagnostics_, &resources_);
    }
    if (!overrideCss_.empty()) {
        auto over = parseStylesheet(overrideCss_, "<theme-override>", diagnostics_, &resources_);
        candidate.rules.insert(candidate.rules.end(), over.rules.begin(), over.rules.end());
    }
    for (std::size_t i = 0; i < candidate.rules.size(); ++i)
        candidate.rules[i].order = i;
    if (hasErrors(diagnostics_))
        return false;
    auto oldDoc = std::move(document_);
    auto oldTree = std::move(tree_);
    auto oldSheet = std::move(stylesheet_);
    auto oldStates = states_;
    auto oldTypes = stateTypes_;
    auto oldBound = boundValues_;
    auto oldFocus = focus_, oldCapture = capture_;
    auto oldImages = images_;
    auto oldVectors = vectorDocuments_;
    auto oldImageRevisions = imageRevisions_;
    auto oldDependencies = dependencies_;
    auto oldAnimations = animations_;
    auto oldStyles = previousStyles_;
    document_ = std::move(parsed.document);
    stylesheet_ = std::move(candidate);
    dirty_ = true;
    rebuild();
    if (hasErrors(diagnostics_)) {
        document_ = std::move(oldDoc);
        tree_ = std::move(oldTree);
        stylesheet_ = std::move(oldSheet);
        states_ = std::move(oldStates);
        stateTypes_ = std::move(oldTypes);
        boundValues_ = std::move(oldBound);
        images_ = std::move(oldImages);
        vectorDocuments_ = std::move(oldVectors);
        imageRevisions_ = std::move(oldImageRevisions);
        dependencies_ = std::move(oldDependencies);
        animations_ = std::move(oldAnimations);
        previousStyles_ = std::move(oldStyles);
        focus_ = oldFocus;
        capture_ = oldCapture;
        dirty_ = false;
        return false;
    }
    screenId_.clear();
    resourceEpoch_ = resources_.epoch();
    dataRevision_ = data_.revision();
    return true;
}
bool Runtime::reload() {
    if (screenId_.empty())
        return false;
    auto id = screenId_;
    return load(id);
}
void Runtime::close() {
    document_.reset();
    tree_.reset();
    states_.clear();
    stateTypes_.clear();
    boundValues_.clear();
    focus_.clear();
    hover_.clear();
    capture_.clear();
    pressed_.clear();
    drag_.clear();
    screenId_.clear();
    images_.clear();
    vectorDocuments_.clear();
    imageRevisions_.clear();
    animations_.clear();
    previousStyles_.clear();
    slotCapture_.clear();
}
bool Runtime::setStyleOverride(std::string css) {
    Diagnostics d;
    auto result = parseStylesheet(css, "<theme-override>", d, &resources_);
    (void)result;
    if (hasErrors(d)) {
        diagnostics_ = std::move(d);
        return false;
    }
    auto old = overrideCss_;
    overrideCss_ = std::move(css);
    if (!document_)
        return true;
    if (!screenId_.empty()) {
        if (reload())
            return true;
        overrideCss_ = old;
        return false;
    }
    overrideCss_ = old;
    diagnostics_.push_back({Severity::error, "<theme-override>", 1, 1,
                            "For in-memory documents, set the override before loadXml"});
    return false;
}
void Runtime::update(FrameInfo frame) {
    if (!std::isfinite(frame.uiScale))
        frame.uiScale = 1;
    if (!std::isfinite(frame.dpiScale))
        frame.dpiScale = 1;
    if (!std::isfinite(frame.viewport.x))
        frame.viewport.x = 960;
    if (!std::isfinite(frame.viewport.y))
        frame.viewport.y = 640;
    if (!std::isfinite(frame.deltaSeconds))
        frame.deltaSeconds = 0;
    frame.uiScale = std::clamp(frame.uiScale, .25f, 4.f);
    frame.dpiScale = std::clamp(frame.dpiScale, .25f, 8.f);
    frame.viewport.x = std::clamp(frame.viewport.x, 1.f, 8192.f);
    frame.viewport.y = std::clamp(frame.viewport.y, 1.f, 8192.f);
    if (frame.viewport.x != frame_.viewport.x || frame.viewport.y != frame_.viewport.y ||
        frame.uiScale != frame_.uiScale)
        dirty_ = true;
    frame_ = frame;
    if (resources_.epoch() != resourceEpoch_) {
        resourceEpoch_ = resources_.epoch();
        vectors_.clear();
        text_.clear();
        if (!screenId_.empty()) {
            reload();
            return;
        }
        dirty_ = true;
    }
    if (dataRevision_ != data_.revision()) {
        for (auto &change : data_.changes())
            for (auto &dependency : dependencies_)
                if (change == dependency || change.starts_with(dependency + ".") ||
                    dependency.starts_with(change + "."))
                    dirty_ = true;
        dataRevision_ = data_.revision();
    }
    if (dirty_) {
        auto oldTree = tree_;
        auto oldStates = states_;
        auto oldTypes = stateTypes_;
        auto oldBound = boundValues_;
        auto oldFocus = focus_, oldCapture = capture_;
        auto oldDeps = dependencies_;
        auto oldStyles = previousStyles_;
        auto oldAnimations = animations_;
        auto diagnosticStart = diagnostics_.size();
        rebuild();
        bool failed = std::any_of(diagnostics_.begin() + std::ptrdiff_t(diagnosticStart), diagnostics_.end(),
                                  [](auto &d) { return d.severity == Severity::error; });
        if (failed) {
            tree_ = std::move(oldTree);
            states_ = std::move(oldStates);
            stateTypes_ = std::move(oldTypes);
            boundValues_ = std::move(oldBound);
            focus_ = oldFocus;
            capture_ = oldCapture;
            dependencies_ = std::move(oldDeps);
            previousStyles_ = std::move(oldStyles);
            animations_ = std::move(oldAnimations);
        }
    }
    data_.acknowledge();
    animate(frame.deltaSeconds);
}
void Runtime::animate(float delta) {
    if (!tree_ || animations_.empty())
        return;
    auto mix = [](Color a, Color b, float t) {
        auto channel = [&](int x, int y) { return std::uint8_t(x + (y - x) * t); };
        return Color{channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
    };
    std::function<void(LayoutNode &)> visit = [&](LayoutNode &n) {
        auto it = animations_.find(n.node.id);
        if (it != animations_.end()) {
            auto &a = it->second;
            a.elapsed += std::clamp(delta, 0.f, .25f);
            float t = std::clamp(a.elapsed / std::max(.001f, a.duration), 0.f, 1.f);
            n.style.background = mix(a.from.background, a.to.background, t);
            n.style.foreground = mix(a.from.foreground, a.to.foreground, t);
            n.style.border = mix(a.from.border, a.to.border, t);
            n.style.opacity = a.from.opacity + (a.to.opacity - a.from.opacity) * t;
            if (t >= 1)
                animations_.erase(it);
        }
        for (auto &c : n.children)
            visit(c);
    };
    visit(*tree_);
}
void Runtime::write(const LayoutNode &n, std::string_view attribute, Value value) {
    if (n.node.attr("writable") == "true") {
        auto path = n.node.attr("data-binding-" + std::string(attribute));
        if (!path.empty() && !path.starts_with("item.") && !path.starts_with("props."))
            data_.set(path, std::move(value));
    }
    dirty_ = true;
}
void Runtime::activate(const LayoutNode &ref) {
    auto n = ref;
    auto &state = states_[n.node.id];
    if (n.node.tag == "Checkbox" || n.node.tag == "Toggle") {
        state.checked = !state.checked;
        write(n, "value", Value(state.checked));
    }
    if (n.node.tag == "Select") {
        std::vector<std::string> options;
        std::istringstream input(n.node.attr("options"));
        std::string option;
        while (std::getline(input, option, '|'))
            options.push_back(option);
        if (!options.empty()) {
            auto it = std::ranges::find(options, state.text);
            state.text =
                options[(it == options.end() ? 0 : (std::size_t(it - options.begin()) + 1) % options.size())];
            write(n, "value", state.text);
        }
    }
    state.selected = true;
    dirty_ = true;
    ActionContext context{n.node.id, n.node.attr("action"), {}};
    if (context.action.empty())
        return;
    auto b = boundValues_.find(n.node.id);
    for (auto &[k, v] : n.node.attributes)
        if (k != "action" && !k.starts_with("data-binding-")) {
            if (b != boundValues_.end() && b->second.contains(k))
                context.parameters[k] = b->second.at(k);
            else
                context.parameters[k] = Value(v);
        }
    if (auto error = actions_.invoke(context))
        diagnostics_.push_back({Severity::error, n.node.source, n.node.line, 1, *error});
}
} // namespace v1
} // namespace voxelui
