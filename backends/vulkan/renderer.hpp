#pragma once
#include "geometry.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <span>

namespace voxelui::vulkan {
// Experimental host-context renderer, Vulkan 1.3 dynamic rendering.
// Host owns the device, rendering scope, submission and completion fence.
// Each slot's fence must be signaled before upload; reset it only immediately
// before submitting commands recorded for that slot. Do not mutate/reuse a slot
// between recording and submission. Complete all submissions before destruction.
class Renderer {
public:
    Renderer(VkPhysicalDevice, VkDevice, VkFormat, std::span<const VkFence> slots);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void upload(std::size_t slot, const Mesh&);
    struct Statistics { std::uint64_t bufferAllocations{}; VkDeviceSize residentBufferBytes{}; };
    Statistics statistics() const;
    // Inside host dynamic rendering, single-sample color attachment, no depth.
    // Sets pipeline, viewport, scissor, vertex/index bindings and push constants.
    void record(VkCommandBuffer, std::size_t slot, VkExtent2D);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
