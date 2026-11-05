# Host-context Vulkan renderer

Build with `VOXELUI_VULKAN=ON`. The optional installed target is `VoxelUI::vulkan`;
core and software-only packages retain no Vulkan dependency. Shader compilation
requires glslc; installed consumers need Vulkan headers/loader, not glslc.
`VOXELUI_VULKAN_GEOMETRY=ON` builds only CPU preparation for specialized hosts.

## Command coverage

All eight DrawList variants are handled:

- Rectangles, rounded rectangles and borders use ordered triangles and local-space
  fragment coverage; straight-alpha blending preserves painter order.
- Images use nearest sampling of immutable RGBA images packed into a shared atlas.
- Text uses the existing positioned glyph masks, including Latin/Cyrillic. Selection,
  caret and composition rectangles remain ordinary ordered commands.
- SVG fills use cached even-odd trapezoid tessellation for concave contours, holes
  and intersections. No arbitrary polygon fan is used. Flattened contours are
  cached by owning immutable document identity; scale changes do not retessellate.
  Gradients evaluate in framebuffer space as in the software reference. Strokes
  evaluate segment distance, including the reference's overlapping segment blending.
  Embedded images, shape clips and fitted SVG transforms are supported.
- Transform stacks and nested rectangular clips preserve the v1 reference's
  transformed axis-aligned clip bounds, including rotated clips. This does not
  implement a different exact rotated-clip profile.

One pipeline, indexed draw and shared atlas preserve source command order without
sorting translucent geometry. Static SVG tessellations and glyph masks are reused.
No full-screen software rasterization, per-glyph VkImage or queue-idle upload occurs.

## Bounds and differences

Atlas pages grow from 256 to 2048 square RGBA texels (16 MiB maximum per GPU frame
slot). At capacity, the compiler evicts/rebuilds from the current DrawList; repacks
are bounded and failure is explicit if a frame cannot fit. Individual images and
masks must fit 2048 square. The compiler retains at most 16,384 atlas identities
and 32 vector documents. SVG decomposition limits each shape to 2,048 nonhorizontal
edges, 8,192 band/intersection ordinates, and each document to 250,000 triangles.
The default command/stack/mesh budgets still apply. Unsupported sizes, malformed
numbers and budget exhaustion return a command-indexed diagnostic, never silently
omit a command or rasterize a full-frame fallback. Null resources are empty as in
the reference. These are backend bounds, not additional XML capabilities.

Nearest sampling and hard rounded/stroke coverage match the current reference.
GPU fixed-function blending can differ by up to two UNORM levels in tested fixtures.
UNORM conformance covers translucent composition over an opaque target. SRGB
attachments decode UI RGB before hardware encoding; opaque color/coverage is
compared. Translucent sRGB blending is linear-light and intentionally differs from
the software reference's byte-space blending. Offscreen transparent attachments
contain premultiplied composite RGB; do not interpret that as SoftwareRenderer's
straight-alpha export. Edge classification can differ at subpixel boundaries;
the tests do not blanket-ignore coverage failures.

## Ownership and recording contract

The host owns the Vulkan 1.3 instance/device (dynamicRendering enabled), queue,
swapchain, color format, command buffer, submissions and completion fences. The
backend creates only UI buffers, atlas images/views, sampler, descriptors and
pipeline objects. No second device, queue submission or window is created.

1. Complete the slot's prior submission; its fence must remain signaled.
2. Compile the owning DrawList with a persistent `Compiler`, then `upload(slot,mesh)`.
   Vertex/index/staging buffers reuse coherent mapped allocations. All replacements
   and descriptor mutations occur only after the slot completion check.
3. Begin host commands and call `recordUploads(command,slot)` **outside rendering**.
   This records transfer/layout barriers for a changed atlas. Unchanged pages upload
   nothing. A published CPU page is immutable; cache edits use copy-on-write.
4. Begin single-sample dynamic rendering with one color attachment and no depth
   attachment, then `record(command,slot,extent)`. Restore any host graphics state
   needed for later draws: this changes pipeline, descriptors, viewport/scissor,
   vertex/index bindings and push constants.
5. End/submit those commands, resetting the same slot fence immediately before
   submission. Do not rerecord/discard a recorded atlas transfer or mutate the slot
   before completion. On abandoned/failed submission discard/recreate the renderer
   after host quiescence; a recorded transition is not proof of GPU execution.
6. Complete all submitted usage before destruction, and destroy Renderer before
   its device/fences. The backend does not wait globally on host teardown.

Per-slot resources are retired at the fence boundary. `statistics()` reports
allocations, buffer/atlas bytes and upload bytes; `LifetimeObserver` reports object
acquisition/release including atlas pages, descriptors, pipeline objects and staging.
The observer context must outlive Renderer and must not throw.

## Focused verification

```sh
cmake -S VoxelUI -B VoxelUI/out/native-gpu -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVOXELUI_PREVIEW_SDL=OFF -DVOXELUI_VULKAN=ON
cmake --build VoxelUI/out/native-gpu --parallel 3
ctest --test-dir VoxelUI/out/native-gpu --output-on-failure
```

The graphics suite requires hardware Vulkan 1.3 and Khronos API/synchronization
validation, with no software-device substitution. Set VK_LAYER_PATH for a local
manifest when needed. It performs 144 readbacks across three viewports and scales:
72 UNORM fixtures with text, images, SVG holes/gradients/strokes and primitives;
72 SRGB opaque primitive fixtures. It checks empty updates, cache reuse, atlas growth
and repack, resource reload identities, fence rejection, and zero observer counts
after destruction. `--context-only` isolates driver initialization from UI resources.
`VOXELUI_GPU_CAPTURE=/path/output.ppm` writes a real GPU fixture readback.

The normal CI compile job tests its checkout; manual hardware CI runs the GPU
suite on a maintained Vulkan runner. Debug/Release installed consumers verify
exact package version and the optional imported target when requested.
