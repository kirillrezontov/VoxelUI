# Implementation progress and handoff

The repository began with only SPEC.md, AGENTS.md and IDE metadata. This working
snapshot implements the initial runtime across milestones A–E. It does **not**
claim every requirement in the specification is complete. README and
limitations.md identify remaining work and distinguish supported profiles.

## Milestones delivered

| Milestone | Implemented and exercised |
|---|---|
| A — Core | C++23/CMake, versioned public headers, bounded libxml2 parser/DOM, templates, components, CSS cascade, flex-like/stack/grid/anchor layout, typed draw output, headless tests |
| B — Rendering | Deterministic software rasterizer/PNG, FreeType text/cache, PNG images, clipping/hit testing, optional SDL3 interactive and headless preview |
| C — Behavior | Focus/modal routing/capture, controls, UTF-8 text/caret/selection/clipboard/composition, typed observable bindings, keyed repeats, typed/capability actions, scroll and drag/drop foundations |
| D — Customization | Limited SVG profile/path flattening/cache, PNG references, transformed dynamic slots with input capture, themes/overrides/transitions, priority providers, transactional XML/CSS/SVG/font/image reload, validator |
| E — Local release preparation | Six examples, custom mock host, benchmark suite/baseline, sanitizers, installed CMake package smoke, documentation/tutorial, Linux/Windows CI and pinned vcpkg baseline |

E remains externally unverified for Windows and vcpkg; full shaping, granular
incremental processing and GPU mesh/atlas work remain unfinished. No milestone
is being certified as complete against every item of the original specification.

## Environment and executed checks

Date: 2026-10-31. Linux x86_64, AMD Ryzen 7 2700, GCC 16.2.1, CMake/Ninja.
System dependencies: libxml2 2.15.4, FreeType 2.15.0, libpng 1.6.59, SDL3 3.4.18.
The local runs used system packages, not the pinned vcpkg manifest.

- `cmake --preset dev`, `cmake --build --preset dev --parallel 4`: passed.
- `ctest --preset dev`: 38 named cases in one CTest executable, passed (4.05 s).
- `cmake --build --preset release --parallel 2` and Release CTest: all 38 cases passed (0.63 s).
- `cmake --build --preset asan --parallel 2`, `ctest --preset asan`: all 38 cases passed with AddressSanitizer and UndefinedBehaviorSanitizer (28.22 s).
- Earlier 26/32/37-case runs were superseded by the final 38-case checks.
- Final benchmark baseline was collected after the builds/tests finished.
- Every example passed CLI `--mock-schema` validation and generated a 960×640 PNG.
  Tests additionally render all six at 960×640 and 1280×720.
- Main menu, settings, inventory, world list and SVG gallery screenshots were
  visually inspected. Gallery deliberately emits an unsupported-filter warning.
- SDL3 main-menu and settings preview windows ran with `--frames 2`, exit 0.
  This is a window/render smoke check, not a claim of manual IME testing.
- Mock host ran, invoked its typed action and generated mock-host.png.
- A separate project under /tmp installed/imported VoxelUI through find_package,
  linked both exported targets, rendered a frame and exited 0.
- CLI validator returned failure for an initially missing image even though the
  runtime can display its checkerboard fallback.
- Reference 64×64 screenshot hash: 12769330923178300131.
- Benchmark output and exact methodology: performance.md / performance-baseline.txt.

No Windows runner, Vulkan implementation, vcpkg dependency build, fuzz campaign,
long-running soak or manual real-user IME/candidate-window test was executed.

## Resume here

1. Read README's remaining-work checklist and docs/limitations.md.
2. Configure/build dev and run ctest before changing behavior.
3. Start with `src/binding/reconcile.cpp`, `src/layout/layout.cpp` and the
   relevant-model-update benchmark to add subtree invalidation/allocation reuse.
4. Text expansion is isolated in src/text; establish Unicode segmentation/shaping
   dependencies and add English/Russian regression cases before extending editing.
5. Backend work belongs outside parsing/layout: extend cached vector geometry or
   consume DrawList through the documented Vulkan adapter contract.
6. Observe Windows CI and pinned vcpkg results; fix platform issues before claiming
   those paths verified. Keep progress and README synchronized after each feature.

The unrelated IDE metadata edit in .idea/editor.xml was left untouched.
