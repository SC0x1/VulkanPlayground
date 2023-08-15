#include "VKPlayground_PCH.h"

#include <vulkan/commandpool.h>

vkBEGIN_NAMESPACE

CommandPool::CommandPool()
{
}

void CommandPool::Create(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = queueFamilyIndex;
    createInfo.flags = createFlags;

    VK_CHECK(vkCreateCommandPool(device, &createInfo, nullptr, &m_VkCommandPool));
}

void CommandPool::Destroy(VkDevice device)
{
    if (m_VkCommandPool)
    {
        vkDestroyCommandPool(device, m_VkCommandPool, nullptr);
    }
}

vkEND_NAMESPACE
