#include "internal.hpp"
#include <functional>
#include <sstream>
namespace voxelui {
inline namespace v1 {
using namespace runtime_detail;
const LayoutNode *Runtime::find(std::string_view id) const {
    auto it = nodeIndex_.find(id);
    return it == nodeIndex_.end() ? nullptr : it->second.node;
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
        if (!style.empty() && !bindingPath(style))
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
    auto oldPayload = expandedBytes_;
    auto oldDataRevision = dataRevision_;
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
    auto oldBindings = bindings_;
    auto oldRepeats = repeats_;
    auto oldBindingIndex = bindingIndex_;
    auto oldHover = hover_, oldPressed = pressed_, oldDrag = drag_, oldSlotCapture = slotCapture_;
    auto oldAnimations = animations_;
    auto oldStyles = previousStyles_;
    document_ = std::move(parsed.document);
    stylesheet_ = std::move(candidate);
    dirty_ = true;
    rebuild();
    if (hasErrors(diagnostics_)) {
        expandedBytes_ = oldPayload;
        dataRevision_ = oldDataRevision;
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
        bindings_ = std::move(oldBindings);
        repeats_ = std::move(oldRepeats);
        bindingIndex_ = std::move(oldBindingIndex);
        hover_ = oldHover;
        pressed_ = oldPressed;
        drag_ = oldDrag;
        slotCapture_ = oldSlotCapture;
        indexTree();
        animations_ = std::move(oldAnimations);
        previousStyles_ = std::move(oldStyles);
        focus_ = oldFocus;
        capture_ = oldCapture;
        dirty_ = false;
        return false;
    }
    screenId_.clear();
    bindingRejected_ = false;
    data_.acknowledge();
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
    nodeIndex_.clear();
    bindings_.clear();
    repeats_.clear();
    bindingIndex_.clear();
    dependencies_.clear();
    styleDirty_.clear();
    layoutDirty_ = false;
    bindingRejected_ = false;
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
bool Runtime::invalidateMeasurement(std::string_view id) {
    auto it = nodeIndex_.find(id);
    if (it == nodeIndex_.end())
        return false;
    invalidateMeasure(*it->second.node);
    return true;
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
        layoutDirty_ = true;
    frame_ = frame;
    if (components_.revision() != componentRevision_) {
        componentRevision_ = components_.revision();
        for (auto &[id, entry] : nodeIndex_)
            if (!builtinTag(entry.node->node.tag))
                invalidateMeasure(*entry.node);
    }
    if (resources_.epoch() != resourceEpoch_) {
        resourceEpoch_ = resources_.epoch();
        vectors_.clear();
        text_.clear();
        if (!screenId_.empty()) {
            if (!reload() && layoutDirty_)
                layout();
            return;
        }
        dirty_ = true;
    }
    if (dataRevision_ != data_.revision()) {
        bindingRejected_ = false;
        if (!dirty_ && !patchBindings())
            dirty_ = true;
        dataRevision_ = data_.revision();
    }
    if (dirty_) {
        auto oldPayload = expandedBytes_;
        auto oldTree = tree_;
        auto oldStates = states_;
        auto oldTypes = stateTypes_;
        auto oldBound = boundValues_;
        auto oldFocus = focus_, oldCapture = capture_;
        auto oldDeps = dependencies_;
        auto oldBindings = bindings_;
        auto oldRepeats = repeats_;
        auto oldBindingIndex = bindingIndex_;
        auto oldHover = hover_, oldPressed = pressed_, oldDrag = drag_, oldSlotCapture = slotCapture_;
        auto oldImages = images_;
        auto oldVectors = vectorDocuments_;
        auto oldImageRevisions = imageRevisions_;
        auto oldStyles = previousStyles_;
        auto oldAnimations = animations_;
        auto diagnosticStart = diagnostics_.size();
        rebuild();
        bool failed = std::any_of(diagnostics_.begin() + std::ptrdiff_t(diagnosticStart), diagnostics_.end(),
                                  [](auto &d) { return d.severity == Severity::error; });
        if (failed) {
            bindingRejected_ = true;
            expandedBytes_ = oldPayload;
            tree_ = std::move(oldTree);
            states_ = std::move(oldStates);
            stateTypes_ = std::move(oldTypes);
            boundValues_ = std::move(oldBound);
            focus_ = oldFocus;
            capture_ = oldCapture;
            dependencies_ = std::move(oldDeps);
            bindings_ = std::move(oldBindings);
            repeats_ = std::move(oldRepeats);
            bindingIndex_ = std::move(oldBindingIndex);
            hover_ = oldHover;
            pressed_ = oldPressed;
            drag_ = oldDrag;
            slotCapture_ = oldSlotCapture;
            images_ = std::move(oldImages);
            vectorDocuments_ = std::move(oldVectors);
            imageRevisions_ = std::move(oldImageRevisions);
            indexTree();
            previousStyles_ = std::move(oldStyles);
            animations_ = std::move(oldAnimations);
        }
    }
    bool interactionChanged = !styleDirty_.empty();
    refreshStyles();
    if (interactionChanged) {
        validateInteraction();
        refreshStyles();
    }
    if (layoutDirty_)
        layout();
    if (!bindingRejected_)
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
    for (auto it = animations_.begin(); it != animations_.end();) {
        auto node = nodeIndex_.find(it->first);
        if (node == nodeIndex_.end()) {
            it = animations_.erase(it);
            continue;
        }
        auto &n = *node->second.node;
        auto &a = it->second;
        a.elapsed += std::clamp(delta, 0.f, .25f);
        float t = std::clamp(a.elapsed / std::max(.001f, a.duration), 0.f, 1.f);
        n.style.background = mix(a.from.background, a.to.background, t);
        n.style.foreground = mix(a.from.foreground, a.to.foreground, t);
        n.style.border = mix(a.from.border, a.to.border, t);
        n.style.opacity = a.from.opacity + (a.to.opacity - a.from.opacity) * t;
        if (t >= 1)
            it = animations_.erase(it);
        else
            ++it;
    }
}
void Runtime::write(const LayoutNode &n, std::string_view attribute, Value value) {
    if (n.node.attr("writable") == "true") {
        auto path = n.node.attr("data-binding-" + std::string(attribute));
        if (!path.empty() && !path.starts_with("item.") && !path.starts_with("props."))
            data_.set(path, std::move(value));
    }
    if (auto it = nodeIndex_.find(n.node.id); it != nodeIndex_.end()) {
        if (n.node.tag == "TextInput") {
            auto &node = it->second.node->node;
            expandedBytes_ -= payloadSize(node);
            node.attributes["value"] = states_.at(n.node.id).text;
            expandedBytes_ += payloadSize(node);
            invalidateMeasure(*it->second.node);
        }
        invalidateStyle(n.node.id);
    }
}
void Runtime::activate(const LayoutNode &ref) {
    auto n = ref.node;
    auto &state = states_[n.id];
    if (n.tag == "Checkbox" || n.tag == "Toggle") {
        state.checked = !state.checked;
        write(ref, "value", Value(state.checked));
    }
    if (n.tag == "Select") {
        std::vector<std::string> options;
        std::istringstream input(n.attr("options"));
        std::string option;
        while (std::getline(input, option, '|'))
            options.push_back(option);
        if (!options.empty()) {
            auto it = std::ranges::find(options, state.text);
            state.text =
                options[(it == options.end() ? 0 : (std::size_t(it - options.begin()) + 1) % options.size())];
            write(ref, "value", state.text);
        }
    }
    state.selected = true;
    invalidateStyle(n.id);
    ActionContext context{n.id, n.attr("action"), {}};
    if (context.action.empty())
        return;
    auto b = boundValues_.find(n.id);
    for (auto &[k, v] : n.attributes)
        if (k != "action" && !k.starts_with("data-binding-")) {
            if (b != boundValues_.end() && b->second.contains(k))
                context.parameters[k] = b->second.at(k);
            else
                context.parameters[k] = Value(v);
        }
    if (auto error = actions_.invoke(context))
        diagnostics_.push_back({Severity::error, n.source, n.line, 1, *error});
}
} // namespace v1
} // namespace voxelui
