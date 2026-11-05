#include "renderer.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <voxelui/software.hpp>
using namespace voxelui;
using namespace voxelui::vulkan;
namespace {
void check(VkResult r) {
    if (r != VK_SUCCESS)
        throw std::runtime_error("Vulkan test error " + std::to_string(r));
}
template <class T> T info(VkStructureType type) {
    T t{};
    t.sType = type;
    return t;
}
unsigned errors{};
VKAPI_ATTR VkBool32 VKAPI_CALL message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                       VkDebugUtilsMessageTypeFlagsEXT,
                                       const VkDebugUtilsMessengerCallbackDataEXT *data, void *) {
    if (severity &
        (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
        ++errors;
        std::cerr << data->pMessage << '\n';
    }
    return VK_FALSE;
}
struct Context {
    VkInstance instance{};
    VkDebugUtilsMessengerEXT debug{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkQueue queue{};
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkFence fence{};
    Context() = default;
    ~Context() {
        if (device) {
            vkDeviceWaitIdle(device); // Test teardown only; backend never waits globally.
            if (fence)
                vkDestroyFence(device, fence, nullptr);
            if (pool)
                vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (debug)
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"))(instance, debug, nullptr);
        if (instance)
            vkDestroyInstance(instance, nullptr);
    }
    void initialize() {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.apiVersion = VK_API_VERSION_1_3;
        const char *layer = "VK_LAYER_KHRONOS_validation";
        const char *extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                    VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
        VkValidationFeatureEnableEXT enable = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
        auto validation = info<VkValidationFeaturesEXT>(VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
        validation.enabledValidationFeatureCount = 1;
        validation.pEnabledValidationFeatures = &enable;
        auto debugInfo =
            info<VkDebugUtilsMessengerCreateInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        debugInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = message;
        debugInfo.pNext = &validation;
        auto create = info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        create.pApplicationInfo = &app;
        create.enabledLayerCount = 1;
        create.ppEnabledLayerNames = &layer;
        create.enabledExtensionCount = 2;
        create.ppEnabledExtensionNames = extensions;
        create.pNext = &debugInfo;
        check(vkCreateInstance(&create, nullptr, &instance));
        debugInfo.pNext = nullptr;
        check(reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT"))(instance, &debugInfo, nullptr, &debug));
        std::uint32_t count{};
        check(vkEnumeratePhysicalDevices(instance, &count, nullptr));
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
        std::uint32_t family{};
        for (auto candidate : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            if (properties.apiVersion < VK_API_VERSION_1_3 ||
                properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
                continue;
            auto features =
                info<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
            auto query = info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            query.pNext = &features;
            vkGetPhysicalDeviceFeatures2(candidate, &query);
            if (!features.dynamicRendering)
                continue;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());
            for (std::uint32_t i = 0; i < count; ++i)
                if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    physical = candidate;
                    family = i;
                    break;
                }
            if (physical) {
                std::cout << "Hardware: " << properties.deviceName << '\n';
                break;
            }
        }
        if (!physical)
            throw std::runtime_error("hardware Vulkan 1.3 required (no software fallback)");
        float priority = 1;
        auto queueInfo = info<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        auto features =
            info<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
        features.dynamicRendering = VK_TRUE;
        auto deviceInfo = info<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        deviceInfo.pNext = &features;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        check(vkCreateDevice(physical, &deviceInfo, nullptr, &device));
        vkGetDeviceQueue(device, family, 0, &queue);
        auto poolInfo = info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        poolInfo.queueFamilyIndex = family;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        check(vkCreateCommandPool(device, &poolInfo, nullptr, &pool));
        auto allocate = info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(device, &allocate, &command));
        auto fenceInfo = info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(device, &fenceInfo, nullptr, &fence));
    }
    VkDeviceMemory memory(VkMemoryRequirements requirements, VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(physical, &properties);
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((requirements.memoryTypeBits & (1u << i)) &&
                (properties.memoryTypes[i].propertyFlags & flags) == flags) {
                auto allocation = info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
                allocation.allocationSize = requirements.size;
                allocation.memoryTypeIndex = i;
                VkDeviceMemory result{};
                check(vkAllocateMemory(device, &allocation, nullptr, &result));
                return result;
            }
        throw std::runtime_error("test memory type unavailable");
    }
};
struct Target {
    Context &c;
    VkImage image{};
    VkImageView view{};
    VkDeviceMemory imageMemory{}, readMemory{};
    VkBuffer read{};
    explicit Target(Context &context) : c(context) {}
    ~Target() {
        if (view)
            vkDestroyImageView(c.device, view, nullptr);
        if (image)
            vkDestroyImage(c.device, image, nullptr);
        if (read)
            vkDestroyBuffer(c.device, read, nullptr);
        if (imageMemory)
            vkFreeMemory(c.device, imageMemory, nullptr);
        if (readMemory)
            vkFreeMemory(c.device, readMemory, nullptr);
    }
    void initialize(VkExtent2D extent, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM) {
        auto imageInfo = info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        check(vkCreateImage(c.device, &imageInfo, nullptr, &image));
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(c.device, image, &requirements);
        imageMemory = c.memory(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkBindImageMemory(c.device, image, imageMemory, 0));
        auto viewInfo = info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(c.device, &viewInfo, nullptr, &view));
        auto bufferInfo = info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        bufferInfo.size = extent.width * extent.height * 4;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        check(vkCreateBuffer(c.device, &bufferInfo, nullptr, &read));
        vkGetBufferMemoryRequirements(c.device, read, &requirements);
        readMemory = c.memory(requirements,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkBindBufferMemory(c.device, read, readMemory, 0));
    }
};
void run(Context &c, std::array<int, std::size_t(ResourceKind::count)> &counts,
         VkFormat format = VK_FORMAT_R8G8B8A8_UNORM) {
    Renderer renderer(c.physical, c.device, format, std::span(&c.fence, 1),
                      {[](ResourceKind kind, bool allocated, void *context) noexcept {
                           auto &counters =
                               *static_cast<std::array<int, std::size_t(ResourceKind::count)> *>(context);
                           counters[std::size_t(kind)] += allocated ? 1 : -1;
                       },
                       &counts});
    Compiler compiler;
    Resources resources;
    resources.mountDirectory("test", VOXELUI_TEST_ASSETS);
    TextSystem text(resources);
    auto layout = text.layout("Ab Яя", "test:font.ttf", 12);
    if (!layout || layout->glyphs.size() < 4)
        throw std::runtime_error("font fixture did not produce Latin/Cyrillic glyphs");
    auto icon = std::make_shared<Image>(
        Image{2, 2, {255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 128, 255, 255, 0, 255}});
    auto vector = std::make_shared<VectorDocument>();
    vector->viewBox = {0, 0, 24, 24};
    VectorShape shape;
    shape.contours = {{{0, 0}, {24, 0}, {24, 24}, {0, 24}}, {{8, 8}, {16, 8}, {16, 16}, {8, 16}}};
    shape.gradient = {{0, 0}, {24, 24}, {200, 30, 40, 255}, {20, 220, 80, 128}, true};
    shape.stroke = {100, 120, 255, 128};
    shape.strokeWidth = 1;
    vector->shapes.push_back(shape);
    VectorShape concave;
    concave.contours = {{{1, 1}, {10, 1}, {10, 4}, {4, 4}, {4, 10}, {1, 10}}};
    concave.fill = {40, 80, 240, 180};
    concave.clip = Rect{2, 2, 16, 16};
    vector->shapes.push_back(concave);
    unsigned comparisons{};
    for (float scale : {1.f, 1.5f, 2.f})
        for (Vec2 size : {Vec2{64, 48}, Vec2{96, 64}, Vec2{128, 96}}) {
            DrawList list;
            list.logicalSize = size;
            list.framebufferScale = scale;
            VkExtent2D extent{std::uint32_t(size.x * scale), std::uint32_t(size.y * scale)};
            Target target(c);
            target.initialize(extent, format);
            for (int repeat = 0; repeat < 8; ++repeat) {
                list.commands = {
                    DrawRectangle{{0, 0, size.x, size.y}, {20, 40, 80, 255}, {}, 0, 0},
                    PushClip{{2, 2, 40, 32}},
                    PushTransform{{-1, 0, 0, 1, 44, 2}},
                    DrawRectangle{{0, 0, 40, 30}, {200, 40, 10, 128}, {20, 220, 10, 128}, 2, 0},
                    PushClip{{8, 4, 8, 12}},
                    DrawRectangle{{0, 0, 40, 30}, {10, 10, 240, 128}, {}, 0, 0},
                    PopClip{},
                    PopTransform{},
                    PopClip{},
                    DrawRectangle{{16, 20, 32, 24}, {60, 180, 30, 128}, {200, 40, 100, 128}, 2, 8},
                    DrawRectangle{{50, 20, 12, 16}, {60, 180, 30, 0}, {200, 40, 100, 128}, 3, 5},
                    PushTransform{{-1, 0, 0, 1, 62, 0}},
                    PushClip{{2, 32, 30, 16}},
                    DrawRectangle{{2, 32, 30, 16}, {80, 40, 180, 128}, {30, 240, 50, 128}, 2, 6},
                    PopClip{},
                    PopTransform{}};
                // Alternate empty and substantially larger meshes to exercise reuse/growth.
                if (repeat == 1)
                    list.commands.clear();
                if (repeat == 2)
                    for (int i = 0; i < 200; ++i)
                        list.commands.push_back(DrawRectangle{{48, 2, 4, 4}, {30, 40, 50, 255}, {}, 0, 0});
                if (repeat == 2) {
                    // New immutable resource identities exercise page growth and reload.
                    icon = std::make_shared<Image>();
                    icon->width = icon->height = 1024;
                    icon->rgba.resize(1024 * 1024 * 4, 180);
                }
                if (repeat != 1) {
                    list.commands.push_back(DrawImage{{4, 4, 16, 16}, icon, .8f});
                    list.commands.push_back(DrawText{{2, 30}, layout, {240, 220, 255, 220}});
                    list.commands.push_back(DrawVectorGeometry{{32, 4, 24, 24}, vector, .8f});
                }
                if (format == VK_FORMAT_R8G8B8A8_SRGB) {
                    list.commands = {
                        DrawRectangle{{0, 0, size.x, size.y}, {20, 40, 80, 255}, {}, 0, 0},
                        DrawRectangle{{4, 4, 32, 24}, {200, 40, 10, 255}, {20, 220, 10, 255}, 2, 6}};
                }
                auto mesh = compiler.compile(list);
                if (!mesh)
                    throw std::runtime_error(mesh.error().message);
                const auto allocations = renderer.statistics().bufferAllocations;
                renderer.upload(0, *mesh);
                if (repeat >= 3 && renderer.statistics().bufferAllocations != allocations)
                    throw std::runtime_error("steady workload reallocated UI buffers");
                check(vkResetCommandBuffer(c.command, 0));
                auto begin = info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
                check(vkBeginCommandBuffer(c.command, &begin));
                renderer.recordUploads(c.command, 0);
                auto barrier = info<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = target.image;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                vkCmdPipelineBarrier(c.command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                                     1, &barrier);
                auto attachment =
                    info<VkRenderingAttachmentInfo>(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
                attachment.imageView = target.view;
                attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.clearValue.color.float32[3] = 1;
                auto rendering = info<VkRenderingInfo>(VK_STRUCTURE_TYPE_RENDERING_INFO);
                rendering.renderArea = {{0, 0}, extent};
                rendering.layerCount = 1;
                rendering.colorAttachmentCount = 1;
                rendering.pColorAttachments = &attachment;
                vkCmdBeginRendering(c.command, &rendering);
                renderer.record(c.command, 0, extent);
                vkCmdEndRendering(c.command);
                barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(c.command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                VkBufferImageCopy copy{};
                copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy.imageExtent = {extent.width, extent.height, 1};
                vkCmdCopyImageToBuffer(c.command, target.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       target.read, 1, &copy);
                auto host = info<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
                host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(c.command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                                     1, &host, 0, nullptr, 0, nullptr);
                check(vkEndCommandBuffer(c.command));
                check(vkResetFences(c.device, 1, &c.fence));
                bool rejected = false;
                try {
                    renderer.upload(0, *mesh);
                } catch (const std::runtime_error &) {
                    rejected = true;
                }
                if (!rejected)
                    throw std::runtime_error("unsignaled slot accepted overwrite");
                auto submit = info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
                submit.commandBufferCount = 1;
                submit.pCommandBuffers = &c.command;
                check(vkQueueSubmit(c.queue, 1, &submit, c.fence));
                check(vkWaitForFences(c.device, 1, &c.fence, VK_TRUE, UINT64_MAX));
                SoftwareRenderer reference;
                reference.render(list);
                void *mapped{};
                check(vkMapMemory(c.device, target.readMemory, 0, reference.pixels().size(), 0, &mapped));
                unsigned worst{};
                auto *bytes = static_cast<const std::uint8_t *>(mapped);
                for (std::size_t i = 0; i < reference.pixels().size(); ++i) {
                    auto error = unsigned(std::abs(int(bytes[i]) - int(reference.pixels()[i])));
                    if (error > 2 && worst <= 2)
                        std::cerr << "Mismatch scale=" << scale << " extent=" << extent.width
                                  << " repeat=" << repeat << " xy=" << (i / 4) % extent.width << ","
                                  << (i / 4) / extent.width << " channel=" << i % 4
                                  << " actual=" << int(bytes[i]) << " expected=" << int(reference.pixels()[i])
                                  << '\n';
                    worst = std::max(worst, error);
                }
                if (const char *capture = std::getenv("VOXELUI_GPU_CAPTURE");
                    capture && scale == 2 && size.x == 128 && repeat == 0) {
                    std::ofstream file(capture, std::ios::binary);
                    file << "P6\n" << extent.width << ' ' << extent.height << "\n255\n";
                    for (std::size_t i = 0; i < reference.pixels().size(); i += 4)
                        file.write(reinterpret_cast<const char *>(bytes + i), 3);
                    if (!file)
                        throw std::runtime_error("cannot write GPU capture");
                }
                vkUnmapMemory(c.device, target.readMemory);
                if (worst > 2)
                    throw std::runtime_error("GPU reference mismatch, worst channel error=" +
                                             std::to_string(worst));
                ++comparisons;
            }
        }
    if (format == VK_FORMAT_R8G8B8A8_UNORM && compiler.tessellations() != 1)
        throw std::runtime_error("static SVG geometry was retessellated");
    std::cout << comparisons << " GPU comparisons, completion rejection, buffer growth/reuse passed\n";
}
} // namespace
int main(int argc, char **argv) try {
    const bool contextOnly = argc == 2 && std::string_view(argv[1]) == "--context-only";
    if (argc != 1 && !contextOnly)
        throw std::runtime_error("usage: voxelui-vulkan-gpu-tests [--context-only]");
    {
        Context context;
        context.initialize();
        std::array<int, std::size_t(ResourceKind::count)> counts{};
        if (!contextOnly) {
            run(context, counts);
            run(context, counts, VK_FORMAT_R8G8B8A8_SRGB);
        }
        for (auto count : counts)
            if (count != 0)
                throw std::runtime_error("native resources remain after Renderer destruction");
    }
    if (errors)
        throw std::runtime_error("validation warnings/errors: " + std::to_string(errors));
    std::cout << "Validation and synchronization validation: zero warnings/errors including teardown"
              << std::endl;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
