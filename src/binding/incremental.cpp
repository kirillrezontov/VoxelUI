#include "../runtime/internal.hpp"
namespace voxelui {
inline namespace v1 {
using namespace runtime_detail;
namespace {
bool overlaps(std::string_view a, std::string_view b) {
    return a == b || (a.starts_with(b) && a.size() > b.size() && a[b.size()] == '.') ||
           (b.starts_with(a) && b.size() > a.size() && b[a.size()] == '.');
}
bool paintValue(const Node &n, std::string_view attribute) {
    if (!builtinTag(n.tag))
        return false;
    return attribute == "tooltip" || attribute == "min" || attribute == "max" || attribute == "step" ||
           (attribute == "value" &&
            (n.tag == "Slider" || n.tag == "ProgressBar" || n.tag == "Checkbox" || n.tag == "Toggle"));
}
} // namespace
const Value *Runtime::bindingValue(const BindingSource &source) const {
    auto *value = data_.get(source.path);
    for (auto &step : source.steps) {
        if (auto *name = std::get_if<std::string>(&step))
            value = property(value, *name);
        else {
            auto *array = value ? std::get_if<Value::Array>(&value->data) : nullptr;
            auto index = std::get<std::size_t>(step);
            value = array && index < array->size() ? &(*array)[index] : nullptr;
        }
    }
    return value;
}
void Runtime::indexTree(bool reuseIds) {
    if (!reuseIds)
        nodeIndex_.clear();
    std::function<void(LayoutNode &, LayoutNode *)> visit = [&](LayoutNode &n, LayoutNode *parent) {
        if (reuseIds)
            nodeIndex_.at(n.node.id) = {&n, parent};
        else
            nodeIndex_[n.node.id] = {&n, parent};
        for (auto &c : n.children)
            visit(c, &n);
    };
    if (tree_)
        visit(*tree_, nullptr);
}
void Runtime::invalidateMeasure(LayoutNode &node) {
    auto *n = &node;
    while (n) {
        n->measureDirty = true;
        n->placementDirty = true;
        auto it = nodeIndex_.find(n->node.id);
        n = it == nodeIndex_.end() ? nullptr : it->second.parent;
    }
    layoutDirty_ = true;
}
void Runtime::invalidatePlacement(LayoutNode &node) {
    auto *n = &node;
    while (n) {
        n->placementDirty = true;
        n = nodeIndex_.at(n->node.id).parent;
    }
    layoutDirty_ = true;
}
void Runtime::invalidateStyle(std::string_view id) {
    if (!id.empty())
        styleDirty_.emplace(id);
}
void Runtime::layout() {
    if (!tree_)
        return;
    std::map<std::string, Vec2> scroll;
    for (auto &[id, entry] : nodeIndex_)
        if (entry.node->node.tag == "ScrollView")
            scroll[id] = states_.at(id).scroll;
    float scale = frame_.uiScale;
    computeLayout(*tree_, {frame_.viewport.x / scale, frame_.viewport.y / scale}, text_, components_, scroll,
                  stats_, true);
    runtimeStats_.measuredNodes += stats_.nodes;
    runtimeStats_.placedNodes += stats_.placed;
    // Store the effective offset, including after content shrink/removal.
    for (auto &[id, offset] : scroll) {
        auto &n = *nodeIndex_.at(id).node;
        float p = n.style.padding + n.style.borderWidth;
        states_.at(id).scroll = {
            std::clamp(offset.x, 0.f, std::max(0.f, n.contentSize.x - std::max(0.f, n.bounds.w - 2 * p))),
            std::clamp(offset.y, 0.f, std::max(0.f, n.contentSize.y - std::max(0.f, n.bounds.h - 2 * p)))};
    }
    layoutDirty_ = false;
}
void Runtime::validateInteraction() {
    if (!tree_)
        return;
    auto allowed = [&](const std::string &id) {
        auto it = nodeIndex_.find(id);
        if (it == nodeIndex_.end())
            return false;
        auto *n = it->second.node;
        while (n) {
            if (!n->style.visible || n->node.attr("disabled") == "true")
                return false;
            n = nodeIndex_.at(n->node.id).parent;
        }
        auto *activeModal = modal(*tree_);
        return !activeModal || search(*activeModal, id);
    };
    for (auto *id : {&focus_, &hover_, &capture_, &pressed_, &drag_})
        if (!id->empty() && !allowed(*id)) {
            invalidateStyle(*id);
            if (id == &focus_)
                if (auto state = states_.find(*id); state != states_.end())
                    state->second.composition.clear();
            id->clear();
        }
    if (capture_.empty())
        slotCapture_.clear();
}
bool Runtime::patchBindings() {
    if (!tree_)
        return true;
    auto affected = [&](const std::string &path) {
        return std::ranges::any_of(data_.changes(), [&](auto &change) { return overlaps(path, change); });
    };
    // Stage source remapping before evaluating values. Tree order changes only
    // after all ordinary value/action/style validation has succeeded.
    struct MetadataTransaction {
        std::vector<BindingTarget> &bindings;
        std::vector<RepeatBinding> &repeats;
        std::optional<std::vector<BindingTarget>> oldBindings;
        std::optional<std::vector<RepeatBinding>> oldRepeats;
        bool committed{};
        MetadataTransaction(std::vector<BindingTarget> &b, std::vector<RepeatBinding> &r)
            : bindings(b), repeats(r) {}
        void save() {
            if (!oldBindings) {
                oldBindings = bindings;
                oldRepeats = repeats;
            }
        }
        ~MetadataTransaction() {
            if (oldBindings && !committed) {
                bindings.swap(*oldBindings);
                repeats.swap(*oldRepeats);
            }
        }
    } transaction(bindings_, repeats_);
    struct Reorder {
        LayoutNode *parent;
        std::size_t first;
        std::vector<std::size_t> order;
    };
    std::vector<Reorder> reorders;
    struct SourceRemap {
        BindingSource source;
        std::vector<std::size_t> indices;
    };
    std::vector<SourceRemap> sourceRemaps;
    for (std::size_t repeatIndex = 0; repeatIndex < repeats_.size(); ++repeatIndex) {
        auto &repeat = repeats_[repeatIndex];
        if (repeat.source.path.empty() || !affected(repeat.source.path))
            continue;
        auto *v = bindingValue(repeat.source);
        auto *array = v ? std::get_if<Value::Array>(&v->data) : nullptr;
        if (!array || array->size() != repeat.keys.size())
            return false;
        bool changed = false;
        for (std::size_t i = 0; i < array->size(); ++i) {
            auto *key = property(&(*array)[i], repeat.key);
            if (!key)
                return false;
            changed |= key->string() != repeat.keys[i];
        }
        if (!changed)
            continue;
        if (repeat.roots.size() != array->size() ||
            std::ranges::any_of(repeat.roots, [](auto &id) { return id.empty(); }))
            return false;
        std::vector<std::string> keys;
        keys.reserve(array->size());
        for (auto &item : *array)
            keys.push_back(property(&item, repeat.key)->string());
        std::vector<std::size_t> oldIndices(repeat.keys.size());
        for (std::size_t i = 0; i < repeat.keys.size(); ++i)
            oldIndices[i] = i;
        auto less = [&](auto a, auto b) { return repeat.keys[a] < repeat.keys[b]; };
        std::sort(oldIndices.begin(), oldIndices.end(), less);
        std::vector<std::size_t> order, remap(keys.size());
        order.reserve(keys.size());
        std::vector<bool> seen(keys.size());
        for (std::size_t i = 0; i < keys.size(); ++i) {
            auto found = std::lower_bound(oldIndices.begin(), oldIndices.end(), keys[i],
                [&](auto index, const auto &key) { return repeat.keys[index] < key; });
            if (found == oldIndices.end() || repeat.keys[*found] != keys[i] || seen[*found])
                return false; // Insert/remove/invalid keys retain candidate validation.
            seen[*found] = true;
            order.push_back(*found);
            remap[*found] = i;
        }
        auto *parent = nodeIndex_.at(repeat.roots.front()).parent;
        if (!parent)
            return false;
        auto begin = std::ranges::find_if(parent->children, [&](auto &n) {
            return n.node.id == repeat.roots.front();
        });
        auto first = std::size_t(begin - parent->children.begin());
        if (first + order.size() > parent->children.size())
            return false;
        for (std::size_t i = 0; i < order.size(); ++i)
            if (parent->children[first + i].node.id != repeat.roots[i])
                return false;
        transaction.save();
        auto source = repeat.source;
        auto remapSource = [&](BindingSource &target) {
            auto index = source.steps.size();
            if (target.path == source.path && target.steps.size() > index &&
                std::equal(source.steps.begin(), source.steps.end(), target.steps.begin())) {
                auto *item = std::get_if<std::size_t>(&target.steps[index]);
                if (item && *item < remap.size())
                    *item = remap[*item];
            }
        };
        auto prior = std::ranges::find_if(sourceRemaps, [&](auto &entry) {
            return entry.source.path == source.path && entry.source.steps == source.steps;
        });
        if (prior == sourceRemaps.end()) {
            // Several independent views can bind the same collection. Remap
            // their shared positional source exactly once, but reorder each view.
            for (auto &binding : bindings_)
                remapSource(binding.source);
            for (auto &nested : repeats_)
                remapSource(nested.source);
            sourceRemaps.push_back({std::move(source), remap});
        } else if (prior->indices != remap)
            return false; // Different key fields disagree; use candidate validation.
        std::vector<std::string> roots;
        roots.reserve(order.size());
        for (auto index : order)
            roots.push_back(repeat.roots[index]);
        repeat.keys = std::move(keys);
        repeat.roots = std::move(roots);
        reorders.push_back({parent, first, std::move(order)});
    }
    std::set<std::size_t> targets;
    for (auto &change : data_.changes()) {
        // Exact paths and ancestors, then descendants in the ordered dependency index.
        std::string_view ancestor = change;
        for (;;) {
            if (auto it = bindingIndex_.find(ancestor); it != bindingIndex_.end())
                targets.insert(it->second.begin(), it->second.end());
            auto dot = ancestor.rfind('.');
            if (dot == ancestor.npos)
                break;
            ancestor = ancestor.substr(0, dot);
        }
        auto prefix = change + ".";
        for (auto it = bindingIndex_.lower_bound(prefix);
             it != bindingIndex_.end() && it->first.starts_with(prefix); ++it)
            targets.insert(it->second.begin(), it->second.end());
    }
    struct Patch {
        LayoutNode *target;
        Node node;
        std::map<std::string, Value> values;
        ElementState state;
        bool measure{}, style{};
    };
    std::map<std::string, Patch> patches;
    auto diagnosticStart = diagnostics_.size();
    for (auto index : targets) {
        auto &binding = bindings_[index];
        ++runtimeStats_.bindingEvaluations;
        auto *value = bindingValue(binding.source);
        auto &entry = nodeIndex_.at(binding.id);
        auto &current = *entry.node;
        auto text = value ? value->string() : std::string{};
        auto &old = boundValues_.at(binding.id);
        bool equal = binding.attribute.empty()
                         ? current.node.text == text
                         : (value ? old.contains(binding.attribute) && old.at(binding.attribute) == *value
                                  : !old.contains(binding.attribute));
        if (equal)
            continue;
        // Resource and identity changes use the transactional expansion path.
        if (binding.attribute == "source" || binding.attribute == "id" || binding.attribute == "stylesheet")
            return false;
        auto it = patches.find(binding.id);
        if (it == patches.end())
            it = patches.emplace(binding.id, Patch{entry.node, current.node, old, states_.at(binding.id)})
                     .first;
        auto &patch = it->second;
        if (binding.attribute.empty())
            patch.node.text = text;
        else {
            patch.node.attributes[binding.attribute] = text;
            if (value)
                patch.values[binding.attribute] = *value;
            else
                patch.values.erase(binding.attribute);
        }
        if (!value)
            diagnostics_.push_back({Severity::warning, current.node.source, current.node.line, 1,
                                    "Missing binding: " + binding.source.path});
        if (binding.attribute == "value" && value) {
            bool expected = current.node.tag == "Slider" || current.node.tag == "ProgressBar"
                                ? matchesType(*value, ValueType::number)
                            : current.node.tag == "Toggle" || current.node.tag == "Checkbox"
                                ? matchesType(*value, ValueType::boolean)
                            : current.node.tag == "TextInput" ? matchesType(*value, ValueType::string)
                                                              : true;
            if (!expected)
                diagnostics_.push_back({Severity::error, current.node.source, current.node.line, 1,
                                        "Incorrect type for bound value on " + current.node.tag});
            if (current.node.tag == "TextInput" || current.node.tag == "Select") {
                patch.state.text = text;
                patch.state.composition.clear();
                auto boundaries = utf8Boundaries(text);
                auto snap = [&](std::size_t offset) {
                    return *std::prev(std::upper_bound(boundaries.begin(), boundaries.end(),
                                                       std::min(offset, text.size())));
                };
                patch.state.caret = snap(patch.state.caret);
                patch.state.anchor = snap(patch.state.anchor);
            }
            if (auto numeric = value->numeric())
                patch.state.value = *numeric;
            if (matchesType(*value, ValueType::boolean))
                patch.state.checked = value->truth();
        }
        if (binding.attribute == "checked")
            patch.state.checked = text == "true";
        if (binding.attribute == "selected")
            patch.state.selected = text == "true";
        if (current.node.tag == "TextInput")
            patch.node.attributes["value"] = patch.state.text;
        bool styleAttribute = binding.attribute == "checked" || binding.attribute == "selected" ||
                              binding.attribute == "style" || binding.attribute == "class" ||
                              binding.attribute == "disabled" || binding.attribute == "visible" ||
                              binding.attribute == "anchor" ||
                              (binding.attribute == "value" &&
                               (current.node.tag == "Checkbox" || current.node.tag == "Toggle"));
        patch.style |= styleAttribute;
        patch.measure |= (!styleAttribute || !builtinTag(current.node.tag)) &&
                         !paintValue(current.node, binding.attribute);
    }
    for (auto &[id, patch] : patches) {
        auto action = patch.node.attr("action");
        if (!action.empty()) {
            ActionContext context{id, action, {}};
            for (auto &[key, value] : patch.node.attributes)
                if (key != "action" && !key.starts_with("data-binding-"))
                    context.parameters[key] =
                        patch.values.contains(key) ? patch.values.at(key) : Value(value);
            if (auto error = actions_.validate(context))
                diagnostics_.push_back({Severity::error, patch.node.source, patch.node.line, 1, *error});
        }
    }
    auto candidateBytes = expandedBytes_;
    for (auto &[id, patch] : patches) {
        candidateBytes -= payloadSize(patch.target->node);
        candidateBytes += payloadSize(patch.node);
    }
    if (candidateBytes > 16 * 1024 * 1024)
        diagnostics_.push_back(
            {Severity::error, "<bindings>", 1, 1, "Expanded document payload exceeds 16 MiB"});
    auto failed = [&] {
        return std::any_of(diagnostics_.begin() + std::ptrdiff_t(diagnosticStart), diagnostics_.end(),
                           [](auto &d) { return d.severity == Severity::error; });
    };
    if (failed()) {
        bindingRejected_ = true;
        return true;
    } // Entire update is rejected; retain the last good state.
    auto priorStyleDirty = styleDirty_;
    for (auto &[id, patch] : patches) {
        std::swap(patch.target->node, patch.node);
        std::swap(boundValues_.at(id), patch.values);
        std::swap(states_.at(id), patch.state);
        if (patch.style)
            invalidateStyle(id);
    }
    if (!refreshStyles()) {
        bindingRejected_ = true;
        styleDirty_ = priorStyleDirty;
        for (auto &[id, patch] : patches) {
            std::swap(patch.target->node, patch.node);
            std::swap(boundValues_.at(id), patch.values);
            std::swap(states_.at(id), patch.state);
        }
        return true;
    }
    expandedBytes_ = candidateBytes;
    bool interactionChanged = !priorStyleDirty.empty();
    for (auto &[id, patch] : patches) {
        ++runtimeStats_.patchedNodes;
        interactionChanged |= patch.style;
        if (patch.measure)
            invalidateMeasure(*patch.target);
    }
    if (interactionChanged) {
        validateInteraction();
        refreshStyles();
    }
    // Nested repeats are processed before their parents move. Existing child
    // vectors/resources/state survive; IDs remain the only durable host handles.
    for (auto it = reorders.rbegin(); it != reorders.rend(); ++it) {
        invalidateMeasure(*it->parent);
        std::vector<LayoutNode> ordered;
        ordered.reserve(it->order.size());
        for (auto index : it->order)
            ordered.push_back(std::move(it->parent->children[it->first + index]));
        for (std::size_t i = 0; i < ordered.size(); ++i)
            it->parent->children[it->first + i] = std::move(ordered[i]);
    }
    if (!reorders.empty())
        indexTree(true); // A validated permutation changes addresses, not the ID set.
    transaction.committed = true;
    return true;
}
bool Runtime::refreshStyles() {
    if (styleDirty_.empty())
        return true;
    struct Pending {
        LayoutNode *node;
        Style style;
    };
    std::vector<Pending> pending;
    auto start = diagnostics_.size();
    std::set<std::string> visited;
    std::function<void(LayoutNode &, const Style *)> visit = [&](LayoutNode &n, const Style *parent) {
        if (!visited.insert(n.node.id).second)
            return;
        std::set<std::string> pseudo;
        if (n.node.id == focus_)
            pseudo.insert("focus");
        if (n.node.id == hover_)
            pseudo.insert("hover");
        if (n.node.id == pressed_)
            pseudo.insert("active");
        if (n.node.attr("disabled") == "true")
            pseudo.insert("disabled");
        auto &state = states_.at(n.node.id);
        if (state.selected || state.checked)
            pseudo.insert("selected");
        auto style = resolveStyle(n.node, parent, stylesheet_, pseudo, &diagnostics_);
        if (n.node.attr("visible") == "false")
            style.visible = false;
        if (!n.node.attr("anchor").empty())
            style.anchor = n.node.attr("anchor");
        if (style.font != "ui:font" || resources_.get("ui:font")) {
            auto font = resources_.get(style.font, &diagnostics_);
            if (font && !text_.validateFont(style.font))
                diagnostics_.push_back(
                    {Severity::error, n.node.source, n.node.line, 1, "Invalid font resource: " + style.font});
        }
        ++runtimeStats_.styledNodes;
        // Only inherited fields can affect descendants; selectors have no combinators.
        bool inherited = style.foreground != n.style.foreground || style.font != n.style.font ||
                         style.fontSize != n.style.fontSize || style.fontWeight != n.style.fontWeight ||
                         style.textAlign != n.style.textAlign || style.variables != n.style.variables ||
                         style.opacity != n.style.opacity;
        if (inherited)
            for (auto &c : n.children)
                visit(c, &style);
        pending.push_back({&n, std::move(style)});
    };
    // Visit parents before explicitly dirty descendants.
    std::vector<std::pair<std::size_t, LayoutNode *>> roots;
    for (auto &id : styleDirty_) {
        auto it = nodeIndex_.find(id);
        if (it == nodeIndex_.end())
            continue;
        std::size_t depth = 0;
        for (auto *p = it->second.parent; p; p = nodeIndex_.at(p->node.id).parent)
            ++depth;
        roots.emplace_back(depth, it->second.node);
    }
    std::stable_sort(roots.begin(), roots.end(), [](auto &a, auto &b) { return a.first < b.first; });
    for (auto &[depth, n] : roots) {
        auto *parent = nodeIndex_.at(n->node.id).parent;
        visit(*n, parent ? &parent->style : nullptr);
    }
    styleDirty_.clear();
    if (std::any_of(diagnostics_.begin() + std::ptrdiff_t(start), diagnostics_.end(),
                    [](auto &d) { return d.severity == Severity::error; }))
        return false;
    for (auto &p : pending) {
        auto &n = *p.node;
        auto &style = p.style;
        auto previous = previousStyles_.find(n.node.id);
        if (previous != previousStyles_.end() && style.transition > 0 &&
            (previous->second.background != style.background ||
             previous->second.foreground != style.foreground || previous->second.border != style.border ||
             previous->second.opacity != style.opacity))
            animations_[n.node.id] = {previous->second, style, 0, style.transition};
        previousStyles_[n.node.id] = style;
        // Measurement is cached for untouched siblings; placement remains conservative.
        bool measure = !layoutEquivalent(n.style, style);
        n.style = std::move(style);
        if (measure)
            invalidateMeasure(n);
    }
    return true;
}
} // namespace v1
} // namespace voxelui
