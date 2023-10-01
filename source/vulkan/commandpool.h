#pragma once

#include "vulkan/utils.h"

vkBEGIN_NAMESPACE

class CommandPool
{
public:
    CommandPool();

    void Create(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = 0);
    void Destroy(VkDevice device);

    bool IsCreated() const;

    VkCommandPool GetVkCommandPool() const;

private:
    VkCommandPool m_VkCommandPool{ VK_NULL_HANDLE };
    VkCommandPoolCreateFlags m_Flags;
    uint32_t m_QueueFamilyIndex = 0;
    bool m_IsCreated = false;
};

inline bool CommandPool::IsCreated() const
{
    return m_IsCreated;
}

inline VkCommandPool CommandPool::GetVkCommandPool() const
{
    return m_VkCommandPool;
}

vkEND_NAMESPACE
