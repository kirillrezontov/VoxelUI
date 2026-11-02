# Architecture (API version 1, library 0.1)

Dependencies flow from applications → software backend → runtime → document/style/layout/text/vector/resource modules. The core target has no SDL or Vulkan dependency. Libxml2, FreeType and libpng are private implementation dependencies. SDL3 is optional and belongs only to the preview executable.

Authored Node trees retain XML attributes and locations. Runtime expansion produces a separate hierarchical tree; stable string identities address interaction state. Layout produces boxes and clips. Rendering produces ordered, owning variant commands. Backends never parse documents. Resource providers, actions and widget functions are the explicit runtime dispatch boundaries (std::function); render adapters use a concept. No virtual base classes or script execution.

Resources use namespace:path identifiers, provider priority, bounded reads, revisions, and shared ownership. Hot reload builds a candidate before swapping. The runtime is single threaded; host callbacks run synchronously and should enqueue long running work in the host.

See implementation-progress.md for verified status and limitations.md for gaps.

Implementation files now separate interaction (`src/events`), runtime lifecycle
(`src/runtime`), binding expansion (`src/binding`), painting (`src/rendering`),
layout, CSS, XML, SVG and text. The two public link targets are VoxelUI::voxelui
and VoxelUI::software. Internal helpers have no exported ABI. API v1 is explicit
but this 0.1 foundation has not yet made a long-term ABI compatibility promise.

## Incremental invalidation

`src/binding/incremental.cpp` owns persistent dependency metadata and the private
ID → node/parent index. A source is a model path plus property/index steps;
Component props alias the original source, including nested item/template scopes.
No metadata retains pointers into a DataModel Value. Exact, ancestor and descendant
model changes select dependent targets through an ordered index. Key topology is
checked before item patches: insertion/removal or changed keys take the candidate path;
supported same-key permutations stage source remapping and subtree moves.

The invalidation categories are separate operations, rather than one frame bit:

- **Binding/value:** evaluate only affected sources; compare typed values, then
  stage changed Node attributes/text, action parameters and control state.
- **Structure:** mount/reload, resource-source changes and repeat insertion/removal
  construct a validated candidate. Supported same-key permutations move existing
  subtrees after patch validation. Keyed IDs preserve compatible state; removed
  IDs and changed types clear invalid state.
- **Style:** class/inline style, visibility/disabled/anchor and pseudo-state changes
  resolve the affected node. Changes to inherited color/font/size/weight/alignment,
  variables or opacity propagate to descendants. Selectors have no combinators.
- **Measurement:** changed text, intrinsic inputs or geometry style mark the node
  and all ancestors. A cached result is reusable only when its measurement input
  size is unchanged. Unaffected siblings restore their measured size before
  placement; stretched dimensions are not mistaken for intrinsic measurements.
- **Layout:** size/viewport/UI-scale changes, measurement changes and scrolling
  run layout. Measurement and placement skip clean subtrees when their inputs
  match; ancestors still aggregate siblings. Placement refreshes dependent positions
  and clips and clamps stored scroll offsets, including after removal. Percentage and auto sizing retain
  the documented externally available-size rules, without a new constraint solver.
- **Paint:** a value such as progress changes commands without measurement/layout.
  DrawList always emits a fresh owning snapshot; there is no separate retained
  paint-command cache. DPI changes affect framebuffer scale, not logical measure.
- **Resource:** resolver epochs invalidate resource caches and use validated
  replacement. Old immutable draw payloads remain owned by existing lists.

Patch validation is transactional for expected type/action/style/budget errors.
Invalid changes retain the previous tree and pending model paths; repaired data
retries them on a later revision. No per-frame retries or repeated diagnostics are
performed for an unchanged rejected model. Structural rollback includes metadata,
interaction IDs and image/vector snapshots. Allocation failure and arbitrary native
widget exceptions are outside the strong rollback guarantee.

`RuntimeStats` exposes cumulative rebuild/evaluation/patch/style/measurement/
placement counters. `LayoutStats::nodes` counts measurements in the last layout
pass; `placed` counts placements. These are work counters, not allocation counters.
The benchmark separately counts ordinary C++ allocations. No custom allocator was
introduced. Public `computeLayout` still performs full measurement by default;
its optional incremental argument requires the caller to maintain dirty flags.

Widget measurement must depend on Node/available size, or the host must call
`invalidateMeasurement(id)` after changing external intrinsic state. Registration
revisions invalidate custom measurements. Per-instance external state is host-owned
and keyed by ID; lifecycle callbacks and automatic state migration are future work.


## Keyed permutations and cached placement (0.2.1)

A repeat with unchanged unique keys and one computed root per item can permute
existing children. Validate every key, locate its contiguous sibling region, stage
positional binding-source remapping (including nested repeats), then evaluate and
validate value/action/style patches. Invalid updates restore metadata and leave
order, geometry and interaction state untouched. Commit nested permutations before
moving their ancestors; repoint the existing private borrowed index entries once, without map-node allocation. Public identities
and ID-keyed state survive. Item-root addresses may move; borrowed pointers remain
invalid across updates. Insert/remove, resource changes and fanout use the validated
candidate path. No general structural-region reconciler is claimed.

Measurement invalidation marks both measurement and placement dirty through all
ancestors. Measurement still aggregates immediate children and checks available
sizes. Placement can reuse a subtree only when its path is clean, its final bounds
(including stretch/grid sizing) match, and its incoming clip matches. A moved or
resized parent therefore invalidates dependent child geometry and clips naturally;
scroll changes explicitly mark the container-to-root placement path. Ancestors
still scan siblings to aggregate flow/justify/grid positions, but unchanged sibling
subtrees are not descended into. Public incremental computeLayout callers must
maintain ancestor measurement/placement flags, including for scroll offsets; the
default full computation remains the correctness oracle.


## Conservative leaf paint culling

Painting receives the actual accumulated draw clip in logical coordinates, starting
at the viewport divided by UI scale. Style clip commands narrow this input exactly
as emitted. An empty intersection with a built-in leaf's bounds skips its bounded
primitives and text-layout lookups. This is recomputed from current layout every
emission, so scrolling, resize and local layout changes cannot retain a stale cull.
An offscreen parent with overflow-visible descendants is still traversed. Custom
widgets and Vector geometry/slots are excluded because their drawing can extend
outside their layout box. RuntimeStats reports paint visits, culled leaves and
emitted command counts separately. Traversal remains proportional to the tree;
this is not list virtualization. Debug outlines/tooltips retain their existing path.


Clipped containers now use their content rectangle for paint clips, matching layout
and descendant hit testing. The earlier outer-box paint clip allowed a scrolled child
to draw into padding where it could not be hit. A pixel regression covers this fix
at UI scale 1.5/DPI 1.25; all six static reference examples remain unchanged.
