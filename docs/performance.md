# Performance

Run an optimized, non-sanitized benchmark from the repository root:

```sh
cmake --preset release
cmake --build --preset release --parallel 2
build-release/voxelui-bench examples
```

Each row reports wall-clock microseconds per operation and iteration count. Scenes
include 1000 lightweight text elements and a 200-key collection. XML parsing includes
authored tree allocation. Runtime setup includes expansion, styling, font validation
and initial layout. The standalone full-layout test reuses the expanded tree.
Unrelated updates and unchanged frames measure invalidation overhead. The bound-label
update measures current whole-tree reconciliation cost. Cached vector emission is
CPU draw-command creation, not GPU or software pixel rendering. Text uses the bundled
Noto Sans font. Results are local observations, not latency guarantees.

The final local baseline below was collected on 2026-10-31, Linux x86_64,
AMD Ryzen 7 2700, GCC 16.2.1, CMake Release, using system libraries. It is a
single unpinned run after build/test completion; tiny timings approach loop/timer
resolution and should not be treated as precision guarantees. No background threads
are used. Current priorities are subtree invalidation, allocation reuse and triangle
mesh/atlas batching; measure before optimizing dispatch boundaries.

```text
XML parse + authored tree (1000)  1567.49 us/op (20)
Style resolution (one node)       0.88 us/op (5000)
Runtime tree/style/initial layout 9454.86 us/op (10)
Full layout (1000 nodes)          343.53 us/op (20)
Unchanged frame update            0.02 us/op (500)
Unrelated model update            0.20 us/op (500)
Bound label update + layout       7849.21 us/op (20)
Keyed collection rotate (200)     1505.66 us/op (20)
Text layout uncached              16.84 us/op (100)
Text layout cached                0.11 us/op (5000)
SVG parse + flatten               19.77 us/op (100)
SVG cached geometry access        0.02 us/op (10000)
Cached vector command emission    0.01 us/op (10000)
Draw list (1000 text nodes)       417.75 us/op (100)
Observable checksum: 1117450
```

This baseline shows that whole-tree bound updates are substantially more expensive
than an isolated layout pass. Granular reconciliation and allocation reuse are the
next runtime optimization targets. Raw output: [performance-baseline.txt](performance-baseline.txt).
