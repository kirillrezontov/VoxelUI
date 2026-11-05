#pragma once
#include <expected>
#include <array>
#include <voxelui/draw_list.hpp>

namespace voxelui::vulkan {
// Experimental backend-private CPU preparation. No Vulkan or core ABI change.
struct Vertex {
    Vec2 position;
    Color color;
    std::array<float,3> inverseX{},inverseY{};
    Rect bounds;
    Vec2 rounding{-1, 0}; // radius < 0: plain triangle; otherwise radius, border width
    Color border;
};
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};
struct GeometryLimits {
    std::size_t commands{100000}, vertices{1000000}, indices{3000000}, depth{64};
};
struct GeometryError {
    std::size_t command; // commands.size() denotes viewport/end-of-stream failure
    std::string message;
};
// Transactional: errors return no partial mesh. Coordinates are framebuffer pixels.
// All triangles share the solid-color pipeline and retain source painter order.
std::expected<Mesh, GeometryError> compileGeometry(const DrawList &, GeometryLimits = {});
} // namespace voxelui::vulkan
