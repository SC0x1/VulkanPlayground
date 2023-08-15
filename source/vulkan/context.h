#pragma once

#include "vulkan/device.h"

vkBEGIN_NAMESPACE

class Context
{
public:
    Context();
    virtual ~Context();

    void Init();

    const Device& GetDevice() const { m_Device; }

    static VkInstance GetInstance() { return s_VulkanInstance; }

private:

    Device m_Device;

    inline static VkInstance s_VulkanInstance;

    VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;
    VkPipelineCache m_PipelineCache = nullptr;

    //VulkanSwapChain m_SwapChain;
};

vkEND_NAMESPACE