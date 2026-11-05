# Native backend geometry foundation

This directory contains **CPU geometry preparation, not a Vulkan renderer**.
Enable `VOXELUI_VULKAN_GEOMETRY=ON` to build the private
`voxelui_vulkan_geometry` target. It is intentionally not installed/exported as
`VoxelUI::vulkan`; core and existing packages remain independent of Vulkan.

`compileGeometry` consumes an owning DrawList and returns an owning indexed mesh
or a command-indexed error. It implements square rectangles, disjoint borders,
ordered alpha colors, affine transform stacks, nested rectangular clips and DPI
scaling (the reference's 0.25–8 clamp). UI scale is already in the DrawList.
Coordinates are framebuffer pixels. The convex quad clipping algorithm permits
fan triangulation **only of clipped convex rectangles**, never SVG contours.
All triangles use one future solid-color pipeline, retaining painter order.

Rounded rectangles, images, text and vectors fail explicitly, even when hidden
by a clip. Stack underflow/unbalanced stacks, invalid numeric parameters,
overflows and exceeded command/stack/mesh budgets also fail without exposing
partial output. Zero-area rectangles/clips are empty. Negative rectangle sizes
are empty, as in the software rasterizer. Rotated clips retain v1's transformed
axis-aligned bounding box semantics; exact rotated clipping is not implemented.

The caller supplies trusted budgets. Defaults bound work to 100,000 commands,
64 levels per stack, one million vertices and three million indices. These
limits apply before mesh growth. Native host code is trusted; this does not
sandbox native mods. No pointer identity cache or borrowed resource is retained.
The result may be destroyed after the host copies its bytes into upload storage;
that upload storage and destination GPU buffer must survive submission completion.

Tests compare CPU triangle coverage/blending against SoftwareRenderer at three
viewports and five DPI scales, including translucent overlap, borders, nested
clips, rotations, reflections, singular transforms, and budget/error cases.
Axis-aligned cases require exact pixels. Transformed differences are accepted
only within 0.001 framebuffer pixel of a triangle edge, because forward triangle
coverage and inverse reference mapping round exact boundaries differently.
These tests do not establish Vulkan conformance or GPU lifetime correctness.

From the workspace parent:

```sh
cmake -S VoxelUI -B VoxelUI/out/native-geometry -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVOXELUI_PREVIEW_SDL=OFF \
  -DVOXELUI_VULKAN_GEOMETRY=ON
cmake --build VoxelUI/out/native-geometry --parallel 3
ctest --test-dir VoxelUI/out/native-geometry --output-on-failure --no-tests=error
```

Next executable slice: add optional host-context Vulkan command recording with
persistent buffers and a solid-color pipeline consuming this mesh, culling
disabled (reflections reverse winding), and straight-alpha blending into a
specified color format. Device, rendering scope, command buffer and completion
remain host-owned. Add GPU readback comparisons before connecting the game.
Implement rounded primitives, cached SVG tessellation and glyph/image atlases
before selecting native rendering for real menus. Do not silently fall back per
unsupported command or claim full-frame software uploads have been removed.
