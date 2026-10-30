# PROJECT: VOXELUI — Independent Declarative Game UI Engine

## ROLE AND MISSION

You are the principal C++23 engine architect, UI runtime developer, vector graphics engineer, rendering systems engineer, and tooling engineer.

Your task is to create **VoxelUI**, a standalone, reusable, high-quality declarative UI framework designed primarily for native game engines.

VoxelUI will eventually be integrated into VoxelCraft, a C++23/Vulkan sandbox game, but **VoxelUI must be developed as a completely independent project**.

The project must be usable without VoxelCraft, Vulkan, Lua, or any particular game engine.

This is an implementation task, not only a design exercise.

Create the source code, build configuration, tests, documentation, examples, and a working preview application.

Use C++23.

Favor static polymorphism, concepts, concrete types, and explicit data structures. Do not introduce virtual inheritance or runtime polymorphism where they are unnecessary.

Do not attempt to implement a full browser engine.

---

# 1. CORE VISION

VoxelUI combines three declarative technologies:

1. **XML** for UI structure, components, templates, data bindings, and actions.
2. **CSS-inspired stylesheets** for styling, layout, visual states, themes, and simple transitions.
3. **SVG** for vector graphics, decorative elements, reusable visual assets, and identified dynamic visual slots.

The host application provides:

- Dynamic data.
- Registered actions.
- Input events.
- Resources.
- A rendering backend.
- Optional custom widgets.

VoxelUI provides:

- Document loading.
- Parsing and validation.
- UI element trees.
- Layout.
- Style resolution.
- Data binding.
- Event routing.
- Focus management.
- Text input.
- Scroll containers.
- Drag-and-drop foundations.
- Animation state.
- Render command generation.

The runtime must not contain game-specific behavior.

A VoxelUI application should be able to define most of its screens without writing new C++ classes.

---

# 2. NON-NEGOTIABLE DESIGN PRINCIPLES

1. No required game engine dependency.
2. No required Vulkan dependency.
3. No required Lua dependency.
4. No browser or webview dependency.
5. No full HTML/CSS/JavaScript implementation.
6. No requirement for C++ virtual interfaces in the public rendering and platform integration APIs.
7. No arbitrary executable code inside XML, CSS, or SVG.
8. Keep authored UI documents editable as ordinary text files.
9. Support vector graphics authored in external applications such as Inkscape.
10. Support runtime data updates without rebuilding entire screens.
11. Support hot reload during development.
12. Preserve UI state across safe document and data updates.
13. Make failure diagnostics useful and precise.
14. Prioritize deterministic behavior, testability, and clean architecture.
15. Support Linux and Windows.

Prefer a small number of clearly designed abstractions over excessive layers and boilerplate.

---

# 3. PROJECT STRUCTURE

Create a repository with an architecture approximately like:

```text
VoxelUI/
├── CMakeLists.txt
├── CMakePresets.json
├── AGENTS.md
├── README.md
├── LICENSE
├── cmake/
├── include/
│   └── voxelui/
│       ├── runtime.hpp
│       ├── document.hpp
│       ├── events.hpp
│       ├── binding.hpp
│       ├── actions.hpp
│       ├── draw_list.hpp
│       ├── resources.hpp
│       ├── components.hpp
│       └── concepts.hpp
├── src/
│   ├── xml/
│   ├── css/
│   ├── svg/
│   ├── dom/
│   ├── layout/
│   ├── widgets/
│   ├── binding/
│   ├── events/
│   ├── text/
│   ├── rendering/
│   └── runtime/
├── apps/
│   ├── preview/
│   └── validator/
├── backends/
│   ├── software/
│   └── sdl3/
├── examples/
│   ├── main_menu/
│   ├── settings/
│   ├── inventory/
│   ├── world_selection/
│   ├── hud/
│   └── svg_gallery/
├── tests/
└── docs/
```

This structure is illustrative. Adjust it where necessary to preserve clean boundaries.

Use independent CMake targets for major modules.

Avoid creating an unnecessarily large number of tiny libraries.

---

# 4. BUILD SYSTEM AND DEPENDENCIES

Use CMake and C++23.

Provide Linux and Windows build configurations.

Prefer CMake + Ninja during development.

The project must support headless unit tests.

The central runtime must not require SDL3 or Vulkan.

SDL3 may be used for the standalone preview application through an optional adapter.

A limited number of small, permissively licensed, well-established dependencies may be used when they solve genuinely difficult problems, such as font rasterization.

However:

- Do not introduce a complete external UI framework.
- Do not introduce a browser engine.
- Do not introduce JavaScript.
- Avoid mandatory network access during normal application runtime.
- Document all third-party components and licenses.
- Make dependencies reproducible and pinned.
- Prefer reusing the standard library when practical.

Do not implement production-grade XML parsing or complex text shaping with unsafe shortcuts merely to avoid a small dependency.

If external libraries are necessary, isolate them behind private implementation boundaries.

---

# 5. XML DOCUMENT MODEL

Define a stable, versioned XML document format.

Example:

```xml
<Screen
    version="1"
    id="example:main_menu"
    stylesheet="example:styles/main">

    <Column class="menu">

        <Image source="example:logo"/>

        <Text value="@menu.title"/>

        <Button action="example:play">
            <Text value="@menu.play"/>
        </Button>

    </Column>

</Screen>
```

Required standard components:

- Screen
- Panel
- Row
- Column
- Grid
- Stack
- ScrollView
- Text
- Image
- Vector
- Button
- Toggle
- Checkbox
- Slider
- TextInput
- ProgressBar
- Repeat
- Template
- Component

Also provide reusable modal dialog and tooltip primitives.

Build an actual hierarchical UI tree.

Do not represent the main runtime as a flat array of hardcoded screen coordinates.

Separate the authored document tree from the computed layout and rendering representations.

Use stable element identifiers.

Support reusable XML components and templates.

Use namespaced resource identifiers.

Validate unknown tags, incorrect attributes, and malformed nesting.

Never execute arbitrary script content from documents.

Disable XML DTDs, external entities, and external resource expansion.

Apply limits to nesting depth, document size, attributes, and generated element counts.

---

# 6. STYLE SYSTEM

Implement a deliberately limited CSS-inspired language.

Do not attempt full CSS compliance.

Support:

- Element selectors.
- Class selectors.
- ID selectors.
- Basic pseudo-states.
- Cascade and specificity.
- Inheritance for appropriate properties.
- CSS-style variables.
- Stylesheet imports through the resource resolver, with cycle detection.
- Inline style overrides where justified.

Required states:

- hover
- focus
- active
- disabled
- selected

Required styling capabilities:

- Background color.
- Foreground/text color.
- Borders.
- Border radius.
- Opacity.
- Padding.
- Margin.
- Gaps.
- Width/height.
- Minimum and maximum dimensions.
- Alignment.
- Font family.
- Font size.
- Font weight.
- Text alignment.
- Image fitting.
- Overflow and clipping.
- Visibility.

Supported logical length units:

- dp
- %
- auto

Additional units may be introduced with clear definitions.

Define unambiguous layout and dimension resolution rules.

Provide clear diagnostic errors for invalid style declarations.

Unknown properties must not crash the runtime.

---

# 7. LAYOUT ENGINE

Implement a small, predictable layout engine.

Required layout modes:

1. Flex-like rows.
2. Flex-like columns.
3. Grid layout.
4. Overlay/stack layout.
5. Anchored positioning.
6. Scrollable containers.

Support:

- Nested layouts.
- Automatic sizing.
- Intrinsic sizing.
- Min/max constraints.
- Responsive layouts.
- Logical coordinates.
- High-DPI scaling.
- UI scaling.
- Clipping.
- Hit testing based on final layout geometry.

Do not repeatedly recalculate the entire layout without a change that invalidates it.

Implement layout dirty flags or another clearly justified invalidation mechanism.

Avoid hidden global layout state.

Define behavior for conflicting constraints and cycles.

Verify layout behavior at multiple resolutions.

---

# 8. SVG SUBSYSTEM

SVG is a first-class visual asset format.

Its primary role is to define scalable visual resources.

Implement a documented and explicitly limited SVG profile for the first release.

Initial supported features:

- svg
- g
- rect
- circle
- ellipse
- line
- polyline
- polygon
- path
- viewBox
- Basic transforms
- Fill
- Stroke
- Opacity
- Simple gradients
- Basic clipping

Support referenced raster images only through the resource resolver.

Provide clear fallback behavior for unsupported SVG features.

Do not silently treat unsupported advanced filters as correctly rendered.

SVG parsing must be safe for untrusted resource files.

Disallow external network references, scripts, foreignObject, and external entity loading.

Implement caching of parsed vector documents.

Avoid reparsing or retessellating static SVG geometry every frame.

Use appropriate path flattening and tessellation with documented tolerance and clipping behavior.

SVG resources must be scalable without persistent loss of quality.

### Dynamic SVG slots

Support identified visual anchors or named insertion points.

Example:

```xml
<svg viewBox="0 0 800 480">
    <g id="frame">
        <!-- decorative shapes -->
    </g>

    <g
        id="inventory-anchor"
        data-voxelui-slot="inventory.grid">
    </g>
</svg>
```

The host must be able to associate a recognized slot with a dynamic UI component.

The slot contract must define coordinate spaces, sizing, transforms, hit testing, and clipping.

Avoid arbitrary XML node mutation from gameplay code every frame.

### Important architectural distinction

Do not force every button, rectangle, or text label to be represented as SVG.

Native primitives should remain efficient.

SVG is for custom shapes and visual content.

---

# 9. RENDER COMMAND ARCHITECTURE

VoxelUI must be independent of graphics APIs.

Implement a typed UI draw list.

Suggested commands:

- DrawRectangle
- DrawRoundedRectangle
- DrawImage
- DrawText
- DrawVectorGeometry
- PushClip
- PopClip
- PushTransform
- PopTransform

Draw commands must preserve visual stacking order.

Include resource handles rather than raw Vulkan-specific objects.

Define ownership and lifetime rules for draw list resources.

Keep the draw list consumable by multiple backends.

Use efficient geometry caching and batching.

Batch compatible commands without breaking clipping or draw order.

Do not allocate unnecessary temporary memory per widget every frame.

---

# 10. REFERENCE RENDERER AND PREVIEW TOOL

Create a standalone preview application.

It must not require VoxelCraft.

The preview tool must:

- Open a UI XML document.
- Resolve its styles and assets.
- Display the resulting interface.
- Support keyboard and mouse input.
- Support window resizing.
- Support UI scale changes.
- Show parsing and layout diagnostics.
- Support hot reload.
- Show element boundaries in debug mode.
- Display element IDs and computed bounds.
- Provide mock data sources.

A simple reference software renderer is required for deterministic screenshot tests.

Use an optional SDL3 preview adapter where practical.

The software renderer may implement a deliberately limited but well-documented subset of drawing commands initially, provided the preview application can still demonstrate all essential UI elements.

Do not leave the preview application as an empty window or nonfunctional stub.

Do not make Vulkan a required dependency.

Provide a clean extension point for a future Vulkan adapter.

---

# 11. TEXT SYSTEM

Implement a practical and robust text subsystem.

Requirements:

- UTF-8 support.
- Cyrillic and Latin.
- Glyph caching.
- Font resource loading.
- Font fallback.
- Text measurement.
- Line wrapping.
- Text clipping.
- Horizontal alignment.
- Vertical alignment.
- Caret movement.
- Selection.
- Copy/paste through platform adapter.
- Text input events.
- IME composition support where available.

Clearly distinguish Unicode code points, grapheme clusters, and shaping.

Do not assume UTF-8 bytes correspond to characters.

Prefer using an appropriate small established shaping library if needed for correctness; isolate it from the core architecture.

The first milestone must deliver reliable English/Russian UI and text entry.

Document limitations for complex writing systems if full shaping is not yet implemented.

---

# 12. DATA BINDING

Implement a typed, observable data model.

Example:

```xml
<Text value="{player.name}"/>

<ProgressBar value="{player.healthRatio}"/>

<Repeat items="{worlds.available}" key="id">
    <WorldCard
        name="{item.name}"
        worldId="{item.id}"/>
</Repeat>
```

Provide:

- Read-only bindings.
- Explicit writable bindings where supported.
- Stable typed values.
- Observable change notifications.
- Dynamic collections.
- Keyed list reconciliation.
- Dependency tracking or equivalent dirty propagation.
- Safe handling of missing data.
- Type validation and diagnostics.

Do not implement a general-purpose expression language in XML.

Support simple property paths.

Computed values should be provided by the host or registered typed computed bindings.

Avoid polling and rebuilding entire documents every frame.

Preserve scroll position, selection and focus where logically possible.

---

# 13. ACTION REGISTRY

Create a host-controlled action registry.

XML may refer to declared action identifiers.

Example:

```xml
<Button
    action="example:open_world"
    worldId="{item.id}">
    Play
</Button>
```

The action dispatcher must:

- Validate action existence.
- Validate typed parameters.
- Capture the relevant UI context.
- Return or report errors.
- Avoid dangling references.
- Support explicit action permissions/capabilities where appropriate.
- Permit host-managed asynchronous operations.

UI XML must never execute arbitrary C++ functions by name or address.

The action registry is a capability boundary, not a security sandbox for malicious native code.

Do not expose raw pointers or object memory addresses to XML.

The API must work for both built-in application actions and future native mod actions.

---

# 14. EVENT SYSTEM

Support:

- Mouse move.
- Mouse down/up.
- Mouse wheel.
- Keyboard down/up.
- Text input.
- Text editing/composition.
- Focus gained/lost.
- Hover transitions.
- Widget activation.
- Selection.
- Drag start/move/end.
- Window resize.
- UI scale changes.

Separate platform event translation from UI event processing.

Provide deterministic event targeting and propagation rules.

Support pointer capture for drag-and-drop.

Support modal input blocking.

Provide keyboard navigation between focusable controls.

Handle removal of focused or captured elements without dangling references.

Avoid delivering UI events to gameplay while the UI owns that input.

---

# 15. WIDGETS AND GAME-SPECIFIC EXTENSION POINTS

VoxelUI must be reusable outside games.

Do not hardcode inventory, player health, world selection, or entity preview behavior into the core runtime.

Instead provide generic components plus custom widget registration.

Example:

```cpp
template<typename T>
concept UiWidgetImplementation = requires(
    T& widget,
    const UiLayoutContext& layout,
    UiDrawList& drawList
) {
    widget.measure(layout);
    widget.draw(drawList);
};
```

This is illustrative; design the actual concept according to the runtime needs.

Allow the host application to register custom widget types.

Consider whether event handling, measurement, layout, painting, and lifecycle should be separate capabilities.

Custom widget code must not require changing the XML parser for each new tag.

Provide at least one example custom component.

Keep built-in UI widgets efficient and directly testable.

---

# 16. STATIC POLYMORPHISM AND PERFORMANCE

Use C++23 concepts and templates where they improve compile-time integration and eliminate unnecessary runtime dispatch.

Examples include:

- Render backend adapters.
- Platform adapters.
- Resource providers.
- Static widget adapters.
- Policy-based algorithms.

Do not automatically turn every class into a template.

Do not build a huge recursive application template with every implementation type exposed everywhere.

Runtime UI documents are inherently dynamic and will require runtime type identification and dispatch in some places.

Use explicit enums, tagged unions, stable handles, function tables, or other well-justified mechanisms where necessary.

Avoid virtual inheritance as the default design.

Document every unavoidable runtime dispatch boundary.

Profile before optimizing dispatch that occurs only once per frame or once per screen.

Focus performance optimization on:

- Layout invalidation.
- Text caching.
- SVG tessellation caching.
- Stable resource handles.
- Render batching.
- Dynamic list reconciliation.
- Allocator behavior.

---

# 17. RESOURCE RESOLUTION

Provide a namespaced resource identifier model.

Examples:

```text
voxelcraft:ui/main_menu
voxelcraft:ui/button
voxelcraft:fonts/default
example:textures/custom_icon
```

Define stable resource IDs independent of filesystem location.

Support resource providers with priority rules.

Built-in assets and external packs must be resolvable using the same abstraction.

Implement:

- Resource lookup.
- Cache management.
- Reload notifications.
- Missing asset fallbacks.
- Explicit diagnostics.
- Dependency tracking where practical.

Do not hardcode VoxelCraft's filesystem layout inside VoxelUI.

Resource pack ZIP handling may remain outside the core library, but the core resource abstraction must support an external provider implementing it.

---

# 18. THEMING AND OVERRIDES

Create a theme system.

Themes must be able to change:

- Colors.
- Fonts.
- Component styling.
- Spacing.
- Borders.
- Decorative SVG resources.
- Visual states.

Allow replacement of entire UI documents through resource resolution.

Support style overrides without copying the original XML document.

Define deterministic stylesheet precedence.

Provide a built-in fallback theme.

Provide a safe recovery mechanism when an external theme or document is invalid.

A resource pack must not be able to execute arbitrary native code.

---

# 19. HOT RELOAD

Implement development-time hot reload for:

- XML.
- CSS.
- SVG.
- Fonts where feasible.
- Referenced images.

Changes must be validated before replacing active resources.

If reload fails:

- Keep the last known-good document where possible.
- Report a diagnostic.
- Avoid destroying the active UI.
- Avoid invalid handles and use-after-free.

Preserve interaction state when stable element identities and types permit it.

The preview application must expose reload errors clearly.

---

# 20. DOCUMENT VALIDATOR

Provide a command-line validator.

Example:

```bash
voxelui-validate examples/main_menu/main.xml
```

It must detect:

- Malformed XML.
- Unknown components.
- Unknown style properties.
- Invalid resource references.
- Incorrect data binding paths when a schema is available.
- Missing actions when a registry/schema is available.
- Duplicate element IDs.
- Invalid templates.
- Excessive nesting.
- Illegal external references.
- Circular imports.

Provide filename, line, column, severity, and actionable messages where possible.

Do not claim successful validation merely because XML syntax is well formed.

Distinguish syntactic validation from schema and resource validation.

---

# 21. EXAMPLES

Create working examples, not placeholders.

### Main menu

Buttons, background, SVG decorations, transitions.

### Settings screen

Sliders, toggles, dropdown/select controls, data binding.

### Inventory

A generic grid of items with icons, quantities, tooltips, selection, and drag-and-drop demonstration.

Use mock data. Do not embed VoxelCraft gameplay logic.

### World selection

Dynamic list with stable keys, thumbnails, selection, and scroll preservation.

### HUD

Anchored elements, progress bars, status indicators.

### SVG gallery

Different vector shapes, transforms, gradients, scaling, clipping, and unsupported-feature diagnostics.

Each example must be loadable through the preview application.

---

# 22. TESTING REQUIREMENTS

Implement unit and integration tests for:

### XML

- Valid documents.
- Invalid documents.
- Nesting limits.
- Duplicate IDs.
- Template references.
- Resource IDs.

### CSS

- Selectors.
- Specificity.
- Inheritance.
- Variables.
- Invalid declarations.
- Override precedence.

### Layout

- Rows.
- Columns.
- Grid.
- Constraints.
- Nested clipping.
- Scroll bounds.
- DPI scaling.

### SVG

- Path parsing.
- Geometry transforms.
- Fill and stroke.
- Clipping.
- ViewBox scaling.
- Cache invalidation.

### Data binding

- Updates.
- Collections.
- Keyed reconciliation.
- Missing values.
- Type mismatches.

### Input

- Mouse hit testing.
- Focus.
- Keyboard navigation.
- Text editing.
- Dragging.
- Modal behavior.

### Lifecycle

- Screen creation.
- Screen destruction.
- Resource replacement.
- Hot reload.
- Unregistering actions.
- Removing focused elements.

### Rendering

- Draw command ordering.
- Clipping.
- Deterministic reference screenshots.
- Correct handling of resized viewports.

Use headless tests wherever practical.

Run sanitizers on supported platforms.

Document tests that could not be executed in the current environment.

---

# 23. PERFORMANCE REQUIREMENTS

Create benchmarks for:

- Parsing a medium-sized UI document.
- Building a widget tree.
- Resolving styles.
- Initial layout.
- Incremental layout.
- Updating a dynamic list.
- Text layout.
- SVG loading.
- Cached SVG drawing.
- Generating a frame's draw list.

Create representative test scenes containing hundreds or thousands of lightweight UI elements.

Do not assert arbitrary performance claims without measurements.

Provide measurement instructions and baseline results.

Avoid reparsing documents or loading fonts during ordinary frame rendering.

Use appropriate dirty-state tracking.

Do not introduce background threads unless they solve a measurable problem.

---

# 24. HOST INTEGRATION CONTRACT

VoxelUI will eventually integrate into VoxelCraft.

Define a small public integration contract.

The host must be able to:

1. Construct the UI runtime.
2. Register resource providers.
3. Register actions.
4. Register data models.
5. Register custom widgets.
6. Load screens.
7. Feed input events.
8. Update UI state.
9. Obtain draw commands.
10. Handle diagnostics.
11. Destroy screens and runtime safely.

Illustrative usage:

```cpp
voxelui::Runtime ui;

ui.resources().mount(provider);
ui.actions().registerAction(...);
ui.data().registerModel(...);

auto screen = ui.load("voxelcraft:ui/main_menu");

ui.processEvent(event);
ui.update(frameInfo);

auto drawList = ui.buildDrawList(viewport);
```

The exact C++ API should be designed and documented, not copied blindly from this illustration.

Do not include `GameWorld`, `VulkanRenderer`, or `SandboxUiController` in the public VoxelUI headers.

Provide a standalone integration example with a mock host.

---

# 25. VULKAN INTEGRATION PREPARATION

Do not implement a hard dependency on Vulkan.

Instead document the requirements for a Vulkan renderer consuming UiDrawList.

The draw representation must support:

- Multiple texture resources.
- Scissor clipping.
- UI transforms.
- Alpha blending.
- Text atlas resources.
- Vector geometry.
- Correct draw ordering.
- High-DPI framebuffer scaling.

The future VoxelCraft Vulkan adapter should not need to parse XML or CSS.

It must only consume the runtime's rendering output and resource handles.

This separation is mandatory.

---

# 26. SECURITY AND ROBUSTNESS

Treat external documents and resources as untrusted input.

Implement bounded parsing and validation.

Reject unsupported external references.

Prevent path traversal through resource IDs.

Avoid XML entity expansion attacks.

Avoid uncontrolled template or import recursion.

Limit generated widget counts.

Avoid unbounded memory use from SVG paths.

Use safe resource ownership and lifetime management.

Never allow document content to invoke arbitrary native symbols.

A native plugin supplied by the host is trusted executable code, not sandboxed by VoxelUI.

Clearly document this distinction.

---

# 27. DOCUMENTATION

Create:

- README.md
- docs/architecture.md
- docs/xml-format.md
- docs/style-language.md
- docs/svg-profile.md
- docs/data-binding.md
- docs/action-system.md
- docs/render-backend.md
- docs/custom-widgets.md
- docs/resource-system.md
- docs/host-integration.md
- docs/performance.md
- docs/limitations.md

Also provide a short tutorial:

**Build a custom game menu without writing C++.**

The tutorial must demonstrate an XML document, CSS styles, SVG asset, preview application, and registered host action.

Clearly distinguish supported syntax from future proposals.

---

# 28. IMPLEMENTATION PROCESS

Begin with an architecture and feasibility pass.

Document the actual API boundaries and dependency graph.

Then implement in milestones.

## Milestone A — Core runtime

- CMake project.
- XML parser.
- DOM.
- Basic components.
- Basic styles.
- Flex/stack layout.
- Draw list.
- Headless tests.

## Milestone B — Rendering and preview

- Reference renderer.
- Preview application.
- Text rendering.
- Images.
- Clipping.
- Hit testing.

## Milestone C — Rich UI behavior

- Input.
- Focus.
- Text editing.
- Data binding.
- Dynamic lists.
- Actions.
- Grid and scrolling.

## Milestone D — SVG and customization

- SVG profile.
- Cached vector geometry.
- Themes.
- Resource providers.
- Hot reload.
- Validator.

## Milestone E — Polish and release preparation

- Examples.
- Performance benchmarks.
- Integration examples.
- Sanitizers.
- Linux build.
- Windows build/CI.
- Documentation.
- Final API review.

Create docs/implementation-progress.md and maintain it throughout execution.

Commit or otherwise preserve coherent progress at milestone boundaries when the environment supports it.

Keep the project compiling as work progresses.

Do not declare a milestone complete if its essential features are placeholders.

---

# 29. ACCEPTANCE CRITERIA

The first standalone version of VoxelUI is complete when:

1. VoxelUI builds independently of VoxelCraft.
2. The core builds without Vulkan.
3. XML screens are loaded and validated.
4. CSS styling and cascade work.
5. Layout works on multiple resolutions.
6. SVG assets render correctly within the documented subset.
7. Standard UI controls respond to input.
8. Text input works with English and Cyrillic.
9. Data binding updates visible UI state.
10. Dynamic lists preserve stable identities.
11. Action callbacks execute through the host registry.
12. Hot reload works without crashing.
13. Invalid documents produce useful diagnostics.
14. The preview application displays the examples.
15. Reference screenshot tests pass in a reproducible environment.
16. A mock application integrates VoxelUI through public headers.
17. No game-specific subsystem is needed.
18. The API is documented sufficiently for future Vulkan integration.
19. Linux tests pass.
20. Windows compatibility is tested or explicitly identified as unverified.

---

# 30. IMPORTANT SCOPE RULES

Do not implement:

- A complete browser.
- HTML5 compatibility.
- JavaScript.
- General-purpose embedded scripting.
- A full CSS standards implementation.
- Full SVG2 compliance in the first release.
- VoxelCraft game logic.
- A launcher.
- A game mod loader.
- A networking protocol.
- A Vulkan renderer as a hard dependency.

These are outside the scope.

Focus on a reliable, expressive, standalone UI engine.

---

# 31. FINAL DELIVERY

At completion, provide:

1. Implemented architecture overview.
2. Public API summary.
3. Supported XML syntax.
4. Supported CSS subset.
5. Supported SVG subset.
6. Working preview instructions.
7. Example project instructions.
8. Build and test commands.
9. Test results.
10. Known limitations.
11. Performance results.
12. Recommended next steps for VoxelCraft integration.

Distinguish genuinely implemented features from partial work.

Never claim that a component is complete solely because it compiles.

The final result should be a reusable foundation for sophisticated game interfaces and future modifiable UI systems.

**Begin implementing VoxelUI as a new standalone C++23 project.**
