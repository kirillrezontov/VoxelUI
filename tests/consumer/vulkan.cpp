#include <renderer.hpp>
#include <stdexcept>
int main() {
    voxelui::DrawList list;
    list.logicalSize = {64, 64};
    list.commands.push_back(voxelui::DrawRectangle{{0, 0, 20, 20}, {255, 0, 0, 255}, {}, 0, 4});
    voxelui::vulkan::Compiler compiler;
    auto mesh = compiler.compile(list);
    if (!mesh || mesh->indices.empty())
        return 1;
    // Link the renderer implementation without requiring a device in package CI.
    try {
        voxelui::vulkan::Renderer invalid(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_FORMAT_UNDEFINED, {});
        return 1;
    } catch (const std::invalid_argument &) {
        return 0;
    }
}
