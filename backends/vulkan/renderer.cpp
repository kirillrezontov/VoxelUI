#include "renderer.hpp"
#include <cstring>
#include <bit>
#include <stdexcept>
#include <array>
#include <limits>

namespace voxelui::vulkan {
namespace {
void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
template<class T> T info(VkStructureType type) { T value{}; value.sType=type; return value; }
const std::uint32_t vertexCode[] =
#include "solid.vert.inc"
;
const std::uint32_t fragmentCode[] =
#include "solid.frag.inc"
;
struct Buffer {
    VkDevice device{};
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    void* mapped{};
    VkDeviceSize capacity{};
    Buffer() = default;
    Buffer(const Buffer&)=delete;
    Buffer& operator=(const Buffer&)=delete;
    Buffer(Buffer&& other) noexcept { swap(other); }
    Buffer& operator=(Buffer&& other) noexcept { Buffer old(std::move(other)); swap(old); return *this; }
    void swap(Buffer& other) noexcept {
        std::swap(device,other.device);std::swap(buffer,other.buffer);
        std::swap(memory,other.memory);std::swap(mapped,other.mapped);std::swap(capacity,other.capacity);
    }
    ~Buffer() {
        if(mapped) vkUnmapMemory(device,memory);
        if(buffer) vkDestroyBuffer(device,buffer,nullptr);
        if(memory) vkFreeMemory(device,memory,nullptr);
    }
};
}
struct Renderer::Impl {
    VkDevice device{};
    VkPhysicalDeviceMemoryProperties properties{};
    VkPipelineLayout layout{};
    VkPipeline pipeline{};
    std::uint64_t bufferAllocations{};
    struct Slot { VkFence fence{}; Buffer vertices,indices; std::uint32_t count{}; };
    std::vector<Slot> slots;
    ~Impl() {
        if(pipeline) vkDestroyPipeline(device,pipeline,nullptr);
        if(layout) vkDestroyPipelineLayout(device,layout,nullptr);
    }
    Buffer allocate(VkDeviceSize bytes, VkBufferUsageFlags usage) {
        Buffer result; result.device=device;
        // Geometric growth amortizes edits; bound allocations to geometry budgets.
        result.capacity=std::max<VkDeviceSize>(4096,std::bit_ceil(bytes));
        auto create=info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        create.size=result.capacity;create.usage=usage;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device,&create,nullptr,&result.buffer),"create UI buffer");
        VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(device,result.buffer,&requirements);
        std::uint32_t type=properties.memoryTypeCount;
        for(std::uint32_t i=0;i<properties.memoryTypeCount;++i)
            if((requirements.memoryTypeBits & (1u<<i)) &&
               (properties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {type=i;break;}
        if(type==properties.memoryTypeCount) throw std::runtime_error("UI requires host-visible coherent buffer memory");
        auto alloc=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize=requirements.size;alloc.memoryTypeIndex=type;
        check(vkAllocateMemory(device,&alloc,nullptr,&result.memory),"allocate UI buffer");
        check(vkBindBufferMemory(device,result.buffer,result.memory,0),"bind UI buffer");
        check(vkMapMemory(device,result.memory,0,result.capacity,0,&result.mapped),"map UI buffer");
        ++bufferAllocations;
        return result;
    }
};
Renderer::Renderer(VkPhysicalDevice physical,VkDevice device,VkFormat format,std::span<const VkFence> fences)
    : impl_(std::make_unique<Impl>()) {
    if(!physical || !device || fences.empty() || format==VK_FORMAT_UNDEFINED)
        throw std::invalid_argument("invalid UI Vulkan context");
    auto& p=*impl_;p.device=device;vkGetPhysicalDeviceMemoryProperties(physical,&p.properties);
    for(auto fence:fences) {
        if(!fence) throw std::invalid_argument("UI frame slot requires a completion fence");
        p.slots.emplace_back();p.slots.back().fence=fence;
    }
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT,0,2*sizeof(float)};
    auto layout=info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;
    check(vkCreatePipelineLayout(device,&layout,nullptr,&p.layout),"create UI pipeline layout");
    struct Shader {
        VkDevice device;VkShaderModule module{};
        ~Shader(){if(module) vkDestroyShaderModule(device,module,nullptr);}
    } vert{device},frag{device};
    auto shader=info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    shader.codeSize=sizeof(vertexCode);shader.pCode=vertexCode;
    check(vkCreateShaderModule(device,&shader,nullptr,&vert.module),"create UI vertex shader");
    shader.codeSize=sizeof(fragmentCode);shader.pCode=fragmentCode;
    check(vkCreateShaderModule(device,&shader,nullptr,&frag.module),"create UI fragment shader");
    std::array<VkPipelineShaderStageCreateInfo,2> stages{};
    for(auto& stage:stages) {stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stage.pName="main";}
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vert.module;
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=frag.module;
    VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription,7> attributes{{
        {0,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,position)},
        {1,0,VK_FORMAT_R8G8B8A8_UNORM,offsetof(Vertex,color)},
        {2,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,inverseX)},
        {3,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(Vertex,bounds)},
        {4,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,rounding)},
        {5,0,VK_FORMAT_R8G8B8A8_UNORM,offsetof(Vertex,border)},
        {6,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,inverseY)}}};
    static_assert(sizeof(Color)==4);
    auto input=info<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;
    input.vertexAttributeDescriptionCount=attributes.size();input.pVertexAttributeDescriptions=attributes.data();
    auto assembly=info<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    auto viewport=info<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    viewport.viewportCount=viewport.scissorCount=1;
    auto raster=info<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
    auto samples=info<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};blend.blendEnable=VK_TRUE;
    blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp=blend.alphaBlendOp=VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorWriteMask=0xf;
    auto blending=info<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    blending.attachmentCount=1;blending.pAttachments=&blend;
    std::array<VkDynamicState,2> dynamics{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
    auto dynamic=info<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    dynamic.dynamicStateCount=dynamics.size();dynamic.pDynamicStates=dynamics.data();
    auto rendering=info<VkPipelineRenderingCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
    rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;
    auto pipeline=info<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    pipeline.pNext=&rendering;pipeline.stageCount=stages.size();pipeline.pStages=stages.data();pipeline.layout=p.layout;
    pipeline.pVertexInputState=&input;pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&viewport;
    pipeline.pRasterizationState=&raster;pipeline.pMultisampleState=&samples;pipeline.pColorBlendState=&blending;
    pipeline.pDynamicState=&dynamic;
    check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipeline,nullptr,&p.pipeline),"create UI graphics pipeline");
}
Renderer::~Renderer()=default;
Renderer::Statistics Renderer::statistics() const {
    Statistics stats{impl_->bufferAllocations,0};
    for(const auto& slot:impl_->slots) stats.residentBufferBytes+=slot.vertices.capacity+slot.indices.capacity;
    return stats;
}
void Renderer::upload(std::size_t slot,const Mesh& mesh) {
    auto& p=*impl_;auto& s=p.slots.at(slot);
    check(vkGetFenceStatus(p.device,s.fence),"UI slot must be complete before upload");
    const GeometryLimits limits;
    if(mesh.vertices.size()>limits.vertices || mesh.indices.size()>limits.indices || mesh.indices.size()%3)
        throw std::invalid_argument("UI mesh exceeds limits or has incomplete triangles");
    for(const auto& vertex:mesh.vertices) {
        for(float value:{vertex.position.x,vertex.position.y,vertex.bounds.x,vertex.bounds.y,
                         vertex.bounds.w,vertex.bounds.h,vertex.rounding.x,vertex.rounding.y})
            if(!std::isfinite(value)) throw std::invalid_argument("nonfinite UI vertex");
        for(auto value:vertex.inverseX) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite UI inverse");
        for(auto value:vertex.inverseY) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite UI inverse");
    }
    for(auto index:mesh.indices) if(index>=mesh.vertices.size()) throw std::invalid_argument("UI index out of bounds");
    const VkDeviceSize vb=mesh.vertices.size()*sizeof(Vertex),ib=mesh.indices.size()*sizeof(std::uint32_t);
    // Allocate both replacements before changing the slot, preserving it on failure.
    Buffer vertices,indices;
    if(vb>s.vertices.capacity) vertices=p.allocate(vb,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if(ib>s.indices.capacity) indices=p.allocate(ib,VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    if(vertices.buffer) s.vertices=std::move(vertices);
    if(indices.buffer) s.indices=std::move(indices);
    if(vb) std::memcpy(s.vertices.mapped,mesh.vertices.data(),vb);
    if(ib) std::memcpy(s.indices.mapped,mesh.indices.data(),ib);
    s.count=static_cast<std::uint32_t>(mesh.indices.size());
}
void Renderer::record(VkCommandBuffer command,std::size_t slot,VkExtent2D extent) {
    auto& p=*impl_;auto& s=p.slots.at(slot);
    if(!command||!extent.width||!extent.height) throw std::invalid_argument("invalid UI command buffer or extent");
    check(vkGetFenceStatus(p.device,s.fence),"UI slot must be complete before recording");
    if(!s.count) return;
    VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1};
    VkRect2D scissor{{0,0},extent};
    vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,p.pipeline);
    const VkDeviceSize offset=0;
    vkCmdBindVertexBuffers(command,0,1,&s.vertices.buffer,&offset);
    vkCmdBindIndexBuffer(command,s.indices.buffer,0,VK_INDEX_TYPE_UINT32);
    const float size[]{float(extent.width),float(extent.height)};
    vkCmdPushConstants(command,p.layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(size),size);
    vkCmdDrawIndexed(command,s.count,1,0,0,0);
}
}
