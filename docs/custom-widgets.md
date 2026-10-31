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


`Components::unregisterWidget(tag)` disables future measure/draw/event dispatch.
An already expanded instance becomes an inert custom node; explicit styles and
children still participate in layout. A registry revision invalidates intrinsic
measurement. Registering the tag again resumes dispatch for existing nodes. Each
dispatch copies its function table, so self-unregistration cannot destroy the
executing callback. Slots have matching `Runtime::unregisterSlot(name)` support;
registration/removal updates drawing without rebuilding the screen.

Measurement is cached by node and available size. When host-owned widget state
changes intrinsic size, call `ui.invalidateMeasurement(elementId)` on the UI thread.
Draw callbacks run when commands are requested, so pure paint-state changes need
no layout invalidation. Context references are borrowed for the callback only.

Native module unload requires host quiescence: stop routing calls, finish active
UI callbacks, close affected screens or migrate/release host instance state,
unregister widgets/slots/actions/providers/observers/clipboard callbacks and release
all host copies of function tables and shared widget objects **while the module is
still loaded**. Wait for host jobs/destructors and GPU frame resources as applicable,
then unload. Unregister is not a wait/join barrier; a copied in-flight callback can
still execute native code. Draw payloads containing library-owned CPU data survive
screen/runtime destruction, but host-defined deleters/resources may retain module
code too. VoxelUI has no plugin loader and cannot sandbox native C++.
