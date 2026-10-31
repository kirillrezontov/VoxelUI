# Resources and themes

IDs are namespace:relative/path; allowed characters are ASCII letters, digits,
underscore, hyphen, dot and slash. IDs cannot contain empty/dot/dot-dot path
segments, absolute paths, backslash, query strings or another colon. They are
independent of filesystem location. Directory providers canonicalize paths and
reject symlink escapes. Load callbacks return optional owned byte vectors, so
external ZIP/pack/memory/network-policy providers need no inheritance.

Mount with an integer priority. Higher wins; later mounts win ties. Mount/unmount
clears resolver caches and advances an epoch. get caches immutable Resource
snapshots with monotonically increasing revisions. invalidate marks a specific ID;
refresh compares previously requested source contents and advances an epoch on change. The
preview polls refresh every 400 ms; normal frame generation performs no disk I/O.
Files/providers are bounded to 16 MiB per resource, 128 MiB cached and 4096 requested IDs.
Missing initial images get a visible diagnostic checkerboard. The validator
rejects those missing resources; failed replacement of an active image keeps the
last good screen. Missing fonts have a visible glyph-box fallback.

The bundled example theme is example:assets/theme.css, and the intrinsic Style
defaults form a minimal fallback theme. Providers can replace documents or any
asset. Runtime::setStyleOverride appends a stylesheet with ordinary specificity
and source order. Inline declarations retain priority. Bad styles/documents/assets
are validated before replacement; active documents and owning draw payloads survive
failed reload. Fonts and images are refreshed through the same epoch mechanism.
In-memory screens can set the theme before loading; hot reload targets resource IDs.

Providers execute trusted host code. Resource packs cannot register native code
or actions. ZIP handling and pack manifests are outside the library.


Provider exceptions during get are reported through its optional Diagnostics;
`refresh(&diagnostics)` reports provider exceptions and retains cached content for
that ID. Providers are trusted synchronous callbacks and must not mutate/reenter
the resolver during a load. Directory canonicalization rejects ordinary symlink
escapes, but is not an OS sandbox against a concurrently modified hostile filesystem.
VectorCache also keys entries on the supplied resolver epoch, so a nested PNG
replacement invalidates standalone cached SVG geometry. Runtime epochs conservatively
invalidate all SVG/text resources; precise per-resource dependency eviction remains
future work.
