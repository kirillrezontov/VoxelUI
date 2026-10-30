#pragma once
#include "types.hpp"
namespace voxelui {
inline namespace v1 {
enum class EventType {
    pointerMove,
    pointerDown,
    pointerUp,
    wheel,
    keyDown,
    keyUp,
    textInput,
    composition,
    resize,
    scale,
    focusLost,
    focusGained,
    hoverEnter,
    hoverLeave,
    activate,
    selection,
    dragStart,
    dragMove,
    dragEnd
};
enum class Key {
    none,
    tab,
    enter,
    space,
    escape,
    left,
    right,
    home,
    end,
    backspace,
    del,
    a,
    c,
    v,
    x,
    up,
    down
};
struct Event {
    EventType type{};
    Vec2 position{};
    float delta{};
    Key key{};
    std::string text;
    bool shift{}, control{};
    int button{1};
};
struct RoutedEvent {
    std::string target, current;
    Event event;
    bool handled{};
};
} // namespace v1
} // namespace voxelui
