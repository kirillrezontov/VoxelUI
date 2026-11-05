#include "renderer.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace voxelui::vulkan {
namespace {
void check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
template <class T> T info(VkStructureType type) {
    T value{};
    value.sType = type;
    return value;
}
const std::uint32_t vertexCode[] =
#include "solid.vert.inc"
    ;
const std::uint32_t fragmentCode[] =
#include "solid.frag.inc"
    ;
struct Buffer {
    LifetimeObserver observer;
    bool staging{};
    VkDevice device{};
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    void *mapped{};
    VkDeviceSize capacity{};
    Buffer() = default;
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&other) noexcept {
        swap(other);
    }
    Buffer &operator=(Buffer &&other) noexcept {
        Buffer old(std::move(other));
        swap(old);
        return *this;
    }
    void swap(Buffer &other) noexcept {
        std::swap(observer, other.observer);
        std::swap(staging, other.staging);
        std::swap(device, other.device);
        std::swap(buffer, other.buffer);
        std::swap(memory, other.memory);
        std::swap(mapped, other.mapped);
        std::swap(capacity, other.capacity);
    }
    ~Buffer() {
        if (mapped)
            vkUnmapMemory(device, memory);
        if (buffer) {
            vkDestroyBuffer(device, buffer, nullptr);
            observer(ResourceKind::buffer, false);
            if (staging)
                observer(ResourceKind::staging, false);
        }
        if (memory) {
            vkFreeMemory(device, memory, nullptr);
            observer(ResourceKind::memory, false);
        }
    }
};
} // namespace
struct AtlasImage {
    LifetimeObserver observer;
    VkDevice device{};
    VkImage image{};
    VkImageView view{};
    VkDeviceMemory memory{};
    int width{}, height{};
    bool initialized{};
    ~AtlasImage() {
        if (view) {
            vkDestroyImageView(device, view, nullptr);
            observer(ResourceKind::imageView, false);
        }
        if (image) {
            vkDestroyImage(device, image, nullptr);
            observer(ResourceKind::image, false);
            observer(ResourceKind::atlasPage, false);
        }
        if (memory) {
            vkFreeMemory(device, memory, nullptr);
            observer(ResourceKind::memory, false);
        }
    }
};
struct Renderer::Impl {
    LifetimeObserver observer;
    std::uint64_t atlasUploads{}, uploadBytes{};
    VkDevice device{};
    VkPhysicalDeviceMemoryProperties properties{};
    VkPipelineLayout layout{};
    VkPipeline pipeline{};
    bool srgb{};
    std::uint64_t bufferAllocations{};
    VkDescriptorSetLayout descriptors{};
    VkDescriptorPool pool{};
    VkSampler sampler{};
    std::shared_ptr<const Image> white = std::make_shared<Image>(Image{1, 1, {255, 255, 255, 255}});
    struct Slot {
        VkFence fence{};
        Buffer vertices, indices, staging;
        std::uint32_t count{};
        VkDescriptorSet descriptor{};
        std::unique_ptr<AtlasImage> texture;
        std::shared_ptr<const Image> snapshot;
        bool pending{};
    };
    std::vector<Slot> slots;
    ~Impl() {
        if (pipeline) {
            vkDestroyPipeline(device, pipeline, nullptr);
            observer(ResourceKind::pipeline, false);
        }
        if (layout) {
            vkDestroyPipelineLayout(device, layout, nullptr);
            observer(ResourceKind::pipelineLayout, false);
        }
        if (pool) {
            for (auto &slot : slots)
                if (slot.descriptor)
                    observer(ResourceKind::descriptorSet, false);
            vkDestroyDescriptorPool(device, pool, nullptr);
            observer(ResourceKind::descriptorPool, false);
        }
        if (descriptors) {
            vkDestroyDescriptorSetLayout(device, descriptors, nullptr);
            observer(ResourceKind::descriptorLayout, false);
        }
        if (sampler) {
            vkDestroySampler(device, sampler, nullptr);
            observer(ResourceKind::sampler, false);
        }
    }
    std::unique_ptr<AtlasImage> image(int width, int height) {
        auto result = std::make_unique<AtlasImage>();
        result->device = device;
        result->observer = observer;
        result->width = width;
        result->height = height;
        auto create = info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        create.imageType = VK_IMAGE_TYPE_2D;
        create.format = VK_FORMAT_R8G8B8A8_UNORM;
        create.extent = {std::uint32_t(width), std::uint32_t(height), 1};
        create.mipLevels = create.arrayLayers = 1;
        create.samples = VK_SAMPLE_COUNT_1_BIT;
        create.tiling = VK_IMAGE_TILING_OPTIMAL;
        create.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        check(vkCreateImage(device, &create, nullptr, &result->image), "create atlas image");
        observer(ResourceKind::image, true);
        observer(ResourceKind::atlasPage, true);
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device, result->image, &requirements);
        std::uint32_t type = properties.memoryTypeCount;
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((requirements.memoryTypeBits & (1u << i)) &&
                (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                type = i;
                break;
            }
        if (type == properties.memoryTypeCount)
            throw std::runtime_error("atlas device-local memory unavailable");
        auto allocation = info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = type;
        check(vkAllocateMemory(device, &allocation, nullptr, &result->memory), "allocate atlas memory");
        observer(ResourceKind::memory, true);
        check(vkBindImageMemory(device, result->image, result->memory, 0), "bind atlas memory");
        auto view = info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        view.image = result->image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = create.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(device, &view, nullptr, &result->view), "create atlas view");
        observer(ResourceKind::imageView, true);
        return result;
    }
    Buffer allocate(VkDeviceSize bytes, VkBufferUsageFlags usage) {
        Buffer result;
        result.device = device;
        result.observer = observer;
        result.staging = usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // Geometric growth amortizes edits; bound allocations to geometry budgets.
        result.capacity = std::max<VkDeviceSize>(4096, std::bit_ceil(bytes));
        auto create = info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        create.size = result.capacity;
        create.usage = usage;
        create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &create, nullptr, &result.buffer), "create UI buffer");
        observer(ResourceKind::buffer, true);
        if (result.staging)
            observer(ResourceKind::staging, true);
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, result.buffer, &requirements);
        std::uint32_t type = properties.memoryTypeCount;
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((requirements.memoryTypeBits & (1u << i)) &&
                (properties.memoryTypes[i].propertyFlags &
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                type = i;
                break;
            }
        if (type == properties.memoryTypeCount)
            throw std::runtime_error("UI requires host-visible coherent buffer memory");
        auto alloc = info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = type;
        check(vkAllocateMemory(device, &alloc, nullptr, &result.memory), "allocate UI buffer");
        observer(ResourceKind::memory, true);
        check(vkBindBufferMemory(device, result.buffer, result.memory, 0), "bind UI buffer");
        check(vkMapMemory(device, result.memory, 0, result.capacity, 0, &result.mapped), "map UI buffer");
        ++bufferAllocations;
        return result;
    }
};
Renderer::Renderer(VkPhysicalDevice physical, VkDevice device, VkFormat format,
                   std::span<const VkFence> fences, LifetimeObserver observer)
    : impl_(std::make_unique<Impl>()) {
    if (!physical || !device || fences.empty() || format == VK_FORMAT_UNDEFINED)
        throw std::invalid_argument("invalid UI Vulkan context");
    auto &p = *impl_;
    p.observer = observer;
    p.device = device;
    p.srgb = format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_B8G8R8A8_SRGB;
    vkGetPhysicalDeviceMemoryProperties(physical, &p.properties);
    for (auto fence : fences) {
        if (!fence)
            throw std::invalid_argument("UI frame slot requires a completion fence");
        p.slots.emplace_back();
        p.slots.back().fence = fence;
    }
    VkDescriptorSetLayoutBinding bindingInfo{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    auto descriptorInfo =
        info<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    descriptorInfo.bindingCount = 1;
    descriptorInfo.pBindings = &bindingInfo;
    check(vkCreateDescriptorSetLayout(device, &descriptorInfo, nullptr, &p.descriptors),
          "create atlas descriptor layout");
    p.observer(ResourceKind::descriptorLayout, true);
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::uint32_t(fences.size())};
    auto pool = info<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    pool.maxSets = poolSize.descriptorCount;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    check(vkCreateDescriptorPool(device, &pool, nullptr, &p.pool), "create atlas descriptor pool");
    p.observer(ResourceKind::descriptorPool, true);
    for (auto &slot : p.slots) {
        auto allocate = info<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        allocate.descriptorPool = p.pool;
        allocate.descriptorSetCount = 1;
        allocate.pSetLayouts = &p.descriptors;
        check(vkAllocateDescriptorSets(device, &allocate, &slot.descriptor), "allocate atlas descriptor");
        p.observer(ResourceKind::descriptorSet, true);
    }
    auto sampler = info<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    check(vkCreateSampler(device, &sampler, nullptr, &p.sampler), "create atlas sampler");
    p.observer(ResourceKind::sampler, true);
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                              4 * sizeof(float)};
    auto layout = info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &p.descriptors;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &range;
    check(vkCreatePipelineLayout(device, &layout, nullptr, &p.layout), "create UI pipeline layout");
    p.observer(ResourceKind::pipelineLayout, true);
    struct Shader {
        VkDevice device;
        VkShaderModule module{};
        ~Shader() {
            if (module)
                vkDestroyShaderModule(device, module, nullptr);
        }
    } vert{device}, frag{device};
    auto shader = info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    shader.codeSize = sizeof(vertexCode);
    shader.pCode = vertexCode;
    check(vkCreateShaderModule(device, &shader, nullptr, &vert.module), "create UI vertex shader");
    shader.codeSize = sizeof(fragmentCode);
    shader.pCode = fragmentCode;
    check(vkCreateShaderModule(device, &shader, nullptr, &frag.module), "create UI fragment shader");
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    for (auto &stage : stages) {
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.pName = "main";
    }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert.module;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag.module;
    VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription, 8> attributes{
        {{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position)},
         {1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vertex, color)},
         {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, inverseX)},
         {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, bounds)},
         {4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, rounding)},
         {5, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vertex, border)},
         {6, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, inverseY)},
         {7, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, sample)}}};
    static_assert(sizeof(Color) == 4);
    auto input =
        info<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &binding;
    input.vertexAttributeDescriptionCount = attributes.size();
    input.pVertexAttributeDescriptions = attributes.data();
    auto assembly = info<VkPipelineInputAssemblyStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    auto viewport =
        info<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    viewport.viewportCount = viewport.scissorCount = 1;
    auto raster = info<VkPipelineRasterizationStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1;
    auto samples =
        info<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorWriteMask = 0xf;
    auto blending =
        info<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;
    std::array<VkDynamicState, 2> dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    auto dynamic =
        info<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    dynamic.dynamicStateCount = dynamics.size();
    dynamic.pDynamicStates = dynamics.data();
    auto rendering = info<VkPipelineRenderingCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &format;
    auto pipeline = info<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    pipeline.pNext = &rendering;
    pipeline.stageCount = stages.size();
    pipeline.pStages = stages.data();
    pipeline.layout = p.layout;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &samples;
    pipeline.pColorBlendState = &blending;
    pipeline.pDynamicState = &dynamic;
    check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &p.pipeline),
          "create UI graphics pipeline");
    p.observer(ResourceKind::pipeline, true);
}
Renderer::~Renderer() = default;
Renderer::Statistics Renderer::statistics() const {
    Statistics stats{impl_->bufferAllocations, 0};
    stats.atlasUploads = impl_->atlasUploads;
    stats.uploadBytes = impl_->uploadBytes;
    for (const auto &slot : impl_->slots) {
        stats.residentBufferBytes += slot.vertices.capacity + slot.indices.capacity + slot.staging.capacity;
        if (slot.texture)
            stats.atlasBytes += VkDeviceSize(slot.texture->width) * slot.texture->height * 4;
    }
    return stats;
}
void Renderer::upload(std::size_t slot, const Mesh &mesh) {
    auto &p = *impl_;
    auto &s = p.slots.at(slot);
    check(vkGetFenceStatus(p.device, s.fence), "UI slot must be complete before upload");
    const auto snapshot = mesh.atlas ? mesh.atlas : p.white;
    if (snapshot->width <= 0 || snapshot->height <= 0 || snapshot->width > 2048 || snapshot->height > 2048 ||
        snapshot->rgba.size() != std::size_t(snapshot->width) * snapshot->height * 4)
        throw std::invalid_argument("invalid atlas page");
    const GeometryLimits limits;
    if (mesh.vertices.size() > limits.vertices || mesh.indices.size() > limits.indices ||
        mesh.indices.size() % 3)
        throw std::invalid_argument("UI mesh exceeds limits or has incomplete triangles");
    for (const auto &vertex : mesh.vertices) {
        for (float value : {vertex.position.x, vertex.position.y, vertex.bounds.x, vertex.bounds.y,
                            vertex.bounds.w, vertex.bounds.h, vertex.rounding.x, vertex.rounding.y,
                            vertex.sample.x, vertex.sample.y, vertex.sample.w, vertex.sample.h})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite UI vertex");
        for (auto value : vertex.inverseX)
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite UI inverse");
        for (auto value : vertex.inverseY)
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite UI inverse");
    }
    for (auto index : mesh.indices)
        if (index >= mesh.vertices.size())
            throw std::invalid_argument("UI index out of bounds");
    const VkDeviceSize vb = mesh.vertices.size() * sizeof(Vertex),
                       ib = mesh.indices.size() * sizeof(std::uint32_t);
    // Allocate both replacements before changing the slot, preserving it on failure.
    Buffer vertices, indices;
    if (vb > s.vertices.capacity)
        vertices = p.allocate(vb, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (ib > s.indices.capacity)
        indices = p.allocate(ib, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    if (vertices.buffer)
        s.vertices = std::move(vertices);
    if (indices.buffer)
        s.indices = std::move(indices);
    if (vb)
        std::memcpy(s.vertices.mapped, mesh.vertices.data(), vb);
    if (ib)
        std::memcpy(s.indices.mapped, mesh.indices.data(), ib);
    if (snapshot != s.snapshot) {
        if (!s.texture || s.texture->width != snapshot->width || s.texture->height != snapshot->height) {
            auto texture = p.image(snapshot->width, snapshot->height);
            VkDescriptorImageInfo imageInfo{p.sampler, texture->view,
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            auto write = info<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            write.dstSet = s.descriptor;
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(p.device, 1, &write, 0, nullptr);
            s.texture = std::move(texture);
        }
        if (s.staging.capacity < snapshot->rgba.size())
            s.staging = p.allocate(snapshot->rgba.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(s.staging.mapped, snapshot->rgba.data(), snapshot->rgba.size());
        s.snapshot = snapshot;
        s.pending = true;
    }
    s.count = static_cast<std::uint32_t>(mesh.indices.size());
}
void Renderer::recordUploads(VkCommandBuffer command, std::size_t slot) {
    auto &p = *impl_;
    auto &s = p.slots.at(slot);
    if (!command)
        throw std::invalid_argument("null atlas upload command buffer");
    check(vkGetFenceStatus(p.device, s.fence), "atlas upload requires completed slot");
    if (!s.pending)
        return;
    auto &image = *s.texture;
    auto barrier = info<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    barrier.image = image.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.oldLayout =
        image.initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = image.initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(command,
                         image.initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                           : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {std::uint32_t(image.width), std::uint32_t(image.height), 1};
    vkCmdCopyBufferToImage(command, s.staging.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);
    image.initialized = true;
    s.pending = false;
    ++p.atlasUploads;
    p.uploadBytes += VkDeviceSize(image.width) * image.height * 4;
}
void Renderer::record(VkCommandBuffer command, std::size_t slot, VkExtent2D extent) {
    auto &p = *impl_;
    auto &s = p.slots.at(slot);
    if (!command || !extent.width || !extent.height)
        throw std::invalid_argument("invalid UI command buffer or extent");
    check(vkGetFenceStatus(p.device, s.fence), "UI slot must be complete before recording");
    if (s.pending)
        throw std::logic_error("record atlas transfers before rendering");
    if (!s.count)
        return;
    VkViewport viewport{0, 0, float(extent.width), float(extent.height), 0, 1};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &scissor);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout, 0, 1, &s.descriptor, 0,
                            nullptr);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command, 0, 1, &s.vertices.buffer, &offset);
    vkCmdBindIndexBuffer(command, s.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    const float size[]{float(extent.width), float(extent.height), p.srgb ? 1.f : 0.f, 0.f};
    vkCmdPushConstants(command, p.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(size), size);
    vkCmdDrawIndexed(command, s.count, 1, 0, 0, 0);
}
} // namespace voxelui::vulkan
