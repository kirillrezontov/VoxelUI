# Performance

Use an optimized, non-sanitized build from the repository root:

```sh
cmake --preset release
cmake --build --preset release --parallel 4
build-release/voxelui-bench examples
```

Each row reports wall-clock microseconds per operation, iterations and ordinary
C++ allocations per operation. Pipeline rows include model mutation or input,
update and owning DrawList generation. The static-heavy scene has 1004 total nodes
(999 Text elements, controls and containers); the keyed scene has 200 items; the
mixed scene combines SVG, text, buttons, nested containers and scrolling.

The original all-bound-label scenario changes all 1000 text elements together;
it is separate from changing one bound label among static siblings. Full layout
uses the public forced-computation default. Cached SVG emission measures command
creation, not software/GPU rendering. Text uses the bundled font. Pipeline counters
report binding evaluations, patched/styled/measured/placed nodes and full candidate
rebuilds per operation. Measurements can be far smaller than placements; do not
interpret the former as the total layout cost.

The benchmark overrides ordinary new/delete in its own executable. Counts exclude
aligned allocations, C-library allocation (libxml2/FreeType/libpng) and allocation
sizes/residency. They are evidence of C++ allocation traffic, not a heap profile.
Iterations include initial cache misses and model/collection copies. Insert/remove
rows replace the prior variant with another inserted/removed key; this includes
removal/insertion of the previous variant. The first one-item update restores the
base collection after the removal row; subsequent iterations patch one item.

See [performance-before-after.md](performance-before-after.md) for controlled
five-run medians, full methodology, allocation counts, work counters and executed
checks. [performance-baseline.txt](performance-baseline.txt) preserves the original
historical measurements. Those historical figures are not the controlled baseline.

Remaining costs are structural candidate expansion/rollback, broad placement and
ancestor aggregation, repeated Value collection copies, and full visible DrawList
emission. No custom allocator or shifted deferred reconciliation was introduced.
