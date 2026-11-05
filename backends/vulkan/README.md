# Experimental host-context Vulkan backend

Enable `VOXELUI_VULKAN=ON` for `voxelui_vulkan`, or
`VOXELUI_VULKAN_GEOMETRY=ON` for CPU preparation alone. Neither experimental target
is installed/exported yet. Core and existing packages do not depend on Vulkan.

Implemented: ordered square/rounded rectangles, disjoint square borders,
fragment-evaluated rounded borders, straight-alpha blending, affine transforms,
nested rectangular clips and DPI scale (reference clamp 0.25–8). Reflections work
with culling disabled. Rounded coverage uses inverse local coordinates at fragment
centers, matching the software reference without an arbitrary polygon fan.
Only convex clipped rectangles are fan triangulated. Rotation of clip rectangles
retains the v1 reference's axis-aligned bounding-box semantics.

**Images, glyphs and SVG still fail explicitly with a command index. This is not
ready to replace the game's full-screen software bridge.** No glyph/image atlas,
SVG cache, sampled-image descriptors, native game adapter or complete DrawList
conformance is implemented. Geometry is owning and transactional on failure;
rounded meshes require the supplied fragment shader, not a plain triangle oracle.

## Host ownership and lifetime

The host supplies an existing Vulkan 1.3 physical device, logical device with
dynamic rendering enabled, color format and completion fences. No instance,
device, queue, window or swapchain is created by the backend. The pipeline uses
single-sample dynamic rendering with one color attachment and no depth attachment.
Conformance currently covers `R8G8B8A8_UNORM`, with opaque destination alpha (UI
composed over the world). Transparent offscreen targets contain premultiplied
composite RGB; do not treat them as the software renderer's straight-alpha image.
Color-space conversion on sRGB targets has not been validated.

For each host frame slot:

1. Complete its previous submission. The fence must be signaled.
2. `upload(slot, mesh)` copies into persistent coherent mapped vertex/index
   buffers. Capacity grows geometrically and is reused; replacement destruction
   is safe because the slot's fence was checked. Upload never waits globally.
3. Inside the host rendering scope call `record(commandBuffer, slot, extent)`.
   It changes pipeline, viewport, scissor, push constants and vertex/index state;
   restore host state before subsequent host draws if needed.
4. Reset the fence immediately before submission, submit with that same fence.
   Do not upload again between recording and completion. Handle failed submissions
   in the host; an unsignaled fence is not evidence of completion.
5. Complete all recorded/submitted usage before destroying Renderer, its fences
   or the device. Destruction does not wait on or own the host's lifecycle.

Each slot owns its buffers; there is no shared staging allocation or queue-idle
upload. The host's normal fence completion is the retirement boundary.
`statistics()` exposes allocation count and resident buffer capacity. Trusted host
mesh uploads are bounded and indices/numeric values checked. DrawLists retain the
100,000 command / 64 stack / one-million vertex / three-million index limits.
Shaders are compiled at build time by glslc and embedded in the library. Their
SPIR-V targets Vulkan 1.1 to avoid requiring optional shader-demote features; the
renderer still requires Vulkan 1.3 dynamic rendering.

## Focused verification

```sh
cmake -S VoxelUI -B VoxelUI/out/native-gpu -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVOXELUI_PREVIEW_SDL=OFF -DVOXELUI_VULKAN=ON
cmake --build VoxelUI/out/native-gpu --parallel 3
ctest --test-dir VoxelUI/out/native-gpu --output-on-failure
```

The graphics test requires hardware Vulkan 1.3, Khronos validation and sync
validation; it fails rather than silently substituting software Vulkan. Configure
`VK_LAYER_PATH` if your validation manifest is not registered system-wide.
It performs 72 offscreen readbacks across three viewports/three scales, opaque and
translucent overlap, square/rounded borders, reflection, nested clips, empty
meshes, buffer growth/reuse and rejection of incomplete slots. Channel tolerance
is two UNORM levels for fixed-function blending; coverage mismatches are not
blanket-exempted. The CPU suite retains its 33 software comparisons/error cases.
This is primitive conformance, not text/SVG/image or application conformance.

Ordinary CI builds this backend and runs CPU checks. The separate manual hardware
workflow runs the graphics test on the checked-out revision. Next: bounded
sampled-image/glyph atlases, cached SVG tessellation, then host application adapter
and the full UI resource lifecycle/benchmark gate.
