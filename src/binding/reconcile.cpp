#include "../runtime/internal.hpp"
namespace voxelui {
inline namespace v1 {
using namespace runtime_detail;
void Runtime::rebuild() {
    if (!document_)
        return;
    ++runtimeStats_.fullRebuilds;
    bindings_.clear();
    repeats_.clear();
    bindingIndex_.clear();
    boundValues_.clear();
    dependencies_.clear();
    std::set<std::string> live;
    std::size_t count = 0, generatedBytes = 0;
    Limits limits;
    std::function<std::vector<LayoutNode>(const Node &, const Style *, std::string, const Value *,
                                          const Value *, std::size_t)>
        expand;
    auto get = [&](std::string_view path, const Value *item, const Value *props) -> const Value * {
        if (path == "item")
            return item;
        if (path.starts_with("item."))
            return property(item, path.substr(5));
        if (path.starts_with("props."))
            return property(props, path.substr(6));
        dependencies_.insert(std::string(path));
        return data_.get(path);
    };
    BindingSource itemSource;
    std::map<std::string, BindingSource> propSources;
    auto sourceFor = [&](std::string_view path) -> BindingSource {
        if (path == "item" || path.starts_with("item.")) {
            auto source = itemSource;
            if (path.size() > 5)
                source.steps.emplace_back(std::string(path.substr(5)));
            return source;
        }
        if (path.starts_with("props.")) {
            auto name = path.substr(6);
            auto dot = name.find('.');
            auto it = propSources.find(std::string(name.substr(0, dot)));
            if (it == propSources.end())
                return {};
            auto source = it->second;
            if (dot != name.npos)
                source.steps.emplace_back(std::string(name.substr(dot + 1)));
            return source;
        }
        return {std::string(path), {}};
    };
    auto track = [&](std::string_view path, const std::string &id, std::string attribute) {
        ++runtimeStats_.bindingEvaluations;
        auto source = sourceFor(path);
        if (!source.path.empty()) {
            dependencies_.insert(source.path);
            bindingIndex_[source.path].push_back(bindings_.size());
            bindings_.push_back({std::move(source), id, std::move(attribute)});
        }
    };
    // Authored ordinals, unlike generated counts, survive insertion into an earlier repeat.
    std::map<const Node *, std::size_t> ordinals;
    std::function<void(const Node &)> enumerate = [&](const Node &n) {
        ordinals[&n] = ordinals.size();
        for (auto &c : n.children)
            enumerate(c);
    };
    enumerate(document_->root);
    for (auto &[name, n] : document_->templates)
        enumerate(n);
    expand = [&](const Node &authored, const Style *parent, std::string prefix, const Value *item,
                 const Value *props, std::size_t depth) -> std::vector<LayoutNode> {
        if (depth > limits.depth || ++count > limits.elements) {
            diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                    "Generated widget/depth limit exceeded"});
            return {};
        }
        if (authored.tag == "Template")
            return {};
        if (authored.tag == "Component") {
            auto it = document_->templates.find(authored.attr("template"));
            if (it == document_->templates.end())
                return {};
            Value::Object params;
            auto savedSources = propSources;
            std::map<std::string, BindingSource> nextSources;
            for (auto &[k, v] : authored.attributes) {
                if (auto path = bindingPath(v)) {
                    if (auto *val = get(*path, item, props))
                        params[k] = *val;
                    nextSources[k] = sourceFor(*path);
                } else
                    params[k] = Value(v);
            }
            Value object(std::move(params));
            auto identity =
                prefix +
                (authored.id.empty() ? "component-" + std::to_string(ordinals.at(&authored)) : authored.id) +
                "/";
            propSources = std::move(nextSources);
            auto result = expand(it->second, parent, identity, item, &object, depth + 1);
            propSources = std::move(savedSources);
            return result;
        }
        if (authored.tag == "Repeat") {
            std::vector<LayoutNode> result;
            auto path = bindingPath(authored.attr("items"));
            auto *value = path ? get(*path, item, props) : nullptr;
            auto *array = value ? std::get_if<Value::Array>(&value->data) : nullptr;
            if (!array) {
                repeats_.push_back({path ? sourceFor(*path) : BindingSource{}, authored.attr("key"), {}});
                diagnostics_.push_back({Severity::warning, authored.source, authored.line, 1,
                                        "Repeat items is missing or is not an array"});
                return result;
            }
            if (array->size() > limits.elements) {
                diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                        "Repeat collection exceeds generated element budget"});
                return {};
            }
            std::set<std::string> keys;
            std::string identity =
                prefix +
                (authored.id.empty() ? "repeat-" + std::to_string(ordinals.at(&authored)) : authored.id) +
                "/";
            auto repeatSource = path ? sourceFor(*path) : BindingSource{};
            auto repeatIndex = repeats_.size();
            repeats_.push_back({repeatSource, authored.attr("key"), {}});
            auto savedItemSource = itemSource;
            std::size_t itemIndex = 0;
            for (auto &entry : *array) {
                auto *keyValue = property(&entry, authored.attr("key"));
                auto key = keyValue ? keyValue->string() : std::string{};
                if (key.empty() || key.size() > 256 || !keys.insert(key).second) {
                    diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                            "Repeat requires unique, nonempty scalar keys"});
                    continue;
                }
                repeats_[repeatIndex].keys.push_back(key);
                itemSource = repeatSource;
                itemSource.steps.emplace_back(itemIndex++);
                std::string safe;
                for (unsigned char c : key) {
                    static const char hex[] = "0123456789abcdef";
                    safe += hex[c >> 4];
                    safe += hex[c & 15];
                }
                auto children = expand(authored.children.front(), parent, identity + safe + "/", &entry,
                                       props, depth + 1);
                for (auto &c : children)
                    result.push_back(std::move(c));
                if (count > limits.elements)
                    break;
            }
            itemSource = std::move(savedItemSource);
            return result;
        }
        LayoutNode out;
        out.node.tag = authored.tag;
        out.node.text = authored.text;
        out.node.source = authored.source;
        out.node.line = authored.line;
        out.node.attributes = authored.attributes;
        out.node.id =
            prefix + (authored.id.empty() ? "@" + std::to_string(ordinals.at(&authored)) : authored.id);
        if (!live.insert(out.node.id).second) {
            diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                    "Duplicate expanded identity: " + out.node.id});
            return {};
        }
        auto &bound = boundValues_[out.node.id];
        for (auto &[k, v] : out.node.attributes) {
            if (auto path = bindingPath(v)) {
                track(*path, out.node.id, k);
                out.node.attributes["data-binding-" + k] = *path;
                if (auto *val = get(*path, item, props)) {
                    bound[k] = *val;
                    v = val->string();
                } else {
                    v.clear();
                    diagnostics_.push_back(
                        {Severity::warning, authored.source, authored.line, 1, "Missing binding: " + *path});
                }
            } else if (v.starts_with('{'))
                diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                        "Invalid binding syntax; only property paths are supported"});
        }
        if (auto path = bindingPath(out.node.text)) {
            track(*path, out.node.id, {});
            auto *val = get(*path, item, props);
            out.node.text = val ? val->string() : "";
        }
        generatedBytes += out.node.id.size() + out.node.tag.size() + out.node.text.size();
        for (auto &[key, value] : out.node.attributes)
            generatedBytes += key.size() + value.size();
        if (generatedBytes > 16 * 1024 * 1024) {
            diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                    "Expanded document payload exceeds 16 MiB"});
            return {};
        }
        if (!stateTypes_.contains(out.node.id) || stateTypes_[out.node.id] != out.node.tag) {
            if (stateTypes_.contains(out.node.id)) {
                if (focus_ == out.node.id)
                    focus_.clear();
                if (capture_ == out.node.id)
                    capture_.clear();
            }
            states_[out.node.id] = {};
            stateTypes_[out.node.id] = out.node.tag;
            auto &st = states_[out.node.id];
            st.text = out.node.attr("value");
            st.caret = st.anchor = st.text.size();
            st.checked = out.node.attr("checked") == "true" || out.node.attr("value") == "true";
            st.selected = out.node.attr("selected") == "true";
            st.value = numeric(out.node, "value");
        }
        auto &state = states_[out.node.id];
        if (bound.contains("value")) {
            auto &value = bound["value"];
            bool expected = out.node.tag == "Slider" || out.node.tag == "ProgressBar"
                                ? matchesType(value, ValueType::number)
                            : out.node.tag == "Checkbox" || out.node.tag == "Toggle"
                                ? matchesType(value, ValueType::boolean)
                            : out.node.tag == "TextInput" ? matchesType(value, ValueType::string)
                                                          : true;
            if (!expected)
                diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                        "Incorrect type for bound value on " + out.node.tag});
            if ((out.node.tag == "TextInput" || out.node.tag == "Select") && state.text != value.string()) {
                state.text = value.string();
                state.composition.clear();
                auto boundaries = utf8Boundaries(state.text);
                auto snap = [&](std::size_t offset) {
                    return *std::prev(std::upper_bound(boundaries.begin(), boundaries.end(),
                                                       std::min(offset, state.text.size())));
                };
                state.caret = snap(state.caret);
                state.anchor = snap(state.anchor);
            }
            if (value.numeric())
                state.value = *value.numeric();
            if (matchesType(value, ValueType::boolean))
                state.checked = value.truth();
        }
        if (bound.contains("checked"))
            state.checked = out.node.attr("checked") == "true";
        if (bound.contains("selected"))
            state.selected = out.node.attr("selected") == "true";
        if (out.node.tag == "TextInput")
            out.node.attributes["value"] = state.text;
        std::set<std::string> pseudo;
        if (out.node.id == focus_)
            pseudo.insert("focus");
        if (out.node.id == hover_)
            pseudo.insert("hover");
        if (out.node.id == pressed_)
            pseudo.insert("active");
        if (out.node.attr("disabled") == "true")
            pseudo.insert("disabled");
        if (state.selected || state.checked)
            pseudo.insert("selected");
        ++runtimeStats_.styledNodes;
        out.style = resolveStyle(out.node, parent, stylesheet_, pseudo, &diagnostics_);
        if (auto previous = previousStyles_.find(out.node.id);
            previous != previousStyles_.end() && out.style.transition > 0) {
            if (previous->second.background != out.style.background ||
                previous->second.foreground != out.style.foreground ||
                previous->second.opacity != out.style.opacity)
                animations_[out.node.id] = {previous->second, out.style, 0, out.style.transition};
        }
        previousStyles_[out.node.id] = out.style;
        if (out.style.font != "ui:font" || resources_.get("ui:font")) {
            auto font = resources_.get(out.style.font, &diagnostics_);
            if (font && !text_.validateFont(out.style.font))
                diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                        "Invalid font resource: " + out.style.font});
        }
        if (out.node.attr("visible") == "false")
            out.style.visible = false;
        if (!out.node.attr("anchor").empty())
            out.style.anchor = out.node.attr("anchor");
        if (out.node.tag == "Image") {
            auto id = out.node.attr("source");
            auto resource = resources_.get(id);
            if (!resource) {
                if (images_.contains(id) && imageRevisions_[id] != 0)
                    diagnostics_.push_back(
                        {Severity::error, id, 1, 1, "Image resource disappeared; previous screen retained"});
                else {
                    auto fallback = std::make_shared<Image>();
                    fallback->width = fallback->height = 8;
                    fallback->rgba.resize(256);
                    for (int y = 0; y < 8; ++y)
                        for (int x = 0; x < 8; ++x) {
                            auto i = (y * 8 + x) * 4;
                            bool light = (x / 4 + y / 4) % 2;
                            fallback->rgba[i] = light ? 220 : 45;
                            fallback->rgba[i + 1] = light ? 60 : 35;
                            fallback->rgba[i + 2] = light ? 160 : 50;
                            fallback->rgba[i + 3] = 255;
                        }
                    images_[id] = fallback;
                    imageRevisions_[id] = 0;
                    diagnostics_.push_back({Severity::warning, id, 1, 1,
                                            "Missing image resource: checkerboard fallback displayed"});
                }
            }
            if (resource) {
                if (!images_.contains(resource->id) || imageRevisions_[resource->id] != resource->revision) {
                    auto image = decodeImage(*resource, diagnostics_);
                    if (image) {
                        std::size_t bytes = image->rgba.size();
                        for (auto &[id, cached] : images_)
                            if (id != resource->id)
                                bytes += cached->rgba.size();
                        if (bytes > 128 * 1024 * 1024) {
                            diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                                    "Decoded image budget exceeded"});
                            return {};
                        }
                        images_[resource->id] = image;
                        imageRevisions_[resource->id] = resource->revision;
                    }
                }
            }
        }
        if (out.node.tag == "Vector") {
            auto resource = resources_.get(out.node.attr("source"), &diagnostics_);
            if (resource)
                if (auto geometry = vectors_.get(*resource, diagnostics_, &resources_)) {
                    std::size_t vertices = 0, rasterBytes = 0;
                    auto count = [&](const VectorDocument &document) {
                        for (auto &shape : document.shapes) {
                            for (auto &contour : shape.contours)
                                vertices += contour.size();
                            if (shape.image)
                                rasterBytes += shape.image->rgba.size();
                        }
                    };
                    count(*geometry);
                    for (auto &[id, cached] : vectorDocuments_)
                        if (id != resource->id)
                            count(*cached);
                    if (vertices > 1000000 || rasterBytes > 128 * 1024 * 1024) {
                        diagnostics_.push_back({Severity::error, authored.source, authored.line, 1,
                                                "Vector cache budget exceeded"});
                        return {};
                    }
                    vectorDocuments_[resource->id] = geometry;
                }
        }
        if (!out.node.attr("action").empty()) {
            ActionContext context{out.node.id, out.node.attr("action"), {}};
            for (auto &[key, value] : out.node.attributes)
                if (key != "action" && !key.starts_with("data-binding-"))
                    context.parameters[key] = bound.contains(key) ? bound.at(key) : Value(value);
            if (auto error = actions_.validate(context))
                diagnostics_.push_back({Severity::error, authored.source, authored.line, 1, *error});
        }
        for (auto &child : authored.children) {
            auto children = expand(child, &out.style, prefix, item, props, depth + 1);
            for (auto &c : children)
                out.children.push_back(std::move(c));
        }
        std::vector<LayoutNode> result;
        result.push_back(std::move(out));
        return result;
    };
    auto roots = expand(document_->root, nullptr, "", nullptr, nullptr, 0);
    if (!roots.empty())
        tree_ = std::make_shared<LayoutNode>(std::move(roots.front()));
    std::erase_if(previousStyles_, [&](auto &pair) { return !live.contains(pair.first); });
    std::erase_if(animations_, [&](auto &pair) { return !live.contains(pair.first); });
    std::erase_if(states_, [&](auto &pair) { return !live.contains(pair.first); });
    std::erase_if(stateTypes_, [&](auto &pair) { return !live.contains(pair.first); });
    if (!live.contains(focus_))
        focus_.clear();
    if (!live.contains(capture_))
        capture_.clear();
    if (!live.contains(hover_))
        hover_.clear();
    expandedBytes_ = generatedBytes;
    indexTree();
    validateInteraction();
    layout();
    styleDirty_.clear();
    dirty_ = false;
    dataRevision_ = data_.revision();
}
} // namespace v1
} // namespace voxelui
