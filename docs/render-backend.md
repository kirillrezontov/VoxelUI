# Rendering backend contract

DrawList owns an ordered variant sequence: DrawRectangle (radius and border are
fields), DrawImage, DrawText, DrawVectorGeometry, Push/PopClip, Push/PopTransform.
All resource payloads use shared ownership of immutable CPU images, glyph bitmaps,
text layouts and vector geometry. A list remains usable after screen replacement,
cache eviction and runtime destruction. No backend object lives in public resources.

Process commands in order with nested transform and clip stacks. Units are logical;
framebufferScale multiplies coordinates for a physical framebuffer. Runtime UI
scale is a root transform and also changes the available logical layout space.
Alpha blending is source-over. Only merge adjacent compatible commands; never
reorder across clips, transforms or overlapping translucent elements. Runtime finalization creates adjacent compatible batch runs in DrawList::batches.
Rectangles share a run; image/text/vector commands share a run when their owning
payload matches. Clips/transforms are barriers. Standalone producers call
rebuildBatches after editing commands. GPU atlas/mesh batching remains adapter work.

`RenderBackend` is a concept requiring `backend.render(const DrawList&)`.
SoftwareRenderer provides deterministic RGBA output and savePng. It renders rounded
rectangles, borders, glyph masks, nearest-neighbor images, scanline vector fills,
strokes and linear gradients. No subpixel antialiasing. Clip transforms use axis
aligned bounds; general transformed clips are approximate. Buffers are bounded to
8192 per dimension and 16 million pixels.

## Future Vulkan adapter

Consume draw output only; never parse XML/CSS/SVG. Upload image/glyph payloads to
texture/atlas handles, cache flattened vector geometry in GPU buffers, maintain
ordered vertex/index batches, apply premultiplied conversion if used by the blend
pipeline, map nested logical clips to framebuffer scissor rectangles, and retain
payloads until GPU completion. Respect alpha, stacking and UI/DPI scales. Transforms
can be baked into vertices or supplied as push constants. Core needs no Vulkan changes.
