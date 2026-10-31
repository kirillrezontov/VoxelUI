# Performance before/after — 2026-10-31

## Environment and methodology

EndeavourOS (Arch-family Linux), x86_64, kernel 7.2.11-arch1-1; AMD Ryzen 7 2700
(8 cores/16 threads). GCC 16.2.1 20260810, CMake 4.4.4, Ninja 1.13.2, Release
(`-O3 -DNDEBUG`), system libxml2 2.15.4, FreeType 2.15.0, libpng 1.6.59.
SDL3 3.4.18 is optional preview-only. No vcpkg dependencies or new libraries were
installed for this phase.

Initial pre-edit checks: `cmake --preset dev`, dev build/CTest (38 cases, 3.87 s),
Release build/CTest (38 cases, 0.61 s), asan preset build/CTest (38 cases, 30.01 s),
all passed. The original benchmark re-run measured XML 1427.79 µs, initial runtime
9087.99 µs, full layout 340.99 µs, unchanged update 0.03 µs, unrelated update
0.20 µs, all-bound label update 7192.20 µs, 200-key rotation 1445.51 µs and draw
emission 419.30 µs. This initial single unpinned run establishes a local baseline;
it is separate from both historical repository numbers and the controlled table.
All six CLI example validations and 960×640 reference PNGs were generated before
runtime changes.

For the controlled comparison, the expanded benchmark was compiled against the
original archived `e56a25d4cb85c19bcbca253262b0d39aaa5bd8be` library and against
the modified library with the same compiler, flags, dependencies, assets and source:

```sh
mkdir -p /tmp/voxelui-audit-baseline
git archive e56a25d4cb85c19bcbca253262b0d39aaa5bd8be | tar -x -C /tmp/voxelui-audit-baseline
cp benchmarks/bench.cpp /tmp/voxelui-audit-baseline/benchmarks/bench.cpp
cmake -S /tmp/voxelui-audit-baseline -B /tmp/voxelui-audit-baseline/out -G Ninja -DCMAKE_BUILD_TYPE=Release -DVOXELUI_PREVIEW_SDL=OFF
cmake --build /tmp/voxelui-audit-baseline/out --parallel 4
cmake --preset release
cmake --build --preset release --parallel 4
for run in 1 2 3 4 5; do
  taskset -c 2 /tmp/voxelui-audit-baseline/out/voxelui-bench examples > "before-$run.txt"
  taskset -c 2 build-release/voxelui-bench examples > "after-$run.txt"
done
```

The benchmark source SHA-256 is
`3553ac7ceb97db27d6baf8f24a859ab27cce10bad4d8af3c514c769bc0c320e0`.
The baseline's CMake does not define the new optional stats-print macro; timed
operations and allocation instrumentation are otherwise identical. Five paired
runs were alternated on CPU 2 after builds/tests completed. Each row times its
stated loop count (20–10,000 original operations, 100 per pipeline); no per-row
warmup is excluded. CPU clocks/other system activity were not controlled, so these
are local medians/ranges, not latency guarantees or statistical confidence bounds.
Tiny update-only numbers approach loop/timer resolution.

Pipeline rows include mutation/input, update and owning DrawList emission. The
1000-labelled localized scene actually has 1004 nodes; its 999 text elements are
mostly static. The original bound-label row changes all 1000 labels. Lists copy
200 model items because the public model has whole-array replacement semantics.
The mixed screen has 608 nodes, SVG, nested layout, controls and a clipped list.
Insert/remove rows replace prior variants; the first one-item row restores the
base topology after removal, and subsequent iterations patch one item. All ten
runs produced the same observable checksum, `3603150`.

Allocation figures count ordinary C++ new/new[] calls, excluding aligned/C-library
allocations and residency; see [benchmark notes](performance.md). There is no custom
allocator. Ratios divide comparable median timings. Full raw outputs:
[before 1](performance-runs/before-1.txt), [2](performance-runs/before-2.txt),
[3](performance-runs/before-3.txt), [4](performance-runs/before-4.txt), [5](performance-runs/before-5.txt);
[after 1](performance-runs/after-1.txt), [2](performance-runs/after-2.txt),
[3](performance-runs/after-3.txt), [4](performance-runs/after-4.txt), [5](performance-runs/after-5.txt).

## Controlled results

Microseconds per operation; ranges are the minimum and maximum of five run means.

| Scenario | Before median [range] | After median [range] | Before/after | C++ allocations before → after |
|---|---:|---:|---:|---:|
| XML parse + authored tree (1000) | 1419.95 [1398.62–1533.71] | 1408.64 [1404.08–1412.28] | 1.01× | 2018.55 → 2018.55 |
| Style resolution (one node) | 0.89 [0.87–0.93] | 0.89 [0.86–0.89] | 1.00× | 7.00 → 7.00 |
| Runtime tree/style/initial layout | 8963.67 [8482.81–9407.98] | 7900.56 [7751.22–7984.46] | 1.13× | 40964.20 → 22975.00 |
| Full layout (1000 nodes) | 337.20 [334.32–345.73] | 347.77 [343.86–351.54] | 0.97× | 2002.80 → 2002.80 |
| Unchanged frame update | 0.01 [0.01–0.02] | 0.02 [0.02–0.02] | 0.50× | 0.00 → 0.00 |
| Unrelated model update | 0.21 [0.21–0.31] | 0.27 [0.26–0.27] | 0.78× | 0.00 → 0.00 |
| Bound label update + layout | 7783.75 [7156.71–7917.26] | 3238.77 [3101.95–3362.42] | 2.40× | 31102.80 → 8112.80 |
| Keyed collection rotate (200) | 1444.95 [1427.71–1542.55] | 1123.69 [1117.36–1147.26] | 1.29× | 5658.05 → 3870.00 |
| Text layout uncached | 16.45 [16.23–16.74] | 16.64 [16.52–16.77] | 0.99× | 18.60 → 18.60 |
| Text layout cached | 0.11 [0.11–0.11] | 0.12 [0.12–0.12] | 0.92× | 1.00 → 1.00 |
| SVG parse + flatten | 19.22 [19.15–19.68] | 19.55 [19.14–20.37] | 0.98× | 61.00 → 61.00 |
| SVG cached geometry access | 0.02 [0.02–0.02] | 0.02 [0.02–0.02] | 1.00× | 0.00 → 0.00 |
| Cached vector command emission | 0.01 [0.01–0.01] | 0.01 [0.01–0.01] | 1.00× | 0.00 → 0.00 |
| Draw list (1000 text nodes) | 413.19 [402.31–423.91] | 428.61 [426.67–443.49] | 0.96× | 26.11 → 26.11 |
| Pipeline unchanged (1000) | 393.69 [378.67–404.34] | 407.93 [401.46–414.75] | 0.97× | 26.29 → 26.29 |
| Pipeline unrelated (1000) | 392.85 [383.84–400.35] | 405.02 [398.14–420.44] | 0.97× | 26.01 → 26.01 |
| Pipeline one text (1000) | 6598.68 [6233.96–6855.63] | 496.90 [488.39–501.36] | 13.28× | 14031.66 → 37.02 |
| Pipeline progress (1000) | 6857.41 [6449.95–7029.08] | 410.52 [404.93–417.27] | 16.70× | 14173.00 → 33.00 |
| Pipeline pseudo state (1000) | 6961.44 [6634.48–7029.48] | 424.05 [416.64–432.89] | 16.42× | 14686.00 → 41.96 |
| Pipeline text resize (1000) | 6851.43 [6470.48–6957.52] | 501.02 [484.86–502.87] | 13.67× | 14183.84 → 42.83 |
| Pipeline class change (1000) | 6796.76 [6458.92–6878.55] | 511.60 [499.89–521.65] | 13.29× | 14047.25 → 54.59 |
| Pipeline keyed rotate (200) | 1563.34 [1504.26–1567.79] | 1201.31 [1186.23–1216.37] | 1.30× | 5680.09 → 3892.09 |
| Pipeline keyed insert (200) | 1260.39 [1229.83–1292.01] | 1219.13 [1201.20–1225.51] | 1.03× | 5126.05 → 3931.03 |
| Pipeline keyed remove (200) | 1248.11 [1211.22–1259.87] | 1192.04 [1174.43–1225.78] | 1.05× | 5066.08 → 3878.14 |
| Pipeline one list item (200) | 1257.16 [1216.70–1262.96] | 241.88 [240.54–242.91] | 5.20× | 5083.38 → 661.01 |
| Pipeline mixed SVG/list screen | 3752.55 [3608.54–3801.38] | 295.76 [291.27–307.78] | 12.69× | 14670.74 → 432.74 |

## Work counters and interpretation

Counter means from after-1 (all runs have the same workload/counts):

| Pipeline | Bindings | Patched | Styled | Measured | Placed | Full candidates |
|---|---:|---:|---:|---:|---:|---:|
| Pipeline unchanged (1000) | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| Pipeline unrelated (1000) | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| Pipeline one text (1000) | 0.99 | 0.99 | 0.00 | 3.96 | 993.96 | 0.00 |
| Pipeline progress (1000) | 1.00 | 1.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| Pipeline pseudo state (1000) | 0.00 | 0.00 | 1.99 | 0.00 | 0.00 | 0.00 |
| Pipeline text resize (1000) | 1.00 | 1.00 | 0.00 | 4.00 | 1004.00 | 0.00 |
| Pipeline class change (1000) | 0.99 | 0.99 | 0.99 | 3.96 | 993.96 | 0.00 |
| Pipeline keyed rotate (200) | 200.00 | 0.00 | 202.00 | 202.00 | 202.00 | 1.00 |
| Pipeline keyed insert (200) | 201.00 | 0.00 | 203.00 | 203.00 | 203.00 | 1.00 |
| Pipeline keyed remove (200) | 199.00 | 0.00 | 201.00 | 201.00 | 201.00 | 1.00 |
| Pipeline one list item (200) | 200.00 | 0.99 | 2.02 | 4.99 | 202.00 | 0.01 |
| Pipeline mixed SVG/list screen | 1.00 | 1.00 | 0.00 | 4.00 | 608.00 | 0.00 |

Fractional counts include the initial unchanged sample (0.99) and first structural
restoration in the item row (0.01). A steady localized label change measures four
nodes while conservatively placing 1004; progress and the tested color pseudo
state measure/place zero. A mixed title update measures four and places 608.
This is measurement reuse with broad placement, not an O(depth) layout claim.

Local-update gains are not shifted into command generation: the pipeline includes
that work and unchanged/draw-only rows remain reported. Draw emission is now the
largest part of static-heavy local updates. Structural rotate/insert/remove still
rebuild candidates and copy rollback metadata; their improvements are smaller.
The forced full-layout microbenchmark has cache bookkeeping overhead and is
slightly slower; unchanged/unrelated update-only overhead also rose by fractions
of a microsecond. These tradeoffs remain explicit instead of being hidden behind
only favorable cases. Text cache and SVG parse differences are not claimed as
significant speedups. Initial runtime construction now allocates substantially
less despite retaining additional dependency metadata.

## Correctness and platform verification

Final commands executed:

- Debug build + `ctest --preset dev`: 57 named cases in one executable, passed,
  4.93 s.
- Release build + `ctest --test-dir build-release --output-on-failure`: 57 cases,
  passed, 0.70 s (CTest total 0.71 s).
- ASan+UBSan build + `ctest --preset asan`: 57 cases, passed, 34.65 s.
- All six example documents: CLI `--mock-schema` passed; headless previews ran.
  The SVG gallery retains its documented unsupported-filter warning.
- Fresh-runtime oracle: full node/style/geometry and RGBA equality for incremental
  states at 320×240/480×320, UI scales 1/1.5 and DPI scales 1/1.25. Keyed structural
  state/pixels, nested image revisions, clipping, action replacement, external
  widget unregistration/measurement and resource lifetime regressions passed.
- Fixed 64×64 primitive golden hash stayed `12769330923178300131`.
- Main menu/settings SDL previews ran with `--frames 2`, exit 0. This is a smoke
  test, not manual IME verification.
- Mock host and separately configured installed-package consumer ran, exit 0.
  The latter registers/unregisters widgets/actions/providers, updates bindings,
  invalidates measurement by ID, and renders owning output after runtime destruction.
- Each public header independently passed `c++ -std=c++23 -I include -fsyntax-only`.
- Six 960×640 before/after PNG files compared byte-identical; their SHA-256 hashes:

| Example | SHA-256 (both versions) |
|---|---|
| hud | `36960ae8bf8ec66547aee3f8fc9e495bf7d90c3c08568c40792dd183074fa10d` |
| inventory | `6b588bd3c4bf3dd3e9843b3c0fc844f8ad1c58d12523fd6fba7786ea21a6e73b` |
| main_menu | `8206532015fae317d6790119db4f5dcde3d27facb648e188b8e1847c46ecb249` |
| settings | `841c682b99f016d4b282ad9bc9d8321bc7b98bd96abaf290c46b0ed470dff8aa` |
| svg_gallery | `d3113e9472b33587a5efa1074130b68a95aa1cd4114650b2d2195f99b8ee52fe` |
| world_selection | `e9e0d1e88150ef257ea3d5d38a5eddda2134ccef320af43f719830f7979801ac` |

The generated PNGs are local artifacts under `/tmp/voxelui-reference/{before,after}`;
raw benchmark runs are retained in this repository. No screenshot baseline was
regenerated to conceal a changed result.

Observed [GitHub Actions run 41134694094](https://github.com/kirillrezontov/VoxelUI/actions/runs/41134694094)
passed Linux/Windows pinned-vcpkg and sanitizer jobs on baseline `e56a25d`. Current
changes are locally verified on Linux only; no new Windows run or local vcpkg build
was performed. No GPU adapter, fuzz campaign, soak test or manual native IME test
was executed. See [audit](audit.md) and [remaining limits](limitations.md).
