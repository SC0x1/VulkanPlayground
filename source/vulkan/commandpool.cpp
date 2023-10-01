#include "VKPlayground_PCH.h"

#include <vulkan/commandpool.h>

vkBEGIN_NAMESPACE

CommandPool::CommandPool()
{
}

void CommandPool::Create(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
{
    m_QueueFamilyIndex = queueFamilyIndex;
    m_Flags = createFlags;

    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = queueFamilyIndex;
    createInfo.flags = m_Flags;

    VkResult result;
    result = vkCreateCommandPool(device, &createInfo, nullptr, &m_VkCommandPool);
    VK_CHECK(result);
    m_IsCreated = result == VK_SUCCESS;
}

void CommandPool::Destroy(VkDevice device)
{
    if (m_VkCommandPool)
    {
        vkDestroyCommandPool(device, m_VkCommandPool, nullptr);
        m_IsCreated = false;
    }
}

vkEND_NAMESPACE
