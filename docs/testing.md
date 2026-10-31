# Tests and validation

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

The single CTest executable contains individually named integration cases; it uses
explicit checks even in Release (not assertions disabled by NDEBUG). Cases cover
XML diagnostics/budgets/entities/templates, CSS cascade/variables/import cycles,
layout modes/constraints/scaling/scroll/clips, SVG paths/arcs/transforms/cache,
bindings/reconciliation, typed actions/capabilities, UTF-8 editing/clipboard,
modal focus/drag capture, reload, custom widgets, slot coordinate conversion,
resource ownership, fonts and example rendering.

The software screenshot regression renders a fixed 64×64 command sequence and
checks both selected pixels and a complete RGBA FNV-1a hash:
12769330923178300131. It contains clips, alpha blending, transform stacks and
rounded rectangles and has no font/PNG-version dependency. Each example also
renders headlessly at 960×640 and 1280×720. These are render/size checks, not
pixel-golden checks for every example. Glyph screenshots may change with FreeType.

`voxelui-validate file.xml` states explicitly that no host schema was checked.
`--mock-schema` checks bundled example data/actions; real hosts validate through
Runtime with their own registrations. Parsing well-formed XML alone is insufficient.

SDL smoke: `build/voxelui-preview examples/main_menu/main.xml --frames 2` opens,
renders two frames and closes. This does not claim manual testing of every native
input method. See implementation-progress.md for actual results/platforms.


The current suite has 57 named cases. New regressions verify local node storage
and measurement reuse, dependency ancestors/descendants, nested template aliases,
same-order keyed properties, keyed structural edits/selection/focus, removal of
hovered/captured nodes, scroll shrink, style/hit-test changes, transactional errors
and repair, updated action parameters with screen replacement, factory self-removal,
public host measurement invalidation, nested resource revisions and draw lifetime.
Security cases bound acyclic template validation and aggregate SVG subdivision,
reject oversized incremental payloads and handle tiny arcs/provider exceptions.

The incremental oracle constructs a fresh runtime at each target state and compares
all node attributes, computed styles, bounds, clips, content sizes and rendered RGBA.
It varies 320×240/480×320 viewports, UI scales 1/1.5 and DPI scales 1/1.25, using
English/Russian text, templates, repeats, percentage sizing, anchors, SVG and scroll
clipping. This supplements the fixed primitive golden; no expected pixels were
blindly regenerated. Six example PNGs also matched the baseline byte for byte on
the recorded local library versions. See performance-before-after.md for hashes.
