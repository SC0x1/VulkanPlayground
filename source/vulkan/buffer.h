#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE
    
struct Buffer
{
    VkDevice m_Device = VK_NULL_HANDLE;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_DeviceMemory = VK_NULL_HANDLE;
    VkDescriptorBufferInfo m_Descriptor;
    VkDeviceSize m_Size = 0;
    VkDeviceSize m_Alignment = 0;
    void* m_Mapped = nullptr;

    // Usage flags to be filled by external source at buffer creation
    VkBufferUsageFlags m_UsageFlags;
    // Memory property flags to be filled by external source at buffer creation
    VkMemoryPropertyFlags m_MemoryPropertyFlags;

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();
    VkResult bind(VkDeviceSize offset = 0);
    void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void copyTo(void* data, VkDeviceSize size);
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void destroy();
};

vkEND_NAMESPACE