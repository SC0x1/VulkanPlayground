#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

class CommandPool
{
public:
    CommandPool();

    void Create(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = 0);
    void Destroy(VkDevice device);

    VkCommandPool GetVkCommandPool() const;

private:
    VkCommandPool m_VkCommandPool{ VK_NULL_HANDLE };
};

inline VkCommandPool CommandPool::GetVkCommandPool() const
{
    return m_VkCommandPool;
}

vkEND_NAMESPACE
