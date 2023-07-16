#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "vulkan/vkutils.h"

typedef struct _SwapChainBuffers {
    VkImage image;
    VkImageView view;
} SwapChainBuffer;

class VulkanSwapChain
{
public:

private:
    VkInstance m_Instance;
    VkDevice m_Device;
    VkPhysicalDevice m_PhysicalDevice;
    VkSurfaceKHR m_Surface;

    std::vector<VkImage> m_SwapChainImages;
    std::vector<VkImageView> m_SwapChainImageViews;

    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

    VkSwapchainKHR m_SwapChain;
    VkFormat m_SwapChainImageFormat;
    VkExtent2D m_SwapChainExtent;

    VkColorSpaceKHR m_SurfaceColorSpace;
};