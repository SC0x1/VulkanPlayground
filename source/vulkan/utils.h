#pragma once

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <stdexcept>
#include <functional>
#include <optional>
#include <chrono>
#include <thread>

#include "vulkan/config.h"

#define vkBEGIN_NAMESPACE namespace Vk {
#define vkEND_NAMESPACE }

#define VK_FLAGS_NONE 0

#define DEFAULT_FENCE_TIMEOUT 100000000000

#define VK_CHECK(f) { VkResult _result_ = (f); \
    Vk::Utils::CheckVkResult(_result_, __FILE__, __LINE__);     \
 }

#define VK_CHECK_RET(f) { VkResult _result_ = (f);          \
    Vk::Utils::CheckVkResult(_result_, __FILE__, __LINE__); \
    return _result_;                                        \
}

vkBEGIN_NAMESPACE

struct VulkanInstance final
{
    VkInstance instance;
    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT messenger;
    VkDebugReportCallbackEXT reportCallback;
};

struct VulkanRenderDevice final
{
    uint32_t framebufferWidth;
    uint32_t framebufferHeight;

    VkDevice device;
    VkQueue graphicsQueue;
    VkPhysicalDevice physicalDevice;

    uint32_t graphicsFamily;

    VkSwapchainKHR swapchain;
    VkSemaphore semaphore;
    VkSemaphore renderSemaphore;

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // For chapter5/6etc (compute shaders)

    // Were we initialized with compute capabilities
    bool useCompute = false;

    // [may coincide with graphicsFamily]
    uint32_t computeFamily;
    VkQueue computeQueue;

    // a list of all queues (for shared buffer allocation)
    std::vector<uint32_t> deviceQueueIndices;
    std::vector<VkQueue> deviceQueues;

    VkCommandBuffer computeCommandBuffer;
    VkCommandPool computeCommandPool;
};

struct QueueFamilyIndices final
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool IsComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct ShaderModule final
{
    std::vector<uint32_t> SPIRV;
    VkShaderModule shaderModule = nullptr;
};

struct SyncObject final
{
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence fence;
};

namespace Utils
{
    inline const char* VKResultToString(VkResult result)
    {
        switch (result)
        {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
        case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
        case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
        case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
        case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
        case VK_PIPELINE_COMPILE_REQUIRED_EXT: return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
        }
        assert(false);
        return nullptr;
    }

    inline void CheckVkResult(VkResult result)
    {
        if (result != VK_SUCCESS)
        {
            printf("VkResult is '%s'", Utils::VKResultToString(result));

            if (result == VK_ERROR_DEVICE_LOST)
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(3s);
            }
            assert(result == VK_SUCCESS);
        }
    }

    inline void CheckVkResult(VkResult result, const char* fileName, int lineNumber)
    {
        if (result != VK_SUCCESS)
        {
            printf("VkResult is '%s' in %s:%i", Utils::VKResultToString(result), fileName, lineNumber);

            if (result == VK_ERROR_DEVICE_LOST)
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(3s);
            }
            assert(result == VK_SUCCESS);
        }
    }

    uint32_t FindQueueFamilies(VkPhysicalDevice device, VkQueueFlags desiredFlags);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    uint32_t FindMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    inline bool HasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory, void* data = nullptr);

    void CreateImage2D(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    VkImageView CreateImageView2D(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

    VkResult CreateShaderModule(VkDevice device, const std::vector<char>& code, VkShaderModule& vkShaderModule);
    VkResult CreateShaderModule(VkDevice device, const uint32_t* code, uint32_t codeSize, VkShaderModule& vkShaderModule);

    VkCommandBuffer BeginSingleTimeCommands(VulkanRenderDevice& vkDev);
    void EndSingleTimeCommands(VulkanRenderDevice& vkDev, VkCommandBuffer commandBuffer);

    // Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
    void SetImageLayout(
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // Uses a fixed sub resource layout with first mip level and layer
    void SetImageLayout(
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkImageAspectFlags aspectMask,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

namespace Init
{
    inline VkPushConstantRange PushConstantRange(VkShaderStageFlags stageFlags, uint32_t size, uint32_t offset)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = stageFlags;
        pushConstantRange.offset = offset;
        pushConstantRange.size = size;
        return pushConstantRange;
    }

    //////////////////////////////////////////////////////////////////////////
    // Pipeline Helpers
    inline VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(const VkDescriptorSetLayout* pSetLayouts, uint32_t setLayoutCount = 1)
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
        pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
        return pipelineLayoutCreateInfo;
    }

    inline VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology, VkPipelineInputAssemblyStateCreateFlags flags, VkBool32 primitiveRestartEnable)
    {
        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
        pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        pipelineInputAssemblyStateCreateInfo.flags = flags;
        pipelineInputAssemblyStateCreateInfo.topology = topology;
        pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
        return pipelineInputAssemblyStateCreateInfo;
    }

    inline VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(
        VkPolygonMode polygonMode,
        VkCullModeFlags cullMode,
        VkFrontFace frontFace,
        VkPipelineRasterizationStateCreateFlags flags = 0)
    {
        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
        pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pipelineRasterizationStateCreateInfo.flags = flags;
        pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
        pipelineRasterizationStateCreateInfo.cullMode = cullMode;
        pipelineRasterizationStateCreateInfo.frontFace = frontFace;
        pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
        return pipelineRasterizationStateCreateInfo;
    }

    inline VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(
        uint32_t attachmentCount,
        const VkPipelineColorBlendAttachmentState* pAttachments)
    {
        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
        pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
        pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
        return pipelineColorBlendStateCreateInfo;
    }

    inline VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(
        VkBool32 depthTestEnable,
        VkBool32 depthWriteEnable,
        VkCompareOp depthCompareOp)
    {
        VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
        pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
        pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
        pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
        pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
        return pipelineDepthStencilStateCreateInfo;
    }

    inline VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(
        uint32_t viewportCount,
        uint32_t scissorCount,
        VkPipelineViewportStateCreateFlags flags = 0)
    {
        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
        pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pipelineViewportStateCreateInfo.viewportCount = viewportCount;
        pipelineViewportStateCreateInfo.scissorCount = scissorCount;
        pipelineViewportStateCreateInfo.flags = flags;
        return pipelineViewportStateCreateInfo;
    }

    inline VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo(
        VkSampleCountFlagBits rasterizationSamples,
        VkPipelineMultisampleStateCreateFlags flags = 0)
    {
        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
        pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
        pipelineMultisampleStateCreateInfo.flags = flags;
        return pipelineMultisampleStateCreateInfo;
    }

    inline VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(
        const VkDynamicState* pDynamicStates,
        uint32_t dynamicStateCount,
        VkPipelineDynamicStateCreateFlags flags = 0)
    {
        VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
        pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
        pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
        pipelineDynamicStateCreateInfo.flags = flags;
        return pipelineDynamicStateCreateInfo;
    }

    inline VkGraphicsPipelineCreateInfo PipelineCreateInfo(
        VkPipelineLayout layout,
        VkRenderPass renderPass,
        VkPipelineCreateFlags flags = 0)
    {
        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.layout = layout;
        pipelineCreateInfo.renderPass = renderPass;
        pipelineCreateInfo.flags = flags;
        pipelineCreateInfo.basePipelineIndex = -1;
        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        return pipelineCreateInfo;
    }

    inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo()
    {
        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
        pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        return pipelineVertexInputStateCreateInfo;
    }

    inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo(
        VkVertexInputBindingDescription* vertexBindingDescriptions,
        uint32_t vertexBindingDescriptionCount,
        VkVertexInputAttributeDescription* vertexAttributeDescriptions,
        uint32_t vertexAttributeDescriptionCount
    )
    {
        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
        pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = vertexBindingDescriptionCount;
        pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexBindingDescriptions;
        pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = vertexAttributeDescriptionCount;
        pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions;
        return pipelineVertexInputStateCreateInfo;
    }

    //////////////////////////////////////////////////////////////////////////
    // Vertex Input
    inline VkVertexInputBindingDescription VertexInputBindingDescription(
        uint32_t binding,
        uint32_t stride,
        VkVertexInputRate inputRate)
    {
        VkVertexInputBindingDescription vInputBindDescription{};
        vInputBindDescription.binding = binding;
        vInputBindDescription.stride = stride;
        vInputBindDescription.inputRate = inputRate;
        return vInputBindDescription;
    }

    inline VkVertexInputAttributeDescription VertexInputAttributeDescription(
        uint32_t binding,
        uint32_t location,
        VkFormat format,
        uint32_t offset)
    {
        VkVertexInputAttributeDescription vInputAttribDescription{};
        vInputAttribDescription.location = location;
        vInputAttribDescription.binding = binding;
        vInputAttribDescription.format = format;
        vInputAttribDescription.offset = offset;
        return vInputAttribDescription;
    }

    inline VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(
        VkDescriptorType type,
        VkShaderStageFlags stageFlags,
        uint32_t binding,
        uint32_t descriptorCount = 1)
    {
        VkDescriptorSetLayoutBinding setLayoutBinding{};
        setLayoutBinding.descriptorType = type;
        setLayoutBinding.stageFlags = stageFlags;
        setLayoutBinding.binding = binding;
        setLayoutBinding.descriptorCount = descriptorCount;
        return setLayoutBinding;
    }

    inline VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
        const VkDescriptorSetLayoutBinding* pBindings,
        uint32_t bindingCount)
    {
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.pBindings = pBindings;
        descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
        return descriptorSetLayoutCreateInfo;
    }

    inline VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(
        VkDescriptorPool descriptorPool,
        const VkDescriptorSetLayout* pSetLayouts,
        uint32_t descriptorSetCount)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool;
        descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
        descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
        return descriptorSetAllocateInfo;
    }

    inline VkDescriptorImageInfo DescriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
    {
        VkDescriptorImageInfo descriptorImageInfo{};
        descriptorImageInfo.sampler = sampler;
        descriptorImageInfo.imageView = imageView;
        descriptorImageInfo.imageLayout = imageLayout;
        return descriptorImageInfo;
    }

    inline VkWriteDescriptorSet WriteDescriptorSet(
        VkDescriptorSet dstSet,
        VkDescriptorType type,
        uint32_t binding,
        VkDescriptorBufferInfo* bufferInfo,
        uint32_t descriptorCount = 1)
    {
        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = dstSet;
        writeDescriptorSet.descriptorType = type;
        writeDescriptorSet.dstBinding = binding;
        writeDescriptorSet.pBufferInfo = bufferInfo;
        writeDescriptorSet.descriptorCount = descriptorCount;
        return writeDescriptorSet;
    }

    inline VkWriteDescriptorSet WriteDescriptorSet(
        VkDescriptorSet dstSet,
        VkDescriptorType type,
        uint32_t binding,
        VkDescriptorImageInfo* imageInfo,
        uint32_t descriptorCount = 1)
    {
        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = dstSet;
        writeDescriptorSet.descriptorType = type;
        writeDescriptorSet.dstBinding = binding;
        writeDescriptorSet.pImageInfo = imageInfo;
        writeDescriptorSet.descriptorCount = descriptorCount;
        return writeDescriptorSet;
    }
}

vkEND_NAMESPACE

namespace Helpers
{
    // File Operations
    void ReadFile(const std::string& filename, std::vector<char>& buffer);
}