# VoxelUI

Standalone C++23 declarative UI engine: **XML structure, CSS-inspired styles,
SVG assets, host-controlled data/actions, graphics-independent draw commands**.
The core has no VoxelCraft, Vulkan, SDL, browser or scripting dependency.

A working initial engine, six examples, tools and tests are implemented. **Not every
requirement in SPEC.md is finished.** The exact supported profile and remaining
work are recorded below and in [implementation progress](docs/implementation-progress.md).

Experimental [native Vulkan primitive backend](backends/vulkan/README.md): host-owned
context, reusable buffers and hardware reference tests. Text/images/SVG and game
integration remain unfinished.

## Build and run

Dependencies: CMake ≥3.25, C++23 compiler, Ninja (presets), libxml2 ≥2.9,
FreeType ≥2.10, libpng ≥1.6. SDL3 is optional for interactive preview. Normal
builds use installed system libraries and do not download anything.

```sh
cmake --preset dev
cmake --build --preset dev --parallel 4
ctest --preset dev
build/voxelui-preview examples/main_menu/main.xml
```

Headless preview, validator and independent mock host:

```sh
build/voxelui-preview examples/settings/main.xml --headless settings.png
build/voxelui-preview examples/svg_gallery/main.xml --headless gallery.png --debug
build/voxelui-validate examples/world_selection/main.xml --mock-schema
build/voxelui-host examples
```

Interactive controls: mouse/wheel, Tab/Shift-Tab focus, Enter/Space activation,
UTF-8 text entry, Ctrl+A/C/X/V, arrows/Home/End, F3 bounds/IDs, F5 reload, +/- UI
scale. XML/CSS/SVG/font/PNG changes are detected every 400 ms. Failed reloads
retain the last good screen; the console and window title report errors.
`--width`, `--height`, `--scale`, `--root` and `--frames` are available; without
SDL3, use `--headless output.png`.

Other configurations:

```sh
cmake --preset asan
cmake --build --preset asan --parallel 2
ctest --preset asan
cmake --preset release
cmake --build --preset release --parallel 2
build-release/voxelui-bench examples
cmake --install build --prefix /your/install/prefix
```

Set `VOXELUI_PREVIEW_SDL=OFF` for a core/headless build. Installed consumers use
`find_package(VoxelUI CONFIG REQUIRED)` and targets `VoxelUI::voxelui` and
optionally `VoxelUI::software`. Public interfaces live in `voxelui::v1`.

`voxelui/profiling.hpp` provides an optional UI-thread phase observer for
bindings, rebuild, style, layout, animation, event processing and DrawList
generation. `profiling::Observe` restores a previous sink on scope exit. With no
sink, the hooks do not read a clock. Callbacks must not throw. Times are inclusive:
DrawList generation flushes pending runtime work and events may recurse. Hosts
using an older 0.2.1 package without this additive header can still render, but
cannot collect these internal phase timings.

For a single-config Linux generator, configure and install the **same** build
type. `--config Debug` does not select compiler flags for Ninja/Makefiles.
Test each installed configuration with a separate prefix and consumer directory:

```sh
for config in Debug Release; do
  cmake -S . -B "out/linux-$config" -G Ninja -DCMAKE_BUILD_TYPE="$config" -DVOXELUI_PREVIEW_SDL=OFF
  cmake --build "out/linux-$config" --parallel 3
  ctest --test-dir "out/linux-$config" --output-on-failure
  cmake --install "out/linux-$config" --config "$config" --prefix "$PWD/out/stage-$config"
  cmake -S tests/consumer -B "out/consumer-$config" -G Ninja -DCMAKE_BUILD_TYPE="$config" \
    -DCMAKE_PREFIX_PATH="$PWD/out/stage-$config" -DVOXELUI_EXPECT_IMPORTED_CONFIG="$config"
  cmake --build "out/consumer-$config" --parallel 3
  ctest --test-dir "out/consumer-$config" --output-on-failure
done
```

The consumer requires `find_package(VoxelUI 0.2.1 EXACT CONFIG REQUIRED)`,
checks both imported targets and their configuration-specific library paths,
then executes the public API contract. Installing an unconfigured build with
`--config Debug` skips its `VoxelUITargets-noconfig.cmake` export; this reproduces
the missing `IMPORTED_LOCATION` CI failure. Linux CI now tests Debug and Release
with matched build/install/consumer configurations.

For Windows, set VCPKG_ROOT to a bootstrapped vcpkg checkout and configure with
`cmake --preset windows`, then `cmake --build build-windows`. The manifest pins
registry baseline `d186940b2f0c4e8e5b4ff804af57d2a26fbfa091`; add
`-DVCPKG_MANIFEST_FEATURES=preview` for SDL3. Linux/Windows CI is included.
Baseline [GitHub Actions run 41134694094](https://github.com/kirillrezontov/VoxelUI/actions/runs/41134694094)
passed Linux/Windows pinned-vcpkg and sanitizer jobs. Current local changes still
require Windows CI; the pinned dependency build was not repeated locally.

## Implemented features

| Area | Working implementation |
|---|---|
| XML | Version 1 tree, validation, source diagnostics, bounded parsing, templates/components, custom registered tags, DTD/entity rejection |
| Styles/themes | Compound selectors, specificity/order, five states, inheritance, variables/imports/cycles, inline overrides, theme overrides, color/opacity transitions |
| Layout | Rows/columns, equal-column grid, overlays, anchors, intrinsic/percent/min/max sizing, clipping, scrolling, DPI/UI scale, cached measurement and placement with ancestor invalidation |
| Widgets/input | Buttons, toggles, checkboxes, sliders, single-line text input, cycling select, progress, tooltip/modal primitives, capture, focus, drag/drop events |
| Text | UTF-8 Latin/Cyrillic, FreeType glyph/layout caches, fallback, wrapping/alignment, caret/selection, clipboard hooks, basic IME composition, synthetic bold |
| Data/actions | Typed observable values, path bindings, explicit writes, keyed repeat state, path/props/item patches and nested keyed permutations, typed action schemas/capabilities/errors |
| SVG | Shapes, M/L/H/V/C/S/Q/T/A/Z paths, transforms, fill/stroke/opacity, endpoint linear gradients, rectangular clips, resolver PNG images, cached geometry, transformed dynamic slots |
| Resources/reload | Namespaced priority providers, memory/directory mounts, traversal protection, revisions/budgets, font/image reload, last-good recovery, missing-image checkerboard |
| Rendering | Owning typed draw lists, ordered clip/transform stacks, clipped-leaf culling, straight-alpha targets, compatible batch runs, deterministic software renderer, PNG output |
| Tools/integration | SDL3/headless preview, schema-aware validator, public-header mock host/custom widget, CMake install package, benchmarks |

Examples: [main menu](examples/main_menu/main.xml), [settings](examples/settings/main.xml),
[inventory](examples/inventory/main.xml), [world selection](examples/world_selection/main.xml),
[HUD](examples/hud/main.xml), [SVG gallery](examples/svg_gallery/main.xml).
Mock data and host actions belong to the examples, not the core.

## Verification and continuation

The integration executable contains **67 named test cases**, including all six
examples, interaction/lifecycle/security cases, and fresh-runtime comparisons of
geometry, styles and software pixels after incremental changes at multiple scales.
Debug, Release and ASan+UBSan checks, six byte-identical before/after example PNGs,
SDL smoke tests and a separately configured installed-package host are recorded in
[the execution log](docs/implementation-progress.md). Controlled before/after timings
and allocation/work counts are in [the performance report](docs/performance-before-after.md).
The 0.2.1 continuation adds installed-package consumer coverage and records current
[measurements](docs/performance-before-after.md#continuation--2026-11-02).
Architecture findings: [audit](docs/audit.md). No production-readiness claim is made.

Remaining work, in recommended order:

- [ ] Region-based insertion/removal reconciliation; direct/template/item edits and nested same-key permutations reuse subtrees, and placement skips unchanged branches. Insert/remove and repeat fanout still build a candidate tree.
- [ ] Full Unicode grapheme/shaping/bidi support, word-aware wrapping, mouse selection drag and native IME candidate placement.
- [ ] Scale-dependent SVG triangle meshes, GPU glyph atlases and backend mesh batching; general transformed clip geometry.
- [ ] Scrollbars, focus auto-scroll, virtualized lists, popup select, widget lifecycle hooks and exhaustive external schema tooling.
- [ ] Run current changes on Windows CI and broaden fuzz/soak and native IME testing.

These are explicit gaps, not APIs claimed to work. Detailed layout, SVG, text,
resource and platform limits are in [limitations](docs/limitations.md). SVG filters,
full browser/CSS/SVG compliance, JavaScript, a mod loader and a required Vulkan
renderer remain outside the documented first-release profile or project scope.

## Documentation

Start with [Build a custom game menu without writing C++](docs/tutorial.md).
Reference: [architecture](docs/architecture.md), [XML](docs/xml-format.md),
[styles](docs/style-language.md), [SVG/slots](docs/svg-profile.md),
[data binding](docs/data-binding.md), [actions](docs/action-system.md),
[custom widgets](docs/custom-widgets.md), [resources/themes](docs/resource-system.md),
[host integration](docs/host-integration.md), [render contract](docs/render-backend.md),
[GPU integration readiness](docs/gpu-integration-readiness.md).

VoxelUI source is MIT licensed. The bundled font and system dependencies retain
separate licenses: [third-party notices](docs/third-party.md),
[font license](examples/assets/FONT-LICENSE.txt).

Experimental native backend work starts with an optional, bounded CPU geometry
compiler for square rectangles, borders, clips and transforms. It is not yet a
Vulkan renderer; see [coverage and reproduction](backends/vulkan/README.md).
