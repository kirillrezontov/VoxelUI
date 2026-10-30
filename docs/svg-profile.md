# SVG profile v1

Supported: svg, g, rect, circle, ellipse, line, polyline, polygon, path; positive
viewBox or numeric width/height; translate/scale/rotate/matrix transforms;
presentation fill/stroke, opacity/fill-opacity/stroke-opacity; two endpoint
object-bounding-box linear gradients; rectangular clipPath; resolver-backed
PNG image elements with namespaced href. Native UI rectangles/text do not use SVG.

Paths: M/L/H/V/C/S/Q/T/A/Z, absolute and relative, repeated arguments, exponent
notation. Cubic/quadratic curves subdivide to 0.25 SVG user-unit flatness, depth
12 maximum. Arcs/circles use bounded adaptive segments, maximum 4096. Geometry
budget is 100,000 vertices. Cached geometry is retained by resource revision;
static frames do not reparse or flatten. Large zoom can expose flattening error;
scale-dependent retessellation and triangle mesh caching remain future work.

Software fill uses even-odd scanlines for every contour. Nonzero winding,
stroke dash/join/cap policies, radial/multistop gradients, CSS style attributes,
SVG text, masks, filters, use and animations are outside this profile. Safe
unsupported elements produce warnings and are skipped. script, foreignObject,
use, event handler attributes, external entities, network/data/file hrefs and
missing resolver assets are errors. Unsupported filter definitions are explicitly
reported; filters do not affect rendering. Rect rounding in SVG is not yet applied.
Transformed clips use axis-aligned bounds; rotated rectangular clips are approximate.

## Dynamic slots

`<g id="badge" data-voxelui-slot="host.badge" data-x="0" data-y="0"
data-width="100" data-height="40" transform="translate(20 30)"/>`.

Register a WidgetDefinition using Runtime::registerSlot. Bounds are in local SVG
user coordinates; absent slot sizes default to the SVG viewBox size. The draw
list applies aspect-preserving viewBox fitting then the slot transform, and clips
to the Vector content bounds and slot rectangle. Slot drawing occurs above static
SVG geometry. Event coordinates are inverse transformed into slot-local space;
slots are tested in reverse definition order. A consumed pointer-down captures
the Vector/slot until pointer-up. Rotated clip bounds have the software limitation
above. Slots do not mutate authored SVG or create a native game object.
