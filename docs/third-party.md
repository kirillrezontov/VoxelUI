# Third-party components

- libxml2 (MIT): bounded XML parsing, no DTD/NOENT/XInclude processing enabled.
- FreeType (FreeType License or GPLv2): glyph rasterization; use the FreeType License.
- libpng (PNG Reference Library License) and transitive zlib (zlib License): PNG I/O.
- SDL3 (zlib License): optional preview only.
- Bundled NotoSans-Regular.ttf (SIL Open Font License 1.1): examples/assets/FONT-LICENSE.txt.

The runtime does not fetch dependencies or contact network services. Default
builds use system libraries. vcpkg.json pins the dependency registry baseline;
CI checks out that exact vcpkg revision. Enable the preview vcpkg feature explicitly
with -DVCPKG_MANIFEST_FEATURES=preview. The local verified build used system
versions recorded in implementation-progress.md; that run did not exercise vcpkg.
Consumers distributing third-party binaries must retain their upstream license
notices. The bundled font's complete notice ships alongside the asset.
