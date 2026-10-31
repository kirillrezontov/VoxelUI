# Architecture audit — 2026-10-31

Inspected the required sources, public headers, all documentation, CMake/CI,
preview, validator, six examples, software backend and external mock host.
Baseline commit: `e56a25d4cb85c19bcbca253262b0d39aaa5bd8be`.
Findings below are verified by source inspection and targeted tests unless marked
as a hypothesis. Measurements are in [the controlled report](performance-before-after.md).

| Severity | Finding and specific source | Disposition |
|---|---|---|
| High | Every relevant binding and pseudo-state change called `Runtime::rebuild`, including rollback map copies, expansion, styling and full layout (`src/runtime/runtime.cpp`, `src/binding/reconcile.cpp`). | Direct bindings, template aliases and same-order keyed item properties now patch existing nodes. Pseudo states resolve affected styles. Structural/resource changes retain candidate expansion. |
| High | Expansion copied `authored.children` only to clear them, and `return {std::move(out)}` selected the copying initializer-list vector constructor (`Runtime::rebuild`). Pointer event dispatch also copied a complete `LayoutNode` subtree (`src/events/events.cpp`). | Removed these copies. Ordinary C++ allocation counts substantiate the reduction; no custom allocator added. |
| High | Acyclic template fanout had only a depth limit during reference validation (`src/xml/document.cpp`, `parseDocument::references`); repeated DAG traversal could grow exponentially before runtime budgets applied. | Added a total validation visit budget and hostile fanout regression. |
| High | SVG path subdivision bounded individual contours, with the total checked only after the path completed (`src/svg/svg.cpp`, `path`). | Enforced aggregate vertices during generation. Tiny arc radii now use double intermediates before bounded segment conversion. |
| Medium | External text replacement could leave a caret inside a UTF-8 sequence, Select state did not follow external bound values, and U+FFFD was treated as invalid bytes (`src/binding/reconcile.cpp`, `src/text/text.cpp`). | Fixed; both full and incremental paths tested. Full shaping remains absent. |
| Medium | Text cache rounded wrap/alignment width to an integer; nonfinite/huge public numeric inputs could reach float-to-int conversion (`TextSystem::layout`, software framebuffer scale). | Fractional width cache keys, clamping before conversion, finite scale validation and regressions. |
| Medium | Removing a hidden/disabled/captured/dragged node did not consistently clear every interaction ID; shrinking content left the stored scroll offset beyond its effective clamp (`Runtime::rebuild`, `src/events/events.cpp`). | Central validity check and persisted effective scroll offset. Rollback now restores hover/pressed/drag/slot IDs, images and dependency metadata too. |
| Medium | Widgets could register but could not unregister; a callback erasing its own registry entry could destroy the executing callable (`include/voxelui/components.hpp`, layout/events/painting). | Added unregister and registry revisions; copy function tables across dispatch. Host must still quiesce native code before unloading. |
| Medium | Standalone VectorCache keyed only the SVG revision, ignoring nested image revisions; provider exceptions escaped (`src/svg/svg.cpp`, `src/runtime/resources.cpp`). | Resolver epoch participates in cache entries; provider failures become diagnostics, refresh preserves the old resource on exception. |
| Medium, remaining | Structural insertion/removal/reorder and resource changes still expand a validated whole candidate and copy rollback maps. | Keyed IDs/state preserved; per-item value changes are incremental. Region-based structural reconciliation is the next runtime task. |
| Medium, remaining | Measurement caches skip unaffected subtrees, but ancestor aggregation and placement can still visit many siblings (`src/layout/layout.cpp`, `measure`/`place`). | Counts report measurement and placement separately. No assertion that a leaf edit is O(depth) overall. |
| Medium, remaining | DrawList emission remains O(visible nodes), with CPU-oriented glyph masks and flattened vector contours (`src/rendering/draw_list.cpp`, public draw/text/svg headers). | Measured in every pipeline benchmark; see GPU readiness contract. |
| Low | Windows verification text was stale; tracked `.idea` files mixed project source with machine state. | Observed successful baseline CI jobs; IDE files removed from the index, local files preserved. New changes still require Windows CI. |

## Boundaries, direction and ownership

Applications depend on runtime/public v1 headers; the software target depends on
core, and SDL belongs only to preview. Runtime orchestrates XML, CSS, binding,
layout, events, text, SVG and resource modules. Layout uses text/components;
painting uses owning resources. Internal runtime helpers are shared across the
binding/events/rendering translation units, so these directories are conceptual
modules in one library, not separately linkable libraries. No game/GPU dependency
or cyclic target dependency was found.

Authored documents own Node values. Runtime owns a computed LayoutNode tree,
state keyed by ID, persistent binding source descriptions and a private borrowed
node/parent index. Local patches retain tree storage. Structural updates invalidate
borrows. The shared root retains event targets during action-driven replacement.
DrawList owns immutable text/glyph/image/vector payloads through shared pointers;
FreeType faces retain their font bytes. Old command lists survive cache eviction,
replacement and runtime destruction.

Major allocations are Node attribute maps/strings, expanded child vectors,
transactional state/metadata snapshots, text layouts/glyph masks and draw-command
vectors. Dependency metadata adds initial-load storage to remove recurring work.
Standard allocation remains appropriate based on measured copy elimination.

Runtime dispatch is explicit: resource providers, actions, widgets/slots, clipboard
and event observer functions. Built-ins use tags and render commands use variants;
backend/concrete widget adapters use concepts. No virtual hierarchy is needed.

## API and integration risks

The public mock host uses no implementation headers. Mounting, actions, data,
widgets, screen/input/update/draw/diagnostics/destruction are accessible through v1.
`tree`, `find`, `state`, WidgetContext and registry `get` return borrowed views,
not durable handles. Keep IDs or owning draw payloads across calls. LayoutNode is
public and changed size in this phase; all consumers must rebuild. Versioned source
names do not promise binary ABI stability. Expected validation errors preserve the
last good screen; allocation failure and arbitrary widget exceptions have no
strong transactional guarantee.

All runtime, cache, registry and model operations require one host UI thread.
Callbacks are synchronous; async results must be marshalled back. Actions may
replace/close screens. Widget, provider and observer callbacks must defer structural
mutation; runtime destruction/reentrant update during callbacks is unsupported.
Unregistering disables future dispatch, not code already executing or retained by
host-owned function objects. Native modules remain trusted host code.

Hypotheses requiring profiling/extended testing: large unique model-path indexes
and long-lived resource churn may make metadata/cache storage significant;
large SVG zoom may expose fixed-flatness error; native IME and platform-specific
font behavior need manual testing. No heap residency profile, fuzz campaign, native
module loader or GPU execution was performed.
