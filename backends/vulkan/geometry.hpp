#pragma once
#include "atlas.hpp"
#include "tessellation.hpp"
#include <array>
#include <expected>
#include <voxelui/draw_list.hpp>

namespace voxelui::vulkan {
// Optional backend geometry and bounded immutable resource preparation.
struct Vertex {
    Vec2 position;
    Color color;
    std::array<float, 3> inverseX{}, inverseY{};
    Rect bounds;
    Vec2 rounding{-1, 0}; // radius < 0: plain triangle; otherwise radius, border width
    Color border;
    Rect sample; // atlas texels; rounding.x == -2 selects sampled image/glyph
};
struct Mesh {
    std::shared_ptr<const Image> atlas;
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
// All triangles share one atlas pipeline and retain source painter order.
class Compiler {
    Atlas atlas_;
    VectorMeshesCache vectors_;
    std::uint64_t tessellations_{};

  public:
    std::expected<Mesh, GeometryError> compile(const DrawList &, GeometryLimits = {});
    std::uint64_t tessellations() const {
        return tessellations_;
    }
    void clear() {
        atlas_.clear();
        vectors_.clear();
    }
};
std::expected<Mesh, GeometryError> compileGeometry(const DrawList &, GeometryLimits = {});
} // namespace voxelui::vulkan
