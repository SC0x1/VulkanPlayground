#include "VKPlayground_PCH.h"

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

namespace Utils
{
    uint32_t FindQueueFamilies(VkPhysicalDevice device, VkQueueFlags desiredFlags)
    {
        // Assign index to queue families that could be found
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i != queueFamilies.size(); i++)
        {
            if (queueFamilies[i].queueCount > 0 &&
                queueFamilies[i].queueFlags & desiredFlags)
            {
                return i;
            }
        }

        return 0;
    }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices;
        // Assign index to queue families that could be found
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.IsComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    uint32_t FindMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        return 0xFFFFFFFF;
    }

    void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory, void* data)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, buffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Utils::FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
        
        // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also
        // need to enable the appropriate flag during allocation.
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo {};
        if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            allocInfo.pNext = &allocFlagsInfo;
        }

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, bufferMemory));
        
        // If a pointer to the buffer data has been passed, map the buffer and copy over the data
        if (data != nullptr)
        {
            void* mapped = nullptr;
            VK_CHECK(vkMapMemory(device, *bufferMemory, 0, size, 0, &mapped));

            memcpy(mapped, data, size);

            // If host coherency hasn't been requested, do a manual flush to make writes visible
            if ((properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            {
                VkMappedMemoryRange mappedRange{};
                mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                mappedRange.memory = *bufferMemory;
                mappedRange.offset = 0;
                mappedRange.size = size;
                vkFlushMappedMemoryRanges(device, 1, &mappedRange);
            }
            vkUnmapMemory(device, *bufferMemory);
        }

        vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
    }

    void CreateImage2D(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.usage = usage;
        /*
        The usage field has the same semantics as the one during buffer creation.
        The image is going to be used as destination for the buffer copy, so it should be set up as a transfer destination.
        We also want to be able to access the image from the shader to color our mesh, so the usage should include VK_IMAGE_USAGE_SAMPLED_BIT
        */
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        /*
        The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
        */

        imageInfo.samples = numSamples;
        imageInfo.flags = 0; // Optional

        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image));

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Utils::FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory));

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    VkResult CreateShaderModule(VkDevice device, const std::vector<char>& code, VkShaderModule& vkShaderModule)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VK_CHECK_RET(vkCreateShaderModule(device, &createInfo, nullptr, &vkShaderModule));
    }

    VkResult CreateShaderModule(VkDevice device, const uint32_t* code, uint32_t codeSize, VkShaderModule& vkShaderModule)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = codeSize;
        createInfo.pCode = code;

        VK_CHECK_RET(vkCreateShaderModule(device, &createInfo, nullptr, &vkShaderModule));
    }

    VkCommandBuffer BeginSingleTimeCommands(VulkanRenderDevice& vkDev)
    {
        VkCommandBuffer commandBuffer;

        const VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = vkDev.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        vkAllocateCommandBuffers(vkDev.device, &allocInfo, &commandBuffer);

        const VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void EndSingleTimeCommands(VulkanRenderDevice& vkDev, VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        const VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        vkQueueSubmit(vkDev.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vkDev.graphicsQueue);

        vkFreeCommandBuffers(vkDev.device, vkDev.commandPool, 1, &commandBuffer);
    }

    VkImageView CreateImageView2D(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
    {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = image;
        // viewType parameter allows you to treat images as 1D textures, 2D textures, 3D textures and cube maps
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = format;
        // The components field allows you to swizzle the color channels around.
        // For example, you can map all of the channels to the red channel for a
        // monochrome texture.
        // You can also map constant values of 0 and 1 to a channel
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        // subresourceRange field describes what the image's purpose is
        // and which part of the image should be accessed.
        // Our images will be used as color targets without any mipmapping levels
        // or multiple layers
        imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = mipLevels;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView))

        return imageView;
    }

    void SetImageLayout(
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask)
    {
        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch (oldImageLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            imageMemoryBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (newImageLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (imageMemoryBarrier.srcAccessMask == 0)
            {
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Put barrier inside setup command buffer
        vkCmdPipelineBarrier(
            cmdbuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);
    }

    void SetImageLayout(
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkImageAspectFlags aspectMask,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask)
    {
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = aspectMask;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        SetImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
    }

}

vkEND_NAMESPACE

namespace Helpers
{
    void ReadFile(const std::string& filename, std::vector<char>& buffer)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t)file.tellg();
        buffer.resize(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();
    }
};