# Custom widgets

Components::registerWidget accepts a tag and concrete WidgetDefinition function
table: measure(Node, available) → Vec2; draw(WidgetContext, DrawList&); optional
input callback → bool consumed. Custom XML attributes are retained. WidgetContext
contains the resolved Node and final bounds. Callbacks are trusted host code.
The registry is a deliberate runtime dispatch boundary; built-ins use tagged data.

The concept adapter accepts a shared_ptr to a type implementing measure and draw:

```cpp
struct Badge {
 voxelui::Vec2 measure(const voxelui::Node&, voxelui::Vec2) { return {100, 32}; }
 void draw(const voxelui::WidgetContext& c, voxelui::DrawList& l) {
   l.commands.emplace_back(voxelui::DrawRectangle{c.bounds,{40,160,100,255},{},0,6});
 }
};
ui.components().registerWidget("HostBadge", std::make_shared<Badge>());
```

See examples/mock_host.cpp. Registration must precede parsing. Instances use
standard tree identity and input bubbling; host-managed state should be keyed by
Node::id. There are no native per-instance lifecycle hooks yet. Use registered
slots for dynamic drawings inside vector content; see svg-profile.md.
