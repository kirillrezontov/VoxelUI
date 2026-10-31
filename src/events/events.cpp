#include "../runtime/internal.hpp"
namespace voxelui {
inline namespace v1 {
using namespace runtime_detail;
bool Runtime::processEvent(const Event &input) {
    if (!tree_)
        return false;
    auto treeLifetime = tree_; // Retain event targets if a host callback loads or closes a screen.
    Event e = input;
    if (!std::isfinite(e.position.x) || !std::isfinite(e.position.y) || !std::isfinite(e.delta))
        return false;
    e.position.x /= frame_.uiScale;
    e.position.y /= frame_.uiScale;
    if (e.type == EventType::resize) {
        auto f = frame_;
        f.viewport = input.position;
        update(f);
        return true;
    }
    if (e.type == EventType::scale) {
        auto f = frame_;
        f.uiScale = input.delta;
        update(f);
        return true;
    }
    const auto *m = modal(*tree_);
    const auto *scope = m ? m : &*tree_;
    const LayoutNode *target = nullptr;
    if (!capture_.empty())
        target = find(capture_);
    if (!target)
        target = hitTest(*scope, e.position);
    std::vector<const LayoutNode *> path;
    if (target)
        pathTo(*scope, target->node.id, path);
    for (auto *n : path)
        if (n->node.attr("disabled") == "true")
            return true;
    if (e.type == EventType::pointerDown || e.type == EventType::pointerUp ||
        e.type == EventType::pointerMove) {
        for (auto it = path.rbegin(); it != path.rend(); ++it)
            if (focusable(**it) || (*it)->node.attr("draggable") == "true") {
                target = *it;
                break;
            }
    }
    std::string targetId = target ? target->node.id : "";
    if (target && target->node.tag == "Vector" &&
        (e.type == EventType::pointerDown || e.type == EventType::pointerMove ||
         e.type == EventType::pointerUp || e.type == EventType::wheel)) {
        auto it = vectorDocuments_.find(target->node.attr("source"));
        if (it != vectorDocuments_.end()) {
            auto &vector = *it->second;
            auto b = target->bounds;
            float p = target->style.padding + target->style.borderWidth;
            Rect content{b.x + p, b.y + p, std::max(0.f, b.w - 2 * p), std::max(0.f, b.h - 2 * p)};
            float k = std::min(content.w / vector.viewBox.w, content.h / vector.viewBox.h);
            Transform fit{k,
                          0,
                          0,
                          k,
                          content.x + (content.w - vector.viewBox.w * k) * .5f - vector.viewBox.x * k,
                          content.y + (content.h - vector.viewBox.h * k) * .5f - vector.viewBox.y * k};
            for (auto slot = vector.slots.rbegin(); slot != vector.slots.rend(); ++slot) {
                auto widget = slots_.find(slot->name);
                if (widget == slots_.end() || !widget->second.event)
                    continue;
                auto t = fit * slot->transform;
                float det = t.a * t.d - t.b * t.c;
                if (std::abs(det) < 1e-8f)
                    continue;
                Transform inv{t.d / det,
                              -t.b / det,
                              -t.c / det,
                              t.a / det,
                              (t.c * t.f - t.d * t.e) / det,
                              (t.b * t.e - t.a * t.f) / det};
                Event local = e;
                local.position = inv.apply(e.position);
                bool captured = slotCapture_ == targetId + "/" + slot->id;
                if (!captured && (!content.contains(e.position) || !slot->bounds.contains(local.position)))
                    continue;
                Node node;
                node.tag = "Slot";
                node.id = targetId + "/" + slot->id;
                auto definition = widget->second;
                bool handled = definition.event({node, slot->bounds}, local);
                if (handled) {
                    if (e.type == EventType::pointerDown) {
                        capture_ = targetId;
                        slotCapture_ = node.id;
                    }
                    if (e.type == EventType::pointerUp) {
                        capture_.clear();
                        slotCapture_.clear();
                    }
                    return true;
                }
            }
        }
    }
    auto notify = [&](EventType type, const std::string &id) {
        if (eventObserver_ && !id.empty()) {
            Event derived = e;
            derived.type = type;
            RoutedEvent routed{id, id, derived, false};
            eventObserver_(routed);
        }
    };
    auto setFocus = [&](std::string id) {
        if (focus_ != id) {
            auto old = focus_;
            focus_ = std::move(id);
            notify(EventType::focusLost, old);
            notify(EventType::focusGained, focus_);
            invalidateStyle(old);
            invalidateStyle(focus_);
        }
    };
    if (e.type == EventType::focusLost) {
        setFocus({});
        capture_.clear();
        invalidateStyle(pressed_);
        pressed_.clear();
        return true;
    }
    if (e.type == EventType::pointerMove) {
        if (hover_ != targetId) {
            invalidateStyle(hover_);
            notify(EventType::hoverLeave, hover_);
            hover_ = targetId;
            notify(EventType::hoverEnter, hover_);
            invalidateStyle(hover_);
        }
    }
    if (e.type == EventType::wheel) {
        for (auto it = path.rbegin(); it != path.rend(); ++it)
            if ((*it)->node.tag == "ScrollView") {
                auto &n = **it;
                auto &st = states_[n.node.id];
                float viewport = n.bounds.h - 2 * (n.style.padding + n.style.borderWidth);
                st.scroll.y =
                    std::clamp(st.scroll.y - e.delta * 36, 0.f, std::max(0.f, n.contentSize.y - viewport));
                layoutDirty_ = true;
                return true;
            }
    }
    if (e.type == EventType::pointerDown && e.button == 1) {
        invalidateStyle(pressed_);
        pressed_ = targetId;
        invalidateStyle(pressed_);
        capture_ = targetId;
        down_ = e.position;
        if (target && focusable(*target))
            setFocus(targetId);
        else if (!m)
            setFocus({});
        if (target && target->node.tag == "TextInput") {
            auto &st = states_[targetId];
            float x = e.position.x - target->bounds.x - target->style.padding;
            auto bounds = utf8Boundaries(st.text);
            std::size_t best = 0;
            for (auto boundary : bounds) {
                if (text_
                        .layout(std::string_view(st.text).substr(0, boundary), target->style.font,
                                target->style.fontSize)
                        ->size.x <= x)
                    best = boundary;
            }
            st.caret = best;
            if (!e.shift)
                st.anchor = best;
        }
    }
    if (target && (e.type == EventType::pointerDown || e.type == EventType::pointerMove) &&
        !capture_.empty()) {
        if (target->node.tag == "Slider") {
            auto &st = states_[targetId];
            double min = numeric(target->node, "min"),
                   max = std::max(min, double(numeric(target->node, "max", 1)));
            double value = min + (max - min) * std::clamp((e.position.x - target->bounds.x) /
                                                              std::max(1.f, target->bounds.w),
                                                          0.f, 1.f);
            double step = numeric(target->node, "step", .01f);
            if (step > 0)
                value = min + std::round((value - min) / step) * step;
            st.value = std::clamp(value, min, max);
            write(*target, "value", Value(st.value));
        }
        if (target->node.attr("draggable") == "true" && e.type == EventType::pointerMove) {
            if (drag_.empty() && std::hypot(e.position.x - down_.x, e.position.y - down_.y) > 4) {
                drag_ = targetId;
                notify(EventType::dragStart, drag_);
            }
            if (!drag_.empty())
                notify(EventType::dragMove, drag_);
        }
    }
    if (e.type == EventType::pointerUp && e.button == 1) {
        auto pressed = pressed_;
        invalidateStyle(pressed_);
        capture_.clear();
        pressed_.clear();
        if (!drag_.empty()) {
            auto source = drag_;
            drag_.clear();
            const auto *drop = hitTest(*scope, e.position);
            e.text = source;
            notify(EventType::dragEnd, drop ? drop->node.id : source);
        } else if (target && !pressed.empty() && targetId == pressed && target->bounds.contains(e.position)) {
            activate(*target);
            notify(EventType::activate, targetId);
            notify(EventType::selection, targetId);
        }
        return !pressed.empty() || m;
    }
    if (e.type == EventType::keyDown && e.key == Key::tab) {
        std::vector<std::string> order;
        std::function<void(const LayoutNode &, bool)> collect = [&](const LayoutNode &n, bool enabled) {
            enabled = enabled && n.style.visible && n.node.attr("disabled") != "true";
            if (!enabled)
                return;
            if (focusable(n))
                order.push_back(n.node.id);
            for (auto &c : n.children)
                collect(c, enabled);
        };
        collect(*scope, true);
        if (!order.empty()) {
            auto it = std::ranges::find(order, focus_);
            int idx = it == order.end() ? (e.shift ? 0 : -1) : int(it - order.begin());
            idx = (idx + (e.shift ? -1 : 1) + int(order.size())) % int(order.size());
            setFocus(order[std::size_t(idx)]);
        }
        return true;
    }
    if (e.type == EventType::keyDown || e.type == EventType::keyUp || e.type == EventType::textInput ||
        e.type == EventType::composition) {
        target = find(focus_);
        targetId = focus_;
        if (!target)
            return m != nullptr;
        if (target->node.tag == "TextInput") {
            auto &st = states_[targetId];
            auto boundaries = utf8Boundaries(st.text);
            st.caret = std::min(st.caret, st.text.size());
            st.anchor = std::min(st.anchor, st.text.size());
            auto erase = [&]() {
                auto a = std::min(st.caret, st.anchor), b = std::max(st.caret, st.anchor);
                st.text.erase(a, b - a);
                st.caret = st.anchor = a;
            };
            bool changed = false;
            if (e.type == EventType::composition) {
                st.composition = e.text;
                return true;
            }
            if (e.type == EventType::textInput) {
                erase();
                std::string text;
                for (auto cp : decodeUtf8(e.text))
                    if (cp >= 32 && cp != 127)
                        text += encodeUtf8(cp);
                if (st.text.size() + text.size() <= 65536) {
                    st.text.insert(st.caret, text);
                    st.caret += text.size();
                    st.anchor = st.caret;
                    changed = true;
                }
                st.composition.clear();
            }
            if (e.type == EventType::keyDown) {
                if (e.control && e.key == Key::a) {
                    st.anchor = 0;
                    st.caret = st.text.size();
                } else if (e.control && (e.key == Key::c || e.key == Key::x)) {
                    if (clipboardWrite_)
                        clipboardWrite_(
                            st.text.substr(std::min(st.caret, st.anchor),
                                           std::max(st.caret, st.anchor) - std::min(st.caret, st.anchor)));
                    if (e.key == Key::x) {
                        erase();
                        changed = true;
                    }
                } else if (e.control && e.key == Key::v) {
                    if (clipboardRead_) {
                        Event paste;
                        paste.type = EventType::textInput;
                        paste.text = clipboardRead_();
                        return processEvent(paste);
                    }
                } else if (e.key == Key::left || e.key == Key::right || e.key == Key::home ||
                           e.key == Key::end) {
                    if (e.key == Key::home)
                        st.caret = 0;
                    else if (e.key == Key::end)
                        st.caret = st.text.size();
                    else if (e.key == Key::left) {
                        auto it = std::lower_bound(boundaries.begin(), boundaries.end(), st.caret);
                        st.caret = it == boundaries.begin() ? 0 : *std::prev(it);
                    } else {
                        auto it = std::upper_bound(boundaries.begin(), boundaries.end(), st.caret);
                        st.caret = it == boundaries.end() ? st.text.size() : *it;
                    }
                    if (!e.shift)
                        st.anchor = st.caret;
                } else if (e.key == Key::backspace || e.key == Key::del) {
                    if (st.caret == st.anchor) {
                        if (e.key == Key::backspace) {
                            auto it = std::lower_bound(boundaries.begin(), boundaries.end(), st.caret);
                            if (it != boundaries.begin())
                                st.anchor = *std::prev(it);
                        } else {
                            auto it = std::upper_bound(boundaries.begin(), boundaries.end(), st.caret);
                            if (it != boundaries.end())
                                st.anchor = *it;
                        }
                    }
                    erase();
                    changed = true;
                }
            }
            if (changed)
                write(*target, "value", Value(st.text));
            return true;
        }
        if (e.type == EventType::keyDown && (e.key == Key::enter || e.key == Key::space)) {
            activate(*target);
            notify(EventType::activate, targetId);
            notify(EventType::selection, targetId);
            return true;
        }
        if (target->node.tag == "Slider" && e.type == EventType::keyDown &&
            (e.key == Key::left || e.key == Key::right)) {
            auto &st = states_[targetId];
            st.value =
                std::clamp(st.value + (e.key == Key::left ? -1 : 1) * numeric(target->node, "step", .01f),
                           double(numeric(target->node, "min")), double(numeric(target->node, "max", 1)));
            write(*target, "value", Value(st.value));
            return true;
        }
    }
    if (target) {
        auto nodeCopy = target->node;
        auto bounds = target->bounds;
        if (auto *custom = components_.get(nodeCopy.tag); custom && custom->event) {
            auto definition = *custom;
            if (definition.event({nodeCopy, bounds}, e))
                return true;
        }
        if (eventObserver_) {
            std::vector<const LayoutNode *> route;
            pathTo(*scope, targetId, route);
            std::vector<std::string> ids;
            for (auto *n : route)
                ids.push_back(n->node.id);
            RoutedEvent routed{targetId, {}, e, false};
            for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
                routed.current = *it;
                eventObserver_(routed);
                if (routed.handled)
                    return true;
            }
        }
    }
    return target != nullptr || m != nullptr || !capture_.empty();
}
} // namespace v1
} // namespace voxelui
