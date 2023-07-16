#pragma once

#include "vulkan/vkdevice.h"

vkSTART_NAMESPACE

class VulkanContext
{
public:
    VulkanContext();
    virtual ~VulkanContext();

    void Init();

    const VulkanDevice& GetDevice() const { m_Device; }

    static VkInstance GetInstance() { return s_VulkanInstance; }

private:

    VulkanDevice m_Device;

    inline static VkInstance s_VulkanInstance;

    VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;
    VkPipelineCache m_PipelineCache = nullptr;

    //VulkanSwapChain m_SwapChain;
};

vkEND_NAMESPACE