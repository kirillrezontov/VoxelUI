# VoxelUI — Development Instructions

VoxelUI is an independent, reusable C++23 declarative UI engine.

## Architecture

- Keep VoxelUI completely independent from VoxelCraft.
- Use XML for UI structure, SVG for vector graphics, and CSS-inspired styles for presentation.
- Prefer C++23 concepts, templates, and concrete types over virtual inheritance.
- Avoid unnecessary runtime polymorphism in performance-sensitive code.
- Keep the core independent of Vulkan, SDL3, and game-specific systems.
- Use explicit, versioned public interfaces.
- Separate document parsing, layout, styling, interaction, and rendering.
- Do not implement a browser engine or embed JavaScript/Lua.

## Implementation quality

- Implement working functionality, not placeholder APIs.
- Preserve clear module boundaries and avoid cyclic dependencies.
- Keep the project buildable after each implementation milestone.
- Add tests for new behavior.
- Treat XML, SVG, CSS, and external resources as untrusted input.
- Document limitations and unsupported features honestly.
- Prefer incremental, verifiable changes over large rewrites.

## Rendering

- Generate graphics-API-independent draw commands.
- Provide a standalone preview application.
- Keep Vulkan integration optional and external.
- Cache parsed SVG geometry, text layout, and reusable resources.

## Modularity

- Support external resource providers.
- Support registered actions, data models, and custom widgets.
- Do not embed game-specific inventory, world, or entity logic in the core.
- Design public APIs for future native mod integration without implementing a mod loader.

## Completion

Before declaring work complete:

1. Build the project.
2. Run available tests.
3. Run the preview application where supported.
4. Update documentation.
5. Report implemented features and remaining limitations.
6. Never claim unexecuted tests passed.
