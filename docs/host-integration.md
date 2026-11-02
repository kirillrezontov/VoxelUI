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


Public additions in this phase: `Runtime::runtimeStats()`,
`Runtime::invalidateMeasurement(id)`, `Runtime::unregisterSlot(name)`,
`Components::unregisterWidget(tag)`/`revision()`, and optional diagnostic output
for `Resources::refresh(&diagnostics)`. Existing ordinary source calls remain valid.
`LayoutNode`/`LayoutStats` gained cache/counter fields and `computeLayout` gained
an optional incremental argument. Consumers must rebuild: v1 identifies source
names, not a stable binary ABI. Auto identities now use authored ordinals, so adding
an earlier repeated item does not renumber unrelated anonymous nodes. Persisted
host state should continue to use explicit IDs and repeat keys.

Expected validation errors return diagnostics and retain the previous valid UI.
Actions and resource-provider exceptions are converted to diagnostics/errors.
Arbitrary widget/observer exceptions and allocation failure do not have a strong
transactional guarantee. Providers must not reenter or mutate their resolver while
loading. No concurrent or reentrant update/destruction is supported.

A separately configured install/import smoke consumer is part of this phase's
manual verification. See [audit](audit.md), [measurements](performance-before-after.md)
and [GPU readiness](gpu-integration-readiness.md) for contract limits and next work.


0.2.1 also exposes const model access and makes the runtime's existing noncopyable,
nonmovable ownership explicit. Keep it in stable storage (a unique_ptr is suitable).
Commands and geometry value types support equality; immutable payloads compare by
shared identity. Software rendering optionally accepts a transparent clear color
for host surface composition. Rebuild all consumers after the LayoutNode cache
layout change; no binary ABI compatibility is promised. The install-only
`tests/consumer` project exercises registration, measurement invalidation, action
screen closure, unregister and command ownership against exported targets. Both
Linux/Windows CI jobs now configure and run that consumer after installation.
