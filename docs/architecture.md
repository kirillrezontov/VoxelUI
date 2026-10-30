# Architecture (API version 1, library 0.1)

Dependencies flow from applications → software backend → runtime → document/style/layout/text/vector/resource modules. The core target has no SDL or Vulkan dependency. Libxml2, FreeType and libpng are private implementation dependencies. SDL3 is optional and belongs only to the preview executable.

Authored Node trees retain XML attributes and locations. Runtime expansion produces a separate hierarchical tree; stable string identities address interaction state. Layout produces boxes and clips. Rendering produces ordered, owning variant commands. Backends never parse documents. Resource providers, actions and widget functions are the explicit runtime dispatch boundaries (std::function); render adapters use a concept. No virtual base classes or script execution.

Resources use namespace:path identifiers, provider priority, bounded reads, revisions, and shared ownership. Hot reload builds a candidate before swapping. The runtime is single threaded; host callbacks run synchronously and should enqueue long running work in the host.

See implementation-progress.md for verified status and limitations.md for gaps.

Implementation files now separate interaction (`src/events`), runtime lifecycle
(`src/runtime`), binding expansion (`src/binding`), painting (`src/rendering`),
layout, CSS, XML, SVG and text. The two public link targets are VoxelUI::voxelui
and VoxelUI::software. Internal helpers have no exported ABI. API v1 is explicit
but this 0.1 foundation has not yet made a long-term ABI compatibility promise.
