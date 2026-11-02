# Implementation progress and handoff

## Incremental runtime and host continuation — 2026-11-02

Source commits `716ddf4` and `c7633ca` extend the existing 57-case implementation:

- Same-key collection permutations move existing subtrees, remap nested and shared
  collection binding sources transactionally, and reuse ID-index storage.
- Placement caches skip clean sibling subtrees with unchanged geometry and clip;
  ancestor measurement/placement still scans dependent siblings for correctness.
- Paint traversal omits built-in leaves outside the actual clip stack. Overflowing
  descendants, vectors and custom drawing remain conservative. Padded containers
  now paint within the same content clip used by descendant hit testing.
- Focus loss clears preedit/drag/capture, disabled pointer regions do not intercept
  keyboard input, self-unregistering observers retain their executing closure, and
  local Select edits keep intrinsic geometry across resource rebuilds. Preserved
  editor/Select values count toward the expanded-payload budget.
- Owning DrawList value equality and transparent software targets support an
  independent host surface bridge. The package consumer exercises public-only
  widget, action, unregister and resource-lifetime contracts. Runtime's existing
  ownership restrictions are explicit; public structure changes require rebuilds.

All 66 named cases passed after `c7633ca`: Debug 6.19 s, Release 1.01 s and
leak-enabled GCC ASan/UBSan 38.73 s. Six example validators and reference screenshots,
the primitive golden hash and installed-package consumer passed in this phase.
The six reference PNGs were byte-identical after the clip/culling changes; a separate
scaled padding regression verifies the deliberate clipping correction.

The controlled five-pair benchmark endpoint is `716ddf4`, before the Select-only
reload follow-up. Complete update-to-command medians on the same machine were
487.26 → 65.82 µs for one text in a clipped 1000-element column, 1205.53 → 289.49 µs
for a keyed rotation, and 287.51 → 83.71 µs for the mixed SVG/list scene. A fully
visible 1000-label grid was 460.61 → 431.28 µs. The large clipped-column gain is
primarily omitted command emission; traversal remains O(tree). Exact methodology,
allocations, counters, regressions and raw runs are in
[performance-before-after.md](performance-before-after.md).

VoxelCraft's separate local `main` consumes the public source/package targets and
the existing Vulkan device through a software-surface reference bridge. Its menu,
connection/status/error screens and subsequent gameplay migration are host work;
the standalone library acquires no game, SDL or Vulkan dependency. This bridge is
not a native GPU DrawList backend. See that repository's
`docs/vulkan-ui-integration.md` for its current scope and executed checks.

Remaining work: regional insertion/removal reconciliation, ancestor aggregation,
collection Value copies, full visible command generation, reusable GPU payload
identifiers/adapters, shaping/Unicode segmentation, extended native IME and fuzz/soak
testing. Current-change Windows execution is deferred, not claimed as passing.

## Architecture/performance phase — 2026-10-31

The existing engine was audited and improved in place. [Audit](audit.md) records
verified defects and remaining risks; [architecture](architecture.md) describes
invalidation and ownership. The original 38-case engine remains the baseline at
`e56a25d4cb85c19bcbca253262b0d39aaa5bd8be`; this phase adds incremental binding
updates and measurement reuse without changing the standalone project vision.

| Stage | Delivered | Remaining |
|---|---|---|
| A — Audit/baseline | Required sources/docs/tools inspected; Debug/Release/ASan+UBSan baseline, example validation and six reference PNGs; expanded benchmark compiled against original code | No heap residency profiler or fuzz campaign |
| B — Runtime | Persistent path/props/item dependencies, transactional in-place value patches, local style/pseudo updates, private ID/parent index, valid keyed state and complete interaction-ID rollback | Repeat topology/resource changes still use full validated candidate expansion |
| C — Layout/allocation | Cached measurements keyed by available size, ancestor invalidation, paint-only progress, scroll-offset clamp; removed recursive Node/initializer-list/event-target copies; indexed animation updates | Broad placement/ancestor aggregation, rollback map copies and full draw emission |
| D — Integration | Public widget/slot unregistration, safe copied dispatch, explicit host measurement invalidation, public-header installed consumer, GPU readiness contract | Native instance lifecycle hooks, stable GPU payload keys and adapter conformance tests |
| E — Reliability | Fresh-runtime geometry/style/pixel oracle; UTF-8/caret/Select fixes, SVG aggregate/path/cache robustness, template fanout budget, provider exceptions, numeric validation; 57 named cases | Full shaping/segmentation/IME, fuzz/soak, current-change Windows run |

No Vulkan, VoxelCraft, scripting runtime, plugin loader or new dependency was added.
No stable binary ABI or production-readiness claim is made. Public structure sizes
changed and consumers must rebuild; ordinary source call sites remain compatible.

## Verification

Environment: Linux x86_64, AMD Ryzen 7 2700, GCC 16.2.1, CMake/Ninja,
libxml2 2.15.4, FreeType 2.15.0, libpng 1.6.59, optional SDL3 3.4.18.
The current phase uses system packages. Exact final timings and raw benchmark
runs are in [performance-before-after.md](performance-before-after.md).

Baseline checks executed before runtime changes: Debug 38 cases (3.87 s), Release
38 cases (0.61 s), ASan+UBSan 38 cases (30.01 s), all passing. All six examples
passed CLI mock-schema validation and produced 960×640 reference PNGs.

Final 57-case checks passed: Debug 4.93 s, Release 0.70 s (CTest total 0.71 s),
ASan+UBSan 34.65 s. Current checks include these expanded-suite runs,
all-example validation/headless rendering, main-menu/settings SDL `--frames 2`,
mock host, and a separately configured installed-package host linking only the
exported targets. Six before/after example PNG files were byte-identical. The
font-independent primitive reference hash remains `12769330923178300131`.
The fresh-runtime oracle compares complete geometry, styles and software pixels
for dynamic template/repeat/SVG/clip scenes over multiple viewport/UI/DPI scales.

Observed baseline GitHub Actions [run 41134694094](https://github.com/kirillrezontov/VoxelUI/actions/runs/41134694094):
Linux pinned-vcpkg, Windows pinned-vcpkg and sanitizer jobs all succeeded for
`e56a25d`. This corrects the prior unobserved-Windows documentation. Current local
changes have not run on Windows, and vcpkg was not rebuilt locally. SDL checks
are window/render smoke tests, not manual native IME/candidate-window testing.

Tracked `.idea` files were removed from the index using `git rm --cached`;
local IDE files, including the pre-existing editor.xml modification, were preserved
and its hash was checked. The existing .gitignore already excludes .idea.
Repository settings/default branch were not changed.

## Continue here

1. Replace full structural candidate expansion with region/key-aware reconciliation;
   retain transactional validation, stable IDs and the fresh-runtime oracle.
2. Optimize conservative placement and draw-command storage using the separate work
   counters and complete update-to-command pipeline measurements.
3. Define adapter conformance images and frame/resource retirement before wiring a
   Vulkan backend. Keep it outside core; see gpu-integration-readiness.md.
4. Add Unicode shaping/segmentation through isolated, justified dependencies before
   extending complex text editing. Keep English/Russian regressions.
5. Run current changes through Windows CI and broader fuzz/soak/native IME tests.
