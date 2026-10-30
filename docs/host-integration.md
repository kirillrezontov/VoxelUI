# Host integration (inline namespace voxelui::v1)

```cpp
#include <voxelui/runtime.hpp>
voxelui::Runtime ui;
ui.resources().mountDirectory("app", "assets");
// Register actions, custom widget tags and initial data before load.
ui.data().set("player.name", "Мария");
if (!ui.load("app:menu.xml")) { /* inspect ui.diagnostics() */ }
ui.processEvent(platformTranslatedEvent); // true means the UI owns this input
ui.update({.deltaSeconds=dt,.uiScale=1,.dpiScale=dpi,.viewport={width,height}});
auto commands=ui.buildDrawList();
backend.render(commands);
```

Return consumed input to the game only when processEvent returns false. Pointer
targeting uses reverse paint order, then bubbles to ancestors through observeEvents.
Builtin controls claim pointer/keyboard input; disabled and modal surfaces block
input behind them. Pointer capture retains drag/slider targets until release.
Tab and Shift-Tab navigate enabled controls within the top visible modal. Text
entry consumes UTF-8 input/composition events. Provide clipboard read/write hooks.
RoutedEvent carries owned IDs; setting handled stops bubbling. Drag events carry
the source ID in text on drop; the host decides whether/how a model changes.

Keep a runtime on one thread. Node/tree/state pointers are borrowed until update,
load, reload or close; retain IDs, never pointers, across frames. DrawList resources
are owning and outlive the runtime. Actions may load/close screens safely; widget
and event observers should defer structural mutations until dispatch returns.
Destroying the runtime during one of its callbacks is unsupported.

The core target depends privately on libxml2, FreeType and PNG, and never on SDL,
Vulkan or an engine. Link VoxelUI::voxelui and optionally VoxelUI::software after
find_package(VoxelUI CONFIG REQUIRED), or add_subdirectory. See mock_host.cpp.
