# Limits and remaining work

This is a working initial implementation, not completion of every ambition in
SPEC.md. The supported subset is documented and tested; the following work remains.

## Highest priority follow-up

1. **Structural reconciliation and placement:** direct bindings, template aliases,
   same-order keyed item properties and pseudo states now update incrementally.
   Measurements reuse clean subtrees and propagate through affected ancestors.
   Insert/remove/reorder and resource changes still construct a full candidate and
   copy rollback metadata. Layout placement and ancestor aggregation remain broad;
   draw-command generation still walks all visible nodes. See the measured counters
   in performance-before-after.md before planning further optimizations.
2. **Text shaping and editing:** FreeType rasterizes English/Russian codepoints.
   Full grapheme segmentation, shaping/kerning, bidi, ligatures, emoji sequences,
   word-aware wrapping and complex scripts are not implemented. Wrapping is by
   codepoint; U+0300–036F combining marks are grouped for caret movement. Input is
   single-line, with keyboard/shift-click selection and basic composition; native
   IME candidate placement, composition-range selection and mouse selection drag
   need further work. Clipboard requires host hooks. Font fallback is the requested
   font → ui:font → visible missing-glyph boxes. Weight ≥600 is synthetic bold.
3. **GPU-ready geometry/batching:** vectors cache flattened contours and the
   software backend scan-converts them. Reusable triangle meshes, scale-dependent
   retessellation, glyph atlases and GPU mesh batching remain future work. Adjacent compatible
   CPU command runs are already batched.
   Vulkan is deliberately external and has not been implemented.
4. **Platform verification:** Linux GCC Debug/Release, headless and SDL smoke tests
   are run locally. Baseline commit `e56a25d` has observed successful Linux/Windows
   pinned-vcpkg and sanitizer jobs in [run 41134694094](https://github.com/kirillrezontov/VoxelUI/actions/runs/41134694094).
   Current uncommitted changes still require Windows CI. The pinned dependency build
   was not repeated locally. SDL real-user IME behavior has not been manually tested.

## Deliberate first-release profile

- Layout has uniform edges, simple rows/columns/equal-column grids/overlays,
  percent/auto/min/max sizes and five anchors. No flex grow/shrink, grid spans,
  separate edge values, constraint solver, scrollbars or collection virtualization.
  Scroll is wheel driven and clipped; keyboard focus does not automatically scroll
  an offscreen item into view yet. Conflicting constraints favor min; percentage
  cycles use available parent size instead of iterating.
- CSS is a subset (see style-language.md). No browser layout, selectors with
  combinators, CSS functions except simple var, media queries or rich text.
  Opacity is a simplified inherited value, not isolated-group compositing.
- SVG uses even-odd fills, endpoint linear gradients and rectangular clip paths.
  Transformed clip bounds are approximate. No filters, masks, SVG text, style
  attributes, use, dash/join policies, multistop/radial gradients or rounded SVG
  rects. Unsupported safe elements warn; forbidden executable/external content
  errors. The software renderer has no vector antialiasing.
- Raster assets are PNG only. Font and PNG safety depends in part on updated host
  libraries. No complete sandbox for hostile native extensions is claimed.
- Controls include a cycling Select, not a popup dropdown. Drag-and-drop emits
  source/target events; host code decides how to change application data.
  Custom widget instance lifecycle hooks are not yet provided. Host state must
  invalidate cached intrinsic measurement explicitly when it changes. Unregistering
  a factory disables future dispatch but requires host quiescence before native unload.
- Hot reload keeps the last valid screen/assets. Initial load failures return
  diagnostics; the host chooses a recovery screen. Missing initial images display a diagnostic checkerboard; the validator still
  rejects their missing resources. Disappearing active images preserve the prior screen. In-memory screens set overrides before load; resource
  screens support reload and later overrides.
- Validator checks syntax, component rules, styles and resources. Mock schema
  validation covers live expanded data/actions; no standalone schema file format
  or exhaustive validation of every possible empty-collection item type exists.
  Semantic diagnostics have column 1; libxml2 syntax columns are more precise.
- All runtime/model/provider operations belong on one UI thread. Host async work
  must marshal results back. Event/widget callbacks should defer structural changes;
  action callbacks can close or replace screens. Runtime destruction during its
  own callback is unsupported. Native host code is trusted.

## Safety budgets

Defaults: 4 MiB documents/import graphs; 64 nesting levels; 10,000 authored or
generated UI nodes and template reference visits; 16 MiB expanded text/attribute payload; 256-byte authored IDs;
64 attributes/node; 100,000 vertices/SVG; resources 16 MiB each,
4096 requested IDs and 128 MiB cached bytes. Runtime image cache: 128 MiB decoded;
vector scene cache: one million vertices and 128 MiB raster payloads. PNG decodes
are at most 8192/axis and 16 million pixels. The software framebuffer has the same
pixel/axis limits. Text input/layout is capped at 64 KiB, font raster size 1–256;
glyph and layout caches have entry/geometry budgets. Hosts should also bound model
sizes before handing native data to the runtime.

No parser fuzz campaign or long-duration application soak test has been run.
See implementation-progress.md for exact executed validation.
