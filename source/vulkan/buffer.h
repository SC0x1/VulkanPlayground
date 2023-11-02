#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE
     
struct Buffer
{
    VkDevice m_Device = VK_NULL_HANDLE;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkDescriptorBufferInfo m_Descriptor;
    VkDeviceSize m_Size = 0;
    VkDeviceSize m_Alignment = 0;
    void* m_Mapped = nullptr;

    // Usage flags to be filled by external source at buffer creation
    VkBufferUsageFlags m_UsageFlags;
    // Memory property flags to be filled by external source at buffer creation
    VkMemoryPropertyFlags m_MemoryPropertyFlags;

    VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void UnMap();
    VkResult Bind(VkDeviceSize offset = 0);
    void SetupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void CopyTo(void* data, VkDeviceSize size);
    VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Destroy();
};

vkEND_NAMESPACE