#pragma once
#include "draw_list.hpp"
#include <concepts>
namespace voxelui {
inline namespace v1 {
template <class T>
concept RenderBackend = requires(T &backend, const DrawList &list) { backend.render(list); };
template <RenderBackend T> void render(T &backend, const DrawList &list) {
    backend.render(list);
}
template <class T>
concept ResourceProvider = requires(T &provider, std::string_view id) {
    { provider(id) } -> std::same_as<std::optional<std::vector<std::uint8_t>>>;
};
} // namespace v1
} // namespace voxelui
