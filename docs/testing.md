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
