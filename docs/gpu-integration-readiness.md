# GPU integration readiness

An independent Vulkan adapter can be developed against the existing v1 DrawList.
There is no Vulkan implementation or dependency in core. This is an experimental
source contract; rebuild consumers after public layout changes. It does not imply
production readiness or stable binary ABI.

1. **Consumable now.** DrawRectangle, DrawImage, DrawText and DrawVectorGeometry
   are ordered variants with owning immutable CPU payloads. Push/PopClip and
   Push/PopTransform preserve nested state. DrawBatch describes adjacent compatible
   runs and stack barriers, not a required GPU draw-call count. Adapters need no XML,
   CSS, binding or gameplay access.
2. **CPU-oriented structures.** TextLayout contains positioned shared glyph alpha
   bitmaps, not glyph IDs or atlas UVs. VectorDocument contains flattened contours,
   stroke data, endpoint gradients, optional image quads and rectangular clips,
   not indexed triangles. Rectangle borders/radii are analytic parameters. The
   reference renderer scan-converts these and blends straight-alpha source-over
   into byte-valued RGBA. Color-space conversion is not standardized as linear-light
   compositing; agree the adapter's output/color-space policy before visual approval.
3. **Reusable meshes.** Cache fill triangulation and stroke meshes by immutable
   geometry identity and tolerance/scale bucket. Respect the even-odd fill profile,
   shape order, gradients, transformed raster images and clips. Fixed 0.25 SVG-unit
   flattening is cached today; large zoom needs scale-aware retessellation, with
   resource/scale/tolerance in the cache key. GPU tessellation is an adapter/future
   geometry-stage choice, not per-frame XML/SVG parsing. Keep the CPU renderer usable.
4. **Glyph atlases.** Upload each immutable GlyphBitmap alpha mask once into atlas
   pages, and emit quads using PositionedGlyph offsets. CPU masks are independent
   of FreeType face lifetime. There is no public font/glyph/revision key: shared
   bitmap identity is sufficient for a prototype cache if ownership is retained;
   an explicit immutable glyph key would improve durable cache accounting. Atlas
   allocation, page eviction, upload staging and fallbacks belong to the adapter.
   Repack only when outstanding commands no longer reference old UVs/pages.
5. **Textures/descriptors.** Cache uploads by owning Image/geometry/glyph identity.
   A resource replacement creates a new immutable payload. Never key solely on a
   raw pointer after releasing ownership: allocator address reuse is possible.
   Retain shared or weak ownership and validate identity; retire stale descriptors,
   views, images and meshes together. Stable resource IDs/revisions in future draw
   payload metadata would help host-wide caches without embedding Vulkan handles.
6. **Frame completion.** Retain DrawList payloads, uploads, atlas pages, buffers and
   descriptor allocations until the submitting frame's fence/timeline value has
   completed. Runtime close/reload/unregister and cache eviction do not imply GPU
   completion. Use adapter-owned deferred destruction. CPU shared ownership alone
   cannot protect an already released VkImage or a reused atlas region.
7. **Clips, transforms, scale and batching.** LogicalSize is the viewport before DPI;
   framebufferScale converts it to physical pixels. UI scale is already a root
   transform and changes layout's available size: do not apply it twice. Intersect
   nested clips after transforms, round/clamp framebuffer scissors and preserve
   empty clips. Current rotated clips use axis-aligned bounds; exact rotated clips
   need stencil/mask support and a versioned semantic change. Preserve source-over
   order for translucency. Merge only adjacent commands with compatible pipelines,
   textures, clips and transforms; never globally sort by material. A DrawBatch of
   text with shared layout is only a hint; atlas-page batching is adapter work.
8. **Before engine integration.** Freeze coordinate/clip/color/alpha semantics with
   adapter conformance images; define cache budgets, frame ownership and device-loss
   recovery; add stable payload keys if shared host caches require them; validate
   descriptor/atlas retirement with several frames in flight. Compare adapter output
   against software at multiple UI/DPI scales and with nested SVG/image/clips. Keep
   render submission separate from UI-thread screen/input lifecycle and keep native
   module unloading under host quiescence control. VoxelCraft integration must not
   depend on borrowed LayoutNode pointers or FreeType/Vulkan internals in core APIs.

The recommended next core task is structural region reconciliation with the same
fresh-runtime geometry/pixel oracle. A prototype external adapter can proceed in
parallel as host work once its rendering semantics and lifetime tests are agreed.
