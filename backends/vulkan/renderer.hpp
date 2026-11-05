#pragma once
#include "geometry.hpp"
#include <memory>
#include <span>
#include <vulkan/vulkan.h>

namespace voxelui::vulkan {
enum class ResourceKind {
    buffer,
    memory,
    image,
    imageView,
    sampler,
    descriptorPool,
    descriptorSet,
    staging,
    atlasPage,
    pipeline,
    pipelineLayout,
    descriptorLayout,
    count
};
struct LifetimeObserver {
    void (*changed)(ResourceKind, bool, void *) noexcept {};
    void *context{};
    void operator()(ResourceKind kind, bool allocated) const noexcept {
        if (changed)
            changed(kind, allocated, context);
    }
};
// Experimental host-context renderer, Vulkan 1.3 dynamic rendering.
// Host owns the device, rendering scope, submission and completion fence.
// Each slot's fence must be signaled before upload; reset it only immediately
// before submitting commands recorded for that slot. Do not mutate/reuse a slot
// between recording and submission. Complete all submissions before destruction.
class Renderer {
  public:
    Renderer(VkPhysicalDevice, VkDevice, VkFormat, std::span<const VkFence> slots, LifetimeObserver = {});
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    void upload(std::size_t slot, const Mesh &);
    struct Statistics {
        std::uint64_t bufferAllocations{};
        VkDeviceSize residentBufferBytes{}, atlasBytes{};
        std::uint64_t atlasUploads{}, uploadBytes{};
    };
    Statistics statistics() const;
    // Record pending atlas transfers before beginning dynamic rendering.
    void recordUploads(VkCommandBuffer, std::size_t slot);
    // Inside single-sample dynamic rendering, one color attachment, no depth.
    // Changes pipeline, descriptors, viewport/scissor, buffers and push constants.
    void record(VkCommandBuffer, std::size_t slot, VkExtent2D);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace voxelui::vulkan
